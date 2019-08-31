#ifndef TLP_TASK_H_
#define TLP_TASK_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "stream.h"

const clang::ExprWithCleanups* GetTlpTask(const clang::Stmt* func_body);

std::vector<const clang::CXXMemberCallExpr*> GetTlpInvokes(
    const clang::Stmt* task);

class TlpVisitor : public clang::RecursiveASTVisitor<TlpVisitor> {
 public:
  explicit TlpVisitor(clang::ASTContext& context, clang::Rewriter& rewriter)
      : context_{context}, rewriter_{rewriter} {}

  bool VisitFunctionDecl(clang::FunctionDecl* func);

 private:
  clang::ASTContext& context_;
  clang::Rewriter& rewriter_;
  bool first_func_{true};

  bool InsertHlsPragma(
      const clang::SourceLocation& loc, const std::string& pragma,
      const std::vector<std::pair<std::string, std::string>>& args);

  void ProcessUpperLevelTask(const clang::ExprWithCleanups* task,
                             const clang::FunctionDecl* func);

  void ProcessLowerLevelTask(const clang::FunctionDecl* func);

  void RewriteStreams(
      const clang::Stmt* stmt,
      std::unordered_map<const clang::CXXMemberCallExpr*, const StreamInfo*>
          stream_table,
      std::shared_ptr<std::unordered_map<const clang::Stmt*, bool>> visited =
          nullptr);
  void RewriteStream(const clang::CXXMemberCallExpr* call_expr,
                     const StreamInfo& stream);
};

#endif  // TLP_TASK_H_
