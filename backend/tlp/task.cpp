#include "task.h"

#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "clang/AST/AST.h"

using std::make_shared;
using std::pair;
using std::regex;
using std::regex_match;
using std::regex_replace;
using std::shared_ptr;
using std::smatch;
using std::string;
using std::to_string;
using std::unordered_map;
using std::vector;

using clang::ClassTemplateSpecializationDecl;
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
using clang::LValueReferenceType;
using clang::MemberExpr;
using clang::RecursiveASTVisitor;
using clang::SourceLocation;
using clang::Stmt;
using clang::TemplateSpecializationType;
using clang::VarDecl;
using clang::WhileStmt;

using llvm::dyn_cast;

const char kUtilFuncs[] = R"(namespace tlp{

template <typename T>
struct data_t {
  bool eos;
  T val;
};

template <typename T>
inline T read_fifo(data_t<T>& value, bool valid, bool* valid_ptr,
                    const T& def) {
  if (valid_ptr) {
    *valid_ptr = valid;
  }
  return valid ? value.val : def;
}

template <typename T>
inline T read_fifo(hls::stream<data_t<T>>& fifo, data_t<T>& value,
                    bool& valid) {
  T val = value.val;
  if (valid) {
    valid = fifo.read_nb(value);
  }
  return val;
}

template <typename T>
inline void write_fifo(hls::stream<data_t<T>>& fifo, const T& value) {
  fifo.write({false, value});
}

template <typename T>
inline void close_fifo(hls::stream<data_t<T>>& fifo) {
  fifo.write({true, {}});
}

}  // namespace tlp

)";

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

// Apply tlp s2s transformations on a function.
bool TlpVisitor::VisitFunctionDecl(FunctionDecl* func) {
  if (func->hasBody() && func->isGlobal()) {
    const auto loc = func->getBeginLoc();
    if (context_.getSourceManager().isWrittenInMainFile(loc)) {
      // Insert utility functions before the first function.
      if (first_func_) {
        first_func_ = false;
        rewriter_.InsertTextBefore(loc, kUtilFuncs);
      }
      if (auto task = GetTlpTask(func->getBody())) {
        ProcessUpperLevelTask(task, func);
      } else {
        ProcessLowerLevelTask(func);
      }
    }
  }
  // Let the recursion continue.
  return true;
}

// Insert `#pragma HLS ...` after the token specified by loc.
bool TlpVisitor::InsertHlsPragma(const SourceLocation& loc,
                                 const string& pragma,
                                 const vector<pair<string, string>>& args) {
  string line{"\n#pragma HLS " + pragma};
  for (const auto& arg : args) {
    line += " " + arg.first;
    if (!arg.second.empty()) {
      line += " = " + arg.second;
    }
  }
  return rewriter_.InsertTextAfterToken(loc, line);
}

// Apply tlp s2s transformations on a upper-level task.
void TlpVisitor::ProcessUpperLevelTask(const ExprWithCleanups* task,
                                       const FunctionDecl* func) {
  const auto func_body = func->getBody();
  // for arrays and scalars only name and type are meanful
  vector<StreamInfo> arrays;
  vector<StreamInfo> scalars;
  // for streams is_consumer and is_producer might be meanful as well
  // TODO: implement qdma streams
  vector<StreamInfo> streams;
  for (const auto param : func->parameters()) {
    // Discover tlp::stream via regex matching.
    const string param_type = param->getType().getAsString();
    const string param_name = param->getNameAsString();
    const regex kPatternToRemove{R"(( \*|const ))"};
    const string elem_type = regex_replace(param_type, kPatternToRemove, "");
    if (*param_type.rbegin() == '*') {
      arrays.emplace_back(param_name, elem_type);
      InsertHlsPragma(func_body->getBeginLoc(), "interface",
                      {{"m_axi", ""},
                       {"port", param_name},
                       {"offset", "slave"},
                       {"bundle", "gmem_" + param_name}});
    } else {
      scalars.emplace_back(param_name, elem_type);
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
  rewriter_.InsertTextAfterToken(func_body->getBeginLoc(), "\n");

  // Process stream declarations.
  for (const auto child : func_body->children()) {
    if (const auto decl_stmt = dyn_cast<DeclStmt>(child)) {
      if (const auto var_decl = dyn_cast<VarDecl>(*decl_stmt->decl_begin())) {
        const auto qual_type = var_decl->getType().getCanonicalType();
        if (const auto decl = dyn_cast<ClassTemplateSpecializationDecl>(
                qual_type.getTypePtr()->getAsRecordDecl())) {
          if (decl->getQualifiedNameAsString() == "tlp::stream") {
            const auto args = decl->getTemplateArgs().asArray();
            const string elem_type{args[0].getAsType().getAsString()};
            const string fifo_depth{args[1].getAsIntegral().toString(10)};
            const string var_name{var_decl->getNameAsString()};
            rewriter_.ReplaceText(
                var_decl->getSourceRange(),
                "hls::stream<tlp::data_t<" + elem_type + ">> " + var_name);
            InsertHlsPragma(child->getEndLoc(), "stream",
                            {{"variable", var_name}, {"depth", fifo_depth}});
          }
        }
      }
    }
  }

  // Instanciate tasks.
  vector<const CXXMemberCallExpr*> invokes = GetTlpInvokes(task);
  string invokes_str{"#pragma HLS dataflow\n\n"};

  for (auto invoke : invokes) {
    int step = -1;
    if (const auto method = dyn_cast<MemberExpr>(invoke->getCallee())) {
      if (method->getNumTemplateArgs() != 1) {
        llvm::errs() << "unexpected number of template args\n";
      }
      step = stoi(rewriter_.getRewrittenText(
          method->getTemplateArgs()[0].getSourceRange()));
    } else {
      llvm::errs() << "unexpected callee: " << invoke->getStmtClassName()
                   << "\n";
    }
    invokes_str += "// step " + to_string(step) + "\n";
    for (unsigned i = 0; i < invoke->getNumArgs(); ++i) {
      const auto arg = invoke->getArg(i);
      if (const auto decl_ref = dyn_cast<DeclRefExpr>(arg)) {
        const string arg_name =
            rewriter_.getRewrittenText(decl_ref->getSourceRange());
        if (i == 0) {
          invokes_str += arg_name + "(";
        } else {
          if (i > 1) {
            invokes_str += ", ";
          }
          invokes_str += arg_name;
        }
      } else {
        llvm::errs() << "unexpected Expr: " << decl_ref->getStmtClassName()
                     << "\n";
      }
    }
    invokes_str += ");\n";
  }
  // task->getSourceRange() does not include the final semicolon so we remove
  // the ending newline and semicolon from invokes_str.
  invokes_str.pop_back();
  invokes_str.pop_back();
  rewriter_.ReplaceText(task->getSourceRange(), invokes_str);

  // SDAccel only works with extern C kernels.
  rewriter_.InsertText(func->getBeginLoc(), "extern \"C\" {\n\n");
  rewriter_.InsertTextAfterToken(func->getEndLoc(), "\n\n}  // extern \"C\"\n");
}

// Apply tlp s2s transformations on a lower-level task.
void TlpVisitor::ProcessLowerLevelTask(const FunctionDecl* func) {
  // Find interface streams.
  vector<StreamInfo> streams;
  for (const auto param : func->parameters()) {
    // Discover tlp::stream via regex matching.
    const string param_type = param->getType().getAsString();
    const regex kTlpStreamPattern{R"(tlp\s*::\s*stream\s*<(.*)>\s*&)"};
    const string replaced_param_type = regex_replace(
        param_type, kTlpStreamPattern, "hls::stream<tlp::data_t<$1>>&");
    if (replaced_param_type != param_type) {
      const string elem_type =
          regex_replace(param_type, kTlpStreamPattern, "$1");
      streams.emplace_back(param->getNameAsString(), elem_type);
      // Regex matched.
      rewriter_.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          replaced_param_type);
    }
  }

  // Retrieve stream information.
  const auto func_body = func->getBody();
  GetStreamInfo(func_body, streams);

  // Before the original function body, insert data_pack pragmas.
  for (const auto& stream : streams) {
    InsertHlsPragma(func_body->getBeginLoc(), "data_pack",
                    {{"variable", stream.name}});
  }
  if (!streams.empty()) {
    rewriter_.InsertTextAfterToken(func_body->getBeginLoc(), "\n\n");
  }
  // Insert _x_value and _x_valid variables for read streams.
  string read_states;
  for (const auto& stream : streams) {
    if (stream.is_consumer) {
      read_states += "tlp::data_t<" + stream.type + "> " + stream.ValueVar() +
                     "{false, {}};\n";
      read_states += "bool " + stream.ValidVar() + "{false};\n\n";
    }
  }
  rewriter_.InsertTextAfterToken(func_body->getBeginLoc(), read_states);

  // Rewrite stream operations via DFS.
  unordered_map<const CXXMemberCallExpr*, const StreamInfo*> stream_table;
  for (const auto& stream : streams) {
    for (auto call_expr : stream.call_exprs) {
      stream_table[call_expr] = &stream;
    }
  }
  RewriteStreams(func_body, stream_table);

  // Find the main loop.
  // TODO: change to find all innermost loops.
  const Stmt* first_stmt{nullptr};
  const Stmt* loop_stmt{nullptr};
  for (auto stmt : func_body->children()) {
    if (first_stmt == nullptr) {
      first_stmt = stmt;
    }
    switch (stmt->getStmtClass()) {
      case Stmt::DoStmtClass:
      case Stmt::ForStmtClass:
      case Stmt::WhileStmtClass: {
        if (loop_stmt == nullptr) {
          loop_stmt = stmt;
        } else {
          llvm::errs() << "more than 1 loop found\n";
        }
        break;
      }
      default: {}
    }
  }

  if (first_stmt == nullptr) {
    llvm::errs() << "no stmt found\n";
    return;
  }
  if (loop_stmt == nullptr) {
    llvm::errs() << "no loop found\n";
    return;
  }

  // Move increment statement to the end of loop body for ForStmt.
  if (const auto for_stmt = dyn_cast<ForStmt>(loop_stmt)) {
    const string inc =
        rewriter_.getRewrittenText(for_stmt->getInc()->getSourceRange());
    rewriter_.RemoveText(for_stmt->getInc()->getSourceRange());
    rewriter_.InsertText(for_stmt->getBody()->getEndLoc(), inc + ";\n");
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
    llvm::errs() << "unexpected loop stmt: ";
    loop_stmt->dumpColor();
    exit(EXIT_FAILURE);
  } else {
    llvm::errs() << "null loop stmt: ";
    exit(EXIT_FAILURE);
  }

  string loop_preamble;
  for (const auto& stream : streams) {
    if (stream.is_consumer && stream.is_blocking) {
      if (!loop_preamble.empty()) {
        loop_preamble += " && ";
      }
      loop_preamble += stream.ValidVar();
    }
  }
  // Insert proceed only if there are blocking-read fifos.
  if (!loop_preamble.empty()) {
    loop_preamble =
        "bool " + (StreamInfo::ProceedVar() + ("{" + loop_preamble)) + "};\n\n";
    rewriter_.InsertText(loop_body->getBeginLoc(), loop_preamble,
                         /* InsertAfter= */ true,
                         /* indentNewLines= */ true);
    rewriter_.InsertText(loop_body->getBeginLoc(),
                         string{"if ("} + StreamInfo::ProceedVar() + ") {\n",
                         /* InsertAfter= */ true,
                         /* indentNewLines= */ true);
    rewriter_.InsertText(loop_stmt->getEndLoc(), "} else {\n",
                         /* InsertAfter= */ true,
                         /* indentNewLines= */ true);

    // If cannot proceed, still need to do state transition.
    string state_transition{};
    for (const auto& stream : streams) {
      if (!stream.is_consumer) {
        continue;
      }
      state_transition += "if (!" + stream.ValidVar() + ") {\n";
      state_transition += stream.ValidVar() + " = " + stream.name +
                          ".read_nb(" + stream.ValueVar() + ");\n";
      state_transition += "}\n";
    }
    state_transition += "}  // if (" + StreamInfo::ProceedVar() + ")\n";
    rewriter_.InsertText(loop_stmt->getEndLoc(), state_transition,
                         /* InsertAfter= */ true,
                         /* indentNewLines= */ true);
  }
}

void TlpVisitor::RewriteStreams(
    const Stmt* stmt,
    unordered_map<const CXXMemberCallExpr*, const StreamInfo*> stream_table,
    shared_ptr<unordered_map<const Stmt*, bool>> visited) {
  if (visited == nullptr) {
    visited = make_shared<decltype(visited)::element_type>();
  }
  if ((*visited)[stmt]) {
    return;
  }
  (*visited)[stmt] = true;

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
void TlpVisitor::RewriteStream(const CXXMemberCallExpr* call_expr,
                               const StreamInfo& stream) {
  assert(stream.name == call_expr->getRecordDecl()->getNameAsString());

  string rewritten_text{};
  switch (GetStreamOp(call_expr)) {
    case StreamOpEnum::kTestEos: {
      rewritten_text =
          "(" + stream.ValidVar() + " && " + stream.ValueVar() + ".eos)";
      break;
    }
    case StreamOpEnum::kBlockingPeek:
    case StreamOpEnum::kNonBlockingPeek: {
      rewritten_text = stream.ValueVar() + ".val";
      break;
    }
    case StreamOpEnum::kBlockingRead: {
      rewritten_text = "tlp::read_fifo(" + stream.name + ", " +
                       stream.ValueVar() + ", " + stream.ValidVar() + ")";
      break;
    }
    case StreamOpEnum::kWrite: {
      rewritten_text =
          "tlp::write_fifo(" + stream.name + ", " + stream.type + "{" +
          rewriter_.getRewrittenText(call_expr->getArg(0)->getSourceRange()) +
          "})";
      break;
    }
    case StreamOpEnum::kClose: {
      rewritten_text = "tlp::close_fifo(" + stream.name + ")";
      break;
    }
    default: { rewritten_text = "NOT_IMPLEMENTED"; }
  }

  rewriter_.ReplaceText(call_expr->getSourceRange(), rewritten_text);
}
