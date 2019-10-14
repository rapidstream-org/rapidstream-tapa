#include "task.h"

#include <algorithm>
#include <array>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/Lex/Lexer.h"

#include "nlohmann/json.hpp"

#include "mmap.h"
#include "stream.h"

using std::array;
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
using clang::CXXMethodDecl;
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
using clang::Lexer;
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

using nlohmann::json;

namespace tlp {
namespace internal {

extern const string* top_name;

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
    if (IsMmap(param)) {
      GetRewriter().ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "\n#ifdef COSIM\n" + GetMmapElemType(param) +
              "*\n#else  // COSIM\nuint64_t\n#endif  // COSIM\n");
    }
  }

  string replaced_body{"{\n"};
  for (const auto param : func->parameters()) {
    replaced_body +=
        "#pragma HLS interface s_axilite port = " + param->getNameAsString() +
        " bundle = control\n";
  }
  replaced_body +=
      "#pragma HLS interface s_axilite port = return bundle = control\n\n";
  for (const auto param : func->parameters()) {
    if (IsMmap(param)) {
      replaced_body +=
          "#pragma HLS data_pack variable = " + param->getNameAsString() + "\n";
    }
  }

  replaced_body += "\n#ifdef COSIM\n";
  for (const auto param : func->parameters()) {
    const bool is_mmap = IsMmap(param);
    replaced_body += "{ auto val = ";
    if (is_mmap) {
      replaced_body += "*";
    }
    replaced_body += "reinterpret_cast<volatile ";
    if (IsStreamInterface(param)) {
      // TODO (maybe?)
    } else {
      auto elem_type = param->getType();
      if (is_mmap) {
        elem_type = elem_type->getAs<clang::TemplateSpecializationType>()
                        ->getArg(0)
                        .getAsType();
      }
      const bool is_const = elem_type.isConstQualified();
      const auto param_name = param->getNameAsString();
      if (is_const) {
        replaced_body += "const ";
      }
      replaced_body += "uint8_t";
      replaced_body += is_mmap ? "*" : "&";
      replaced_body += ">(" + param_name + "); }\n";
      if (is_mmap && !is_const) {
        replaced_body += param_name + "[1] = {};\n";
      }
    }
  }
  replaced_body += "#endif  // COSIM\n";

  replaced_body += "}\n";

  // We need a empty shell.
  GetRewriter().ReplaceText(func_body->getSourceRange(), replaced_body);

  // Obtain the connection schema from the task.
  // metadata: {tasks, fifos}
  // tasks: {task_name: [{step, {args: var_name: {var_type, port_name}}}]}
  // fifos: {fifo_name: {depth, produced_by, consumed_by}}
  auto& metadata = GetMetadata();

  if (*top_name == func->getNameAsString()) {
    for (const auto param : func->parameters()) {
      const auto param_name = param->getNameAsString();
      if (IsMmap(param)) {
        metadata["ports"].push_back(
            {{"name", param_name},
             {"cat", "mmap"},
             {"width",
              context_
                  .getTypeInfo(param->getType()
                                   ->getAs<clang::TemplateSpecializationType>()
                                   ->getArg(0)
                                   .getAsType())
                  .Width},
             {"type", GetMmapElemType(param) + "*"}});
      } else if (IsStreamInterface(param)) {
        // TODO
      } else {
        metadata["ports"].push_back(
            {{"name", param_name},
             {"cat", "scalar"},
             {"width", context_.getTypeInfo(param->getType()).Width},
             {"type", param->getType().getAsString()}});
      }
    }
  }

  // Process stream declarations.
  for (const auto child : func_body->children()) {
    if (const auto decl_stmt = dyn_cast<DeclStmt>(child)) {
      if (const auto var_decl = dyn_cast<VarDecl>(*decl_stmt->decl_begin())) {
        if (auto decl = GetTlpStreamDecl(var_decl->getType())) {
          const auto args = decl->getTemplateArgs().asArray();
          const string elem_type = GetTemplateArgName(args[0]);
          const uint64_t fifo_depth{*args[1].getAsIntegral().getRawData()};
          const string var_name{var_decl->getNameAsString()};
          metadata["fifos"][var_name]["depth"] = fifo_depth;
        }
      }
    }
  }

  // Instanciate tasks.
  vector<const CXXMemberCallExpr*> invokes = GetTlpInvokes(task);

  for (auto invoke : invokes) {
    int step = -1;
    if (const auto method = dyn_cast<CXXMethodDecl>(invoke->getCalleeDecl())) {
      step =
          *reinterpret_cast<const int*>(method->getTemplateSpecializationArgs()
                                            ->get(0)
                                            .getAsIntegral()
                                            .getRawData());
    } else {
      ReportError(invoke->getCallee()->getBeginLoc(),
                  "unexpected invocation: %0")
          .AddString(invoke->getStmtClassName());
    }
    const FunctionDecl* task = nullptr;
    string task_name;
    for (unsigned i = 0; i < invoke->getNumArgs(); ++i) {
      const auto arg = invoke->getArg(i);
      if (const auto decl_ref = dyn_cast<DeclRefExpr>(arg)) {
        const auto arg_name = decl_ref->getNameInfo().getName().getAsString();
        if (i == 0) {
          task_name = arg_name;
          metadata["tasks"][task_name].push_back({{"step", step}});
          task = decl_ref->getDecl()->getAsFunction();
        } else {
          auto param = task->getParamDecl(i - 1);
          string param_cat;
          if (IsMmap(param)) {
            param_cat = "mmap";
          } else if (IsInputStream(param)) {
            param_cat = "istream";
            if (metadata["fifos"][arg_name].contains("consumed_by")) {
              auto diagnostics_builder =
                  ReportError(param->getLocation(),
                              "tlp::stream '%0' consumed more than once");
              diagnostics_builder.AddString(arg_name);
              diagnostics_builder.AddSourceRange(
                  CharSourceRange::getCharRange(param->getSourceRange()));
            }
            metadata["fifos"][arg_name]["consumed_by"] = {
                task_name, metadata["tasks"][task_name].size() - 1};
          } else if (IsOutputStream(param)) {
            param_cat = "ostream";
            if (metadata["fifos"][arg_name].contains("produced_by")) {
              auto diagnostics_builder =
                  ReportError(param->getLocation(),
                              "tlp::stream '%0' produced more than once");
              diagnostics_builder.AddString(arg_name);
              diagnostics_builder.AddSourceRange(
                  CharSourceRange::getCharRange(param->getSourceRange()));
            }
            metadata["fifos"][arg_name]["produced_by"] = {
                task_name, metadata["tasks"][task_name].size() - 1};
          } else {
            param_cat = "scalar";
          }
          (*metadata["tasks"][task_name].rbegin())["args"][arg_name] = {
              {"cat", param_cat}, {"port", param->getNameAsString()}};
        }
      } else {
        auto diagnostics_builder =
            ReportError(arg->getBeginLoc(), "unexpected argument: %0");
        diagnostics_builder.AddString(arg->getStmtClassName());
        diagnostics_builder.AddSourceRange(
            CharSourceRange::getCharRange(arg->getSourceRange()));
      }
    }
  }

  /*
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
  }
  */

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

  string new_params{};
  for (auto stream : streams) {
    if (stream.need_peeking) {
      new_params += ", hls::stream<tlp::data_t<" + stream.type + ">>& " +
                    stream.PeekVar();
    }
  }
  if (!new_params.empty()) {
    auto end_loc = func->getParamDecl(func->getNumParams() - 1)->getEndLoc();
    GetRewriter().InsertText(GetEndOfLoc(end_loc), new_params);
  }

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
    if (stream.need_peeking) {
      InsertHlsPragma(func_body->getBeginLoc(), "data_pack",
                      {{"variable", stream.PeekVar()}});
    }
  }
  for (const auto& mmap : mmaps) {
    InsertHlsPragma(func_body->getBeginLoc(), "data_pack",
                    {{"variable", mmap.name}});
  }
  if (!streams.empty()) {
    GetRewriter().InsertTextAfterToken(func_body->getBeginLoc(), "\n\n");
  }

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
            "blocking read cannot be used in an innermost loop with peeking");
    static const auto blocking_read_in_pipeline_note =
        diagnostics_engine.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                           "on tlp::stream '%0'");
    for (const auto& stream : streams) {
      decltype(stream.call_exprs) call_exprs;
      for (auto expr : stream.call_exprs) {
        auto stream_op = GetStreamOp(expr);
        if ((stream_op & StreamOpEnum::kIsBlocking) &&
            (stream_op & StreamOpEnum::kIsConsumer) &&
            binary_search(stream_ops.begin(), stream_ops.end(), expr)) {
          call_exprs.push_back(expr);
        }
      }

      if (!call_exprs.empty() && stream.need_peeking) {
        diagnostics_engine.Report(blocking_read_in_pipeline_error);
        for (auto expr : call_exprs) {
          auto diagnostics_builder = diagnostics_engine.Report(
              expr->getBeginLoc(), blocking_read_in_pipeline_note);
          diagnostics_builder.AddString(stream.name);
          diagnostics_builder.AddSourceRange(CharSourceRange::getCharRange(
              expr->getBeginLoc(), expr->getEndLoc().getLocWithOffset(1)));
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
  vector<string> args;
  for (auto arg : call_expr->arguments()) {
    args.push_back(GetRewriter().getRewrittenText(arg->getSourceRange()));
  }
  switch (GetStreamOp(call_expr)) {
    case StreamOpEnum::kTryEos: {
      rewritten_text = "tlp::try_eos(" + stream.name + ", " + stream.PeekVar() +
                       ", " + args[0] + ")";
      break;
    }
    case StreamOpEnum::kBlockingEos: {
      rewritten_text = "tlp::eos(" + stream.PeekVar() + ")";
      break;
    }
    case StreamOpEnum::kBlockingPeek: {
      rewritten_text = "tlp::peek(" + stream.PeekVar() + ")";
      break;
    }
    case StreamOpEnum::kNonBlockingPeek: {
      rewritten_text = "tlp::peek(" + stream.name + ", " + stream.PeekVar() +
                       ", " + args[0] + ")";
      break;
    }
    case StreamOpEnum::kBlockingRead: {
      rewritten_text = "tlp::read(" + stream.name + ")";
      break;
    }
    case StreamOpEnum::kNonBlockingRead: {
      rewritten_text = "tlp::read(" + stream.name + ", " + args[0] + ")";
      break;
    }
    case StreamOpEnum::kOpen: {
      // somehow using tlp::open crashes Vivado HLS
      rewritten_text = "assert(" + stream.name + ".read().eos)";
      break;
    }
    case StreamOpEnum::kWrite: {
      rewritten_text = "tlp::write(" + stream.name + ", " + stream.type + "(" +
                       args[0] + "))";
      break;
    }
    case StreamOpEnum::kClose: {
      rewritten_text = "tlp::close(" + stream.name + ")";
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

SourceLocation Visitor::GetEndOfLoc(SourceLocation loc) {
  return loc.getLocWithOffset(Lexer::MeasureTokenLength(
      loc, GetRewriter().getSourceMgr(), GetRewriter().getLangOpts()));
}

}  // namespace internal
}  // namespace tlp