#include "async_mmap.h"

#include <string>

#include "clang/AST/AST.h"

using std::string;

using clang::CXXMemberCallExpr;
using clang::DeclRefExpr;
using clang::DiagnosticsEngine;
using clang::Stmt;

using llvm::dyn_cast;

AsyncMmapOpEnum GetAsyncMmapOp(const CXXMemberCallExpr* call_expr) {
  if (!IsAsyncMmap(call_expr->getRecordDecl())) {
    return AsyncMmapOpEnum::kNotAsyncMmapOperation;
  }
  // Check caller name and number of arguments.
  const string callee = call_expr->getMethodDecl()->getNameAsString();
  if (callee == "fence") {
    return AsyncMmapOpEnum::kFence;
  }
  if (callee == "write_addr_write") {
    return AsyncMmapOpEnum::kWriteAddrWrite;
  }
  if (callee == "write_data_write") {
    return AsyncMmapOpEnum::kWriteDataWrite;
  }
  if (callee == "read_addr_try_write") {
    return AsyncMmapOpEnum::kReadAddrTryWrite;
  }
  if (callee == "read_data_empty") {
    return AsyncMmapOpEnum::kReadDataEmpty;
  }
  if (callee == "read_data_try_read") {
    return AsyncMmapOpEnum::kReadDataTryRead;
  }

  return AsyncMmapOpEnum::kNotAsyncMmapOperation;
}

// Retrive information about async_mmap.
void AsyncMmapInfo::GetAsyncMmapInfo(const Stmt* root,
                                     DiagnosticsEngine& diagnostics_engine) {
  for (const auto stmt : root->children()) {
    if (stmt == nullptr) continue;
    GetAsyncMmapInfo(stmt, diagnostics_engine);

    if (const auto call_expr = dyn_cast<CXXMemberCallExpr>(stmt)) {
      const string callee = call_expr->getMethodDecl()->getNameAsString();
      const string caller =
          GetNameOfFirst<DeclRefExpr>(call_expr->getImplicitObjectArgument());
      if (this->name == caller) {
        const auto op = GetAsyncMmapOp(call_expr);
        this->has_fence |=
            (op & AsyncMmapOpEnum::kFence) == AsyncMmapOpEnum::kFence;
        this->is_write |=
            (op & AsyncMmapOpEnum::kWrite) == AsyncMmapOpEnum::kWrite;
        this->is_read |=
            (op & AsyncMmapOpEnum::kRead) == AsyncMmapOpEnum::kRead;
        this->is_addr |=
            (op & AsyncMmapOpEnum::kAddr) == AsyncMmapOpEnum::kAddr;
        this->is_data |=
            (op & AsyncMmapOpEnum::kData) == AsyncMmapOpEnum::kData;
        this->ops.push_back(op);
        this->call_exprs.push_back(call_expr);
      }
    }
  }
}

string AsyncMmapInfo::GetReplacedText() const {
  string replaced_text;
  if (this->is_write) {
    if (this->is_addr) {
      if (!replaced_text.empty()) replaced_text += ",\n";
      replaced_text += "hls::stream<uint64_t>& " + this->WriteAddrVar();
    }
    if (this->is_data) {
      if (!replaced_text.empty()) replaced_text += ",\n";
      replaced_text +=
          "hls::stream<" + this->type + ">& " + this->WriteDataVar();
    }
  }
  if (this->is_read) {
    if (this->is_addr) {
      if (!replaced_text.empty()) replaced_text += ",\n";
      replaced_text += "hls::stream<uint64_t>& " + this->ReadAddrVar();
    }
    if (this->is_data) {
      if (!replaced_text.empty()) replaced_text += ",\n";
      replaced_text +=
          "hls::stream<" + this->type + ">& " + this->ReadDataVar();
    }
  }
  return replaced_text;
}