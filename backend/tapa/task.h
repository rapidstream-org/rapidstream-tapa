#ifndef TAPA_TASK_H_
#define TAPA_TASK_H_

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "nlohmann/json.hpp"

#include "stream.h"

namespace tapa {
namespace internal {

const clang::ExprWithCleanups* GetTapaTask(const clang::Stmt* func_body);

std::vector<const clang::CXXMemberCallExpr*> GetTapaInvokes(
    const clang::Stmt* task);

class Visitor : public clang::RecursiveASTVisitor<Visitor> {
 public:
  explicit Visitor(
      clang::ASTContext& context,
      std::vector<const clang::FunctionDecl*>& funcs,
      std::unordered_map<const clang::FunctionDecl*, clang::Rewriter>&
          rewriters,
      std::unordered_map<const clang::FunctionDecl*, nlohmann::json>& metadata)
      : context_{context},
        funcs_{funcs},
        rewriters_{rewriters},
        metadata_{metadata} {}

  bool VisitFunctionDecl(clang::FunctionDecl* func);

  static thread_local const clang::FunctionDecl* current_task;

 private:
  clang::ASTContext& context_;
  std::vector<const clang::FunctionDecl*>& funcs_;
  std::unordered_map<const clang::FunctionDecl*, clang::Rewriter>& rewriters_;
  std::unordered_map<const clang::FunctionDecl*, nlohmann::json>& metadata_;

  clang::Rewriter& GetRewriter() { return rewriters_[current_task]; }
  nlohmann::json& GetMetadata() { return metadata_[current_task]; }

  void ProcessUpperLevelTask(const clang::ExprWithCleanups* task,
                             const clang::FunctionDecl* func);

  void ProcessLowerLevelTask(const clang::FunctionDecl* func);
  std::string GetFrtInterface(const clang::FunctionDecl* func);

  clang::CharSourceRange GetCharSourceRange(const clang::Stmt* stmt);
  clang::CharSourceRange GetCharSourceRange(clang::SourceRange range);
  clang::SourceLocation GetEndOfLoc(clang::SourceLocation loc);

  int64_t EvalAsInt(const clang::Expr* expr);
};

// Find for a given upper-level task, return all direct children tasks (e.g.
// tasks instanciated directly in upper).
// Lower-level tasks or non-task functions return an empty vector.
inline std::vector<const clang::FunctionDecl*> FindChildrenTasks(
    const clang::FunctionDecl* upper) {
  auto body = upper->getBody();
  if (auto task = GetTapaTask(body)) {
    auto invokes = GetTapaInvokes(task);
    std::vector<const clang::FunctionDecl*> tasks;
    for (auto invoke : invokes) {
      // Dynamic cast correctness is guaranteed by tapa.h.
      if (auto decl_ref =
              llvm::dyn_cast<clang::DeclRefExpr>(invoke->getArg(0))) {
        auto func_decl =
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref->getDecl());
        if (func_decl->isTemplateInstantiation()) {
          func_decl = func_decl->getPrimaryTemplate()->getTemplatedDecl();
        }
        tasks.push_back(func_decl);
      }
    }
    return tasks;
  }
  return {};
}

// Find all tasks instanciated using breadth-first search.
// If a task is instantiated more than once, it will only appear once.
// Lower-level tasks or non-task functions return an empty vector.
inline std::vector<const clang::FunctionDecl*> FindAllTasks(
    const clang::FunctionDecl* root_upper) {
  std::vector<const clang::FunctionDecl*> tasks{root_upper};
  std::unordered_set<const clang::FunctionDecl*> task_set{root_upper};
  std::queue<const clang::FunctionDecl*> task_queue;

  task_queue.push(root_upper);
  while (!task_queue.empty()) {
    auto upper = task_queue.front();
    for (auto child : FindChildrenTasks(upper)) {
      if (task_set.count(child) == 0) {
        tasks.push_back(child);
        task_set.insert(child);
        task_queue.push(child);
      }
    }
    task_queue.pop();
  }

  return tasks;
}

// Return the body of a loop stmt or nullptr if the input is not a loop.
inline const clang::Stmt* GetLoopBody(const clang::Stmt* loop) {
  if (loop != nullptr) {
    if (auto stmt = llvm::dyn_cast<clang::DoStmt>(loop)) {
      return stmt->getBody();
    }
    if (auto stmt = llvm::dyn_cast<clang::ForStmt>(loop)) {
      return stmt->getBody();
    }
    if (auto stmt = llvm::dyn_cast<clang::WhileStmt>(loop)) {
      return stmt->getBody();
    }
  }
  return nullptr;
}

}  // namespace internal
}  // namespace tapa

#endif  // TAPA_TASK_H_
