#include "task.h"

#include <algorithm>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "clang/AST/AST.h"

#include "mmap.h"
#include "stream.h"

using std::binary_search;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::sort;
using std::string;
using std::to_string;
using std::unordered_map;
using std::vector;

using clang::CharSourceRange;
using clang::CXXMemberCallExpr;
using clang::DeclGroupRef;
using clang::DeclRefExpr;
using clang::DeclStmt;
using clang::DoStmt;
using clang::ElaboratedType;
using clang::Expr;
using clang::ExprWithCleanups;
using clang::ForStmt;
using clang::FunctionDecl;
using clang::FunctionProtoType;
using clang::LValueReferenceType;
using clang::MemberExpr;
using clang::PrintingPolicy;
using clang::RecordType;
using clang::SourceLocation;
using clang::Stmt;
using clang::TemplateSpecializationType;
using clang::VarDecl;
using clang::WhileStmt;

using llvm::dyn_cast;

namespace tlp {
namespace internal {

// Get a string representation of the function signature a stream operation.
std::string GetSignature(const CXXMemberCallExpr* call_expr) {
  auto target = call_expr->getDirectCallee();
  assert(target != nullptr);

  if (const auto instantiated = target->getTemplateInstantiationPattern()) {
    target = instantiated;
  }

  string signature{target->getQualifiedNameAsString()};

  signature += "(";

  for (auto param : target->parameters()) {
    PrintingPolicy policy{{}};
    policy.Bool = true;
    signature.append(param->getType().getAsString(policy));
    signature += ", ";
  }

  if (target->isVariadic()) {
    signature += ("...");
  } else if (target->getNumParams() > 0) {
    signature.resize(signature.size() - 2);
  }
  signature += ")";

  if (auto target_type =
          dyn_cast<FunctionProtoType>(target->getType().getTypePtr())) {
    if (target_type->isConst()) signature.append(" const");
    if (target_type->isVolatile()) signature.append(" volatile");
    if (target_type->isRestrict()) signature.append(" restrict");

    switch (target_type->getRefQualifier()) {
      case clang::RQ_LValue:
        signature.append(" &");
        break;
      case clang::RQ_RValue:
        signature.append(" &&");
        break;
      default:
        break;
    }
  }

  return signature;
}

// Given a Stmt, find the first tlp::task in its children.
const ExprWithCleanups* GetTlpTask(const Stmt* stmt) {
  for (auto child : stmt->children()) {
    if (auto expr = dyn_cast<ExprWithCleanups>(child)) {
      if (expr->getType().getAsString() == "struct tlp::task") {
        return expr;
      }
    }
  }
  return nullptr;
}

// Given a Stmt, find all tlp::task::invoke's via DFS and update invokes.
void GetTlpInvokes(const Stmt* stmt,
                   vector<const CXXMemberCallExpr*>& invokes) {
  for (auto child : stmt->children()) {
    GetTlpInvokes(child, invokes);
  }
  if (const auto invoke = dyn_cast<CXXMemberCallExpr>(stmt)) {
    if (invoke->getRecordDecl()->getQualifiedNameAsString() == "tlp::task" &&
        invoke->getMethodDecl()->getNameAsString() == "invoke") {
      invokes.push_back(invoke);
    }
  }
}

// Given a Stmt, return all tlp::task::invoke's via DFS.
vector<const CXXMemberCallExpr*> GetTlpInvokes(const Stmt* stmt) {
  vector<const CXXMemberCallExpr*> invokes;
  GetTlpInvokes(stmt, invokes);
  return invokes;
}

// Return all loops that do not contain other loops but do contain FIFO
// operations.
void GetInnermostLoops(const Stmt* stmt, vector<const Stmt*>& loops) {
  for (auto child : stmt->children()) {
    if (child != nullptr) {
      GetInnermostLoops(child, loops);
    }
  }
  if (RecursiveInnermostLoopsVisitor().IsInnermostLoop(stmt)) {
    loops.push_back(stmt);
  }
}
vector<const Stmt*> GetInnermostLoops(const Stmt* stmt) {
  vector<const Stmt*> loops;
  GetInnermostLoops(stmt, loops);
  return loops;
}

thread_local const FunctionDecl* Visitor::current_task{nullptr};

// Apply tlp s2s transformations on a function.
bool Visitor::VisitFunctionDecl(FunctionDecl* func) {
  if (func->hasBody() && func->isGlobal() &&
      context_.getSourceManager().isWrittenInMainFile(func->getBeginLoc())) {
    if (rewriters_.size() == 0) {
      funcs_.push_back(func);
    } else {
      if (rewriters_.count(func) > 0) {
        if (func == current_task) {
          if (auto task = GetTlpTask(func->getBody())) {
            ProcessUpperLevelTask(task, func);
          } else {
            ProcessLowerLevelTask(func);
          }
        } else {
          GetRewriter().RemoveText(func->getSourceRange());
        }
      }
    }
  }
  // Let the recursion continue.
  return true;
}

// Insert `#pragma HLS ...` after the token specified by loc.
bool Visitor::InsertHlsPragma(const SourceLocation& loc, const string& pragma,
                              const vector<pair<string, string>>& args) {
  string line{"\n#pragma HLS " + pragma};
  for (const auto& arg : args) {
    line += " " + arg.first;
    if (!arg.second.empty()) {
      line += " = " + arg.second;
    }
  }
  return GetRewriter().InsertTextAfterToken(loc, line);
}

// Apply tlp s2s transformations on a upper-level task.
void Visitor::ProcessUpperLevelTask(const ExprWithCleanups* task,
                                    const FunctionDecl* func) {
  const auto func_body = func->getBody();
  // TODO: implement qdma streams
  vector<StreamInfo> streams;
  for (const auto param : func->parameters()) {
    const string param_name = param->getNameAsString();
    if (IsMmap(param->getType())) {
      GetRewriter().ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          GetMmapElemType(param) + "*");
      InsertHlsPragma(func_body->getBeginLoc(), "interface",
                      {{"m_axi", ""},
                       {"port", param_name},
                       {"offset", "slave"},
                       {"bundle", param_name}});
    }
  }
  for (const auto param : func->parameters()) {
    InsertHlsPragma(func_body->getBeginLoc(), "interface",
                    {{"s_axilite", ""},
                     {"port", param->getNameAsString()},
                     {"bundle", "control"}});
  }
  InsertHlsPragma(
      func_body->getBeginLoc(), "interface",
      {{"s_axilite", ""}, {"port", "return"}, {"bundle", "control"}});
  GetRewriter().InsertTextAfterToken(func_body->getBeginLoc(), "\n");

  // Process stream declarations.
  for (const auto child : func_body->children()) {
    if (const auto decl_stmt = dyn_cast<DeclStmt>(child)) {
      if (const auto var_decl = dyn_cast<VarDecl>(*decl_stmt->decl_begin())) {
        if (auto decl = GetTlpStreamDecl(var_decl->getType())) {
          const auto args = decl->getTemplateArgs().asArray();
          const string elem_type{args[0].getAsType().getAsString()};
          const string fifo_depth{args[1].getAsIntegral().toString(10)};
          const string var_name{var_decl->getNameAsString()};
          GetRewriter().ReplaceText(
              var_decl->getSourceRange(),
              "hls::stream<tlp::data_t<" + elem_type + ">> " + var_name);
          InsertHlsPragma(child->getEndLoc(), "stream",
                          {{"variable", var_name}, {"depth", fifo_depth}});
        }
      }
    }
  }

  // Instanciate tasks.
  vector<const CXXMemberCallExpr*> invokes = GetTlpInvokes(task);
  string invokes_str{"#pragma HLS dataflow\n\n"};

  invokes_str += "#ifdef __SYNTHESIS__\n";

  for (auto invoke : invokes) {
    int step = -1;
    if (const auto method = dyn_cast<MemberExpr>(invoke->getCallee())) {
      if (method->getNumTemplateArgs() != 1) {
        ReportError(method->getMemberLoc(),
                    "exactly 1 template argument expected")
            .AddSourceRange(CharSourceRange::getCharRange(
                method->getMemberLoc(),
                method->getEndLoc().getLocWithOffset(1)));
      }
      step = stoi(GetRewriter().getRewrittenText(
          method->getTemplateArgs()[0].getSourceRange()));
    } else {
      ReportError(invoke->getBeginLoc(), "unexpected invocation: %0")
          .AddString(invoke->getStmtClassName());
    }
    invokes_str += "// step " + to_string(step) + "\n";
    for (unsigned i = 0; i < invoke->getNumArgs(); ++i) {
      const auto arg = invoke->getArg(i);
      if (const auto decl_ref = dyn_cast<DeclRefExpr>(arg)) {
        const string arg_name =
            GetRewriter().getRewrittenText(decl_ref->getSourceRange());
        if (i == 0) {
          invokes_str += arg_name + "(";
        } else {
          if (i > 1) {
            invokes_str += ", ";
          }
          invokes_str += arg_name;
        }
      } else {
        ReportError(arg->getBeginLoc(), "unexpected argument: %0")
            .AddString(arg->getStmtClassName());
      }
    }
    invokes_str += ");\n";
  }

  invokes_str += "#else // __SYNTHESIS__\n";
  invokes_str += "std::vector<std::thread> threads;\n";

  for (auto invoke : invokes) {
    int step = -1;
    if (const auto method = dyn_cast<MemberExpr>(invoke->getCallee())) {
      if (method->getNumTemplateArgs() != 1) {
        ReportError(method->getMemberLoc(),
                    "exactly 1 template argument expected")
            .AddSourceRange(CharSourceRange::getCharRange(
                method->getMemberLoc(),
                method->getEndLoc().getLocWithOffset(1)));
      }
      step = stoi(GetRewriter().getRewrittenText(
          method->getTemplateArgs()[0].getSourceRange()));
    } else {
      ReportError(invoke->getBeginLoc(), "unexpected invocation: %0")
          .AddString(invoke->getStmtClassName());
    }
    invokes_str += "// step " + to_string(step) + "\n";
    invokes_str += "threads.emplace_back(";
    for (unsigned i = 0; i < invoke->getNumArgs(); ++i) {
      const auto arg = invoke->getArg(i);
      if (const auto decl_ref = dyn_cast<DeclRefExpr>(arg)) {
        string arg_name =
            GetRewriter().getRewrittenText(decl_ref->getSourceRange());
        if (IsStreamInstance(arg->getType())) {
          arg_name = "std::ref(" + arg_name + ")";
        }
        invokes_str += arg_name + ", ";
      } else {
        ReportError(arg->getBeginLoc(), "unexpected argument: %0")
            .AddString(arg->getStmtClassName());
      }
    }
    invokes_str.pop_back();
    invokes_str.pop_back();
    invokes_str += ");\n";
  }

  invokes_str += "for (auto& t : threads) {t.join();}\n";
  invokes_str += "#endif // __SYNTHESIS__\n\n;";

  // task->getSourceRange() does not include the final semicolon so we remove
  // the ending newline and semicolon from invokes_str.
  invokes_str.pop_back();
  invokes_str.pop_back();
  GetRewriter().ReplaceText(task->getSourceRange(), invokes_str);

  // SDAccel only works with extern C kernels.
  GetRewriter().InsertText(func->getBeginLoc(), "extern \"C\" {\n\n");
  GetRewriter().InsertTextAfterToken(func->getEndLoc(),
                                     "\n\n}  // extern \"C\"\n");
}

// Apply tlp s2s transformations on a lower-level task.
void Visitor::ProcessLowerLevelTask(const FunctionDecl* func) {
  // Find interface streams.
  vector<StreamInfo> streams;
  vector<MmapInfo> mmaps;
  for (const auto param : func->parameters()) {
    if (IsStreamInterface(param)) {
      auto elem_type = GetStreamElemType(param);
      streams.emplace_back(param->getNameAsString(), elem_type);
      GetRewriter().ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "hls::stream<tlp::data_t<" + elem_type + ">>&");
    } else if (IsMmap(param)) {
      auto elem_type = GetMmapElemType(param);
      mmaps.emplace_back(param->getNameAsString(), elem_type);
      GetRewriter().ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          elem_type + "*");
    }
  }

  // Retrieve stream information.
  const auto func_body = func->getBody();
  GetStreamInfo(func_body, streams, context_.getDiagnostics());

  // Insert interface pragmas.
  for (const auto& mmap : mmaps) {
    InsertHlsPragma(func_body->getBeginLoc(), "interface",
                    {{"m_axi", {}},
                     {"port", mmap.name},
                     {"offset", "direct"},
                     {"bundle", mmap.name}});
  }

  // Before the original function body, insert data_pack pragmas.
  for (const auto& stream : streams) {
    InsertHlsPragma(func_body->getBeginLoc(), "data_pack",
                    {{"variable", stream.name}});
  }
  for (const auto& mmap : mmaps) {
    InsertHlsPragma(func_body->getBeginLoc(), "data_pack",
                    {{"variable", mmap.name}});
  }
  if (!streams.empty()) {
    GetRewriter().InsertTextAfterToken(func_body->getBeginLoc(), "\n\n");
  }
  // Insert _x_value and _x_valid variables for read streams.
  string read_states;
  for (const auto& stream : streams) {
    if (stream.is_consumer && stream.need_peeking) {
      read_states += "tlp::data_t<" + stream.type + "> " + stream.ValueVar() +
                     "{false, {}};\n";
      read_states += "bool " + stream.ValidVar() + "{false};\n\n";
    }
  }
  GetRewriter().InsertTextAfterToken(func_body->getBeginLoc(), read_states);

  // Rewrite stream operations via DFS.
  unordered_map<const CXXMemberCallExpr*, const StreamInfo*> stream_table;
  for (const auto& stream : streams) {
    for (auto call_expr : stream.call_exprs) {
      stream_table[call_expr] = &stream;
    }
  }
  RewriteStreams(func_body, stream_table);

  // Find loops that contain FIFOs operations but do not contain sub-loops;
  // These loops will be pipelined with II = 1.
  for (auto loop_stmt : GetInnermostLoops(func_body)) {
    auto stream_ops = GetTlpStreamOps(loop_stmt);
    sort(stream_ops.begin(), stream_ops.end());
    auto is_accessed = [&stream_ops](const StreamInfo& stream) -> bool {
      for (auto expr : stream.call_exprs) {
        if (binary_search(stream_ops.begin(), stream_ops.end(), expr)) {
          return true;
        }
      }
      return false;
    };

    // Blocking reads (destructive or nondestructive) cannot be used.
    auto& diagnostics_engine = context_.getDiagnostics();
    static const auto blocking_read_in_pipeline_error =
        diagnostics_engine.getCustomDiagID(
            clang::DiagnosticsEngine::Error,
            "blocking read cannot be used in an innermost loop");
    static const auto blocking_read_in_pipeline_note =
        diagnostics_engine.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                           "on tlp::stream '%0'");
    for (const auto& stream : streams) {
      if (is_accessed(stream) && stream.is_blocking && stream.is_consumer) {
        diagnostics_engine.Report(blocking_read_in_pipeline_error);
        for (auto expr : stream.call_exprs) {
          auto stream_op = GetStreamOp(expr);
          if ((stream_op & StreamOpEnum::kIsBlocking) &&
              (stream_op & StreamOpEnum::kIsConsumer) &&
              binary_search(stream_ops.begin(), stream_ops.end(), expr)) {
            auto diagnostics_builder = diagnostics_engine.Report(
                expr->getBeginLoc(), blocking_read_in_pipeline_note);
            diagnostics_builder.AddString(stream.name);
            diagnostics_builder.AddSourceRange(CharSourceRange::getCharRange(
                expr->getBeginLoc(), expr->getEndLoc().getLocWithOffset(1)));
          }
        }
      }
    }

    // Is peeking buffer needed for this loop.
    bool need_peeking = false;
    for (const auto& stream : streams) {
      if (is_accessed(stream) && stream.is_consumer && stream.need_peeking) {
        need_peeking = true;
        break;
      }
    }

    auto mmap_ops = GetTlpMmapOps(loop_stmt);
    if (!mmap_ops.empty() && !need_peeking) {
      continue;
    }

    // Find loop body.
    const Stmt* loop_body{nullptr};
    if (auto do_stmt = dyn_cast<DoStmt>(loop_stmt)) {
      loop_body = *do_stmt->getBody()->child_begin();
    } else if (auto for_stmt = dyn_cast<ForStmt>(loop_stmt)) {
      loop_body = *for_stmt->getBody()->child_begin();
    } else if (auto while_stmt = dyn_cast<WhileStmt>(loop_stmt)) {
      loop_body = *while_stmt->getBody()->child_begin();
    } else if (loop_stmt != nullptr) {
      ReportError(loop_stmt->getBeginLoc(), "unexpected loop: %0")
          .AddString(loop_stmt->getStmtClassName());
    } else {
      ReportError(func_body->getBeginLoc(), "null loop stmt");
    }

    string loop_preamble;
    for (const auto& stream : streams) {
      if (is_accessed(stream) && stream.is_consumer && stream.is_blocking) {
        if (!loop_preamble.empty()) {
          loop_preamble += " && ";
        }
        if (stream.need_peeking) {
          loop_preamble += stream.ValidVar();
        } else {
          loop_preamble += "!" + stream.name + ".empty()";
        }
      }
    }
    // Insert proceed only if there are blocking-read fifos.
    if (false && !loop_preamble.empty()) {
      GetRewriter().InsertText(loop_body->getBeginLoc(),
                               "if (/* " + StreamInfo::ProceedVar() + " = */" +
                                   loop_preamble + ") {\n",
                               /* InsertAfter= */ true,
                               /* indentNewLines= */ true);
      GetRewriter().InsertText(loop_stmt->getEndLoc(), "} else {\n",
                               /* InsertAfter= */ true,
                               /* indentNewLines= */ true);

      // If cannot proceed, still need to do state transition.
      string state_transition{};
      for (const auto& stream : streams) {
        if (is_accessed(stream) && stream.is_consumer && stream.need_peeking) {
          state_transition += "if (!" + stream.ValidVar() + ") {\n";
          state_transition += stream.ValidVar() + " = " + stream.name +
                              ".read_nb(" + stream.ValueVar() + ");\n";
          state_transition += "}\n";
        }
      }
      state_transition += "}  // if (" + StreamInfo::ProceedVar() + ")\n";
      GetRewriter().InsertText(loop_stmt->getEndLoc(), state_transition,
                               /* InsertAfter= */ true,
                               /* indentNewLines= */ true);
    }
  }
}  // namespace internal

void Visitor::RewriteStreams(
    const Stmt* stmt,
    unordered_map<const CXXMemberCallExpr*, const StreamInfo*> stream_table) {
  for (auto child : stmt->children()) {
    if (child != nullptr) {
      RewriteStreams(child, stream_table);
    }
  }
  if (const auto call_expr = dyn_cast<CXXMemberCallExpr>(stmt)) {
    auto stream = stream_table.find(call_expr);
    if (stream != stream_table.end()) {
      RewriteStream(stream->first, *stream->second);
    }
  }
}

// Given the CXXMemberCallExpr and the corresponding StreamInfo, rewrite the
// code.
void Visitor::RewriteStream(const CXXMemberCallExpr* call_expr,
                            const StreamInfo& stream) {
  string rewritten_text{};
  switch (GetStreamOp(call_expr)) {
    case StreamOpEnum::kTryEos: {
      rewritten_text = stream.name + ".try_eos(" +
                       GetRewriter().getRewrittenText(
                           call_expr->getArg(0)->getSourceRange()) +
                       ")";
      break;
    }
    case StreamOpEnum::kBlockingEos: {
      rewritten_text =
          "(" + stream.ValidVar() + " && " + stream.ValueVar() + ".eos)";
      rewritten_text = "tlp::eos_fifo(" + stream.name + ", " +
                       stream.ValueVar() + ", " + stream.ValidVar() + ")";
      break;
    }
    case StreamOpEnum::kBlockingPeek:
    case StreamOpEnum::kNonBlockingPeek: {
      rewritten_text = stream.ValueVar() + ".val";
      break;
    }
    case StreamOpEnum::kBlockingRead: {
      if (stream.need_peeking) {
        rewritten_text = "tlp::read_fifo(" + stream.name + ", " +
                         stream.ValueVar() + ", " + stream.ValidVar() + ")";
      } else {
        rewritten_text = stream.name + ".read().val";
      }
      break;
    }
    case StreamOpEnum::kNonBlockingRead: {
      rewritten_text = stream.name + ".read_nb(" +
                       GetRewriter().getRewrittenText(
                           call_expr->getArg(0)->getSourceRange()) +
                       ")";
      break;
    }
    case StreamOpEnum::kOpen: {
      if (stream.need_peeking) {
        rewritten_text = "tlp::read_fifo(" + stream.name + ", " +
                         stream.ValueVar() + ", " + stream.ValidVar() + ")";
      } else {
        rewritten_text = "assert(" + stream.name + ".read().eos == true);";
      }
      break;
    }
    case StreamOpEnum::kWrite: {
      rewritten_text = "tlp::write_fifo(" + stream.name + ", " + stream.type +
                       "(" +
                       GetRewriter().getRewrittenText(
                           call_expr->getArg(0)->getSourceRange()) +
                       "))";
      break;
    }
    case StreamOpEnum::kClose: {
      rewritten_text = "tlp::close_fifo(" + stream.name + ")";
      break;
    }
    default: {
      auto callee = dyn_cast<MemberExpr>(call_expr->getCallee());
      auto diagnostics_builder = ReportError(
          callee->getMemberLoc(), "'%0' has not yet been implemented");
      diagnostics_builder.AddSourceRange(CharSourceRange::getCharRange(
          callee->getMemberLoc(),
          callee->getMemberLoc().getLocWithOffset(
              callee->getMemberNameInfo().getAsString().size())));
      diagnostics_builder.AddString(GetSignature(call_expr));
      rewritten_text = "NOT_IMPLEMENTED";
    }
  }

  GetRewriter().ReplaceText(call_expr->getSourceRange(), rewritten_text);
}

}  // namespace internal
}  // namespace tlp