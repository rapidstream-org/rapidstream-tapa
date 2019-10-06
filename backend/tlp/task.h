#ifndef TLP_TASK_H_
#define TLP_TASK_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "stream.h"

namespace tlp {
namespace internal {

const clang::ExprWithCleanups* GetTlpTask(const clang::Stmt* func_body);

std::vector<const clang::CXXMemberCallExpr*> GetTlpInvokes(
    const clang::Stmt* task);

class Visitor : public clang::RecursiveASTVisitor<Visitor> {
 public:
  explicit Visitor(clang::ASTContext& context,
                   std::vector<const clang::FunctionDecl*>& funcs,
                   std::unordered_map<const clang::FunctionDecl*,
                                      clang::Rewriter>& rewriters)
      : context_{context}, funcs_{funcs}, rewriters_{rewriters} {}

  bool VisitFunctionDecl(clang::FunctionDecl* func);

  static thread_local const clang::FunctionDecl* current_task;

 private:
  clang::ASTContext& context_;
  std::vector<const clang::FunctionDecl*>& funcs_;
  std::unordered_map<const clang::FunctionDecl*, clang::Rewriter>& rewriters_;

  clang::Rewriter& GetRewriter() { return rewriters_[current_task]; }

  bool InsertHlsPragma(
      const clang::SourceLocation& loc, const std::string& pragma,
      const std::vector<std::pair<std::string, std::string>>& args);

  void ProcessUpperLevelTask(const clang::ExprWithCleanups* task,
                             const clang::FunctionDecl* func);

  void ProcessLowerLevelTask(const clang::FunctionDecl* func);

  void RewriteStreams(
      const clang::Stmt* stmt,
      std::unordered_map<const clang::CXXMemberCallExpr*, const StreamInfo*>
          stream_table);
  void RewriteStream(const clang::CXXMemberCallExpr* call_expr,
                     const StreamInfo& stream);

  // Use the given format_string to create a unique diagnostic id and report
  // error with it.
  template <unsigned N>
  clang::DiagnosticBuilder ReportError(clang::SourceLocation loc,
                                       const char (&format_string)[N]) {
    static const auto diagnostic_id = context_.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, format_string);
    return context_.getDiagnostics().Report(loc, diagnostic_id);
  }
};

// Find for a given upper-level task, return all direct children tasks (e.g.
// tasks instanciated directly in upper).
// Lower-level tasks or non-task functions return an empty vector.
inline std::vector<const clang::FunctionDecl*> FindChildrenTasks(
    const clang::FunctionDecl* upper) {
  auto body = upper->getBody();
  if (auto task = GetTlpTask(body)) {
    auto invokes = GetTlpInvokes(task);
    std::vector<const clang::FunctionDecl*> tasks;
    for (auto invoke : invokes) {
      // Dynamic cast correctness is guaranteed by tlp.h.
      if (auto decl_ref =
              llvm::dyn_cast<clang::DeclRefExpr>(invoke->getArg(0))) {
        tasks.push_back(
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref->getDecl()));
      }
    }
    return tasks;
  }
  return {};
}

// Recursively find all tasks instanciated. If a task is instantiated more than
// once, it will only appear once.
// Lower-level tasks or non-task functions return an empty vector.
inline std::vector<const clang::FunctionDecl*> FindAllTasks(
    const clang::FunctionDecl* upper) {
  std::vector<const clang::FunctionDecl*> tasks{upper};
  std::unordered_set<const clang::FunctionDecl*> task_set{upper};
  for (auto child : FindChildrenTasks(upper)) {
    if (task_set.count(child) == 0) {
      tasks.push_back(child);
      task_set.insert(child);
    }
  }
  return tasks;
}
}  // namespace internal
}  // namespace tlp

#endif  // TLP_TASK_H_
