#include "mmap.h"

#include <vector>

#include "clang/AST/AST.h"

using std::vector;

using clang::CXXOperatorCallExpr;
using clang::DeclRefExpr;
using clang::Stmt;

using llvm::dyn_cast;

// Given a Stmt, find all tlp::mmap operations via DFS and update mmap_ops.
void GetTlpMmapOps(const Stmt* stmt,
                   vector<const CXXOperatorCallExpr*>& mmap_ops) {
  if (stmt == nullptr) {
    return;
  }
  for (auto child : stmt->children()) {
    GetTlpMmapOps(child, mmap_ops);
  }
  if (const auto call_expr = dyn_cast<CXXOperatorCallExpr>(stmt)) {
    if (const auto caller = dyn_cast<DeclRefExpr>(call_expr->getArg(0))) {
      if (IsMmap(caller->getType())) {
        mmap_ops.push_back(call_expr);
      }
    }
  }
}

// Given a Stmt, return all tlp::mmap opreations via DFS.
vector<const CXXOperatorCallExpr*> GetTlpMmapOps(const Stmt* stmt) {
  vector<const CXXOperatorCallExpr*> mmap_ops;
  GetTlpMmapOps(stmt, mmap_ops);
  return mmap_ops;
}
