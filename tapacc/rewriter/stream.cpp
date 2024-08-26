// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "stream.h"

#include <vector>

#include "clang/AST/AST.h"

using std::vector;

using clang::ClassTemplateSpecializationDecl;
using clang::CXXMemberCallExpr;
using clang::QualType;
using clang::Stmt;
using clang::Type;

using llvm::dyn_cast;

// Given a Stmt, find all tapa::istream and tapa::ostream operations via DFS and
// update stream_ops.
void GetTapaStreamOps(const Stmt* stmt,
                      vector<const CXXMemberCallExpr*>& stream_ops,
                      bool init = false) {
  if (stmt == nullptr) {
    return;
  }
  if (!init) {
    switch (stmt->getStmtClass()) {
      case Stmt::DoStmtClass:
      case Stmt::ForStmtClass:
      case Stmt::WhileStmtClass:
        return;
      default: {
      }
    }
  }
  for (auto child : stmt->children()) {
    GetTapaStreamOps(child, stream_ops);
  }
  if (const auto stream = dyn_cast<CXXMemberCallExpr>(stmt)) {
    if (IsStreamInterface(stream->getRecordDecl())) {
      stream_ops.push_back(stream);
    }
  }
}

// Given a Stmt, return all tapa::istream and tapa::ostream opreations via DFS.
vector<const CXXMemberCallExpr*> GetTapaStreamOps(const Stmt* stmt) {
  vector<const CXXMemberCallExpr*> stream_ops;
  GetTapaStreamOps(stmt, stream_ops, /* init = */ true);
  return stream_ops;
}

const ClassTemplateSpecializationDecl* GetTapaStreamDecl(const Type* type) {
  if (type != nullptr) {
    if (const auto record = type->getAsRecordDecl()) {
      if (const auto decl = dyn_cast<ClassTemplateSpecializationDecl>(record)) {
        if (IsStream(decl)) {
          return decl;
        }
      }
    }
  }
  return nullptr;
}

const ClassTemplateSpecializationDecl* GetTapaStreamDecl(
    const QualType& qual_type) {
  return GetTapaStreamDecl(
      qual_type.getUnqualifiedType().getCanonicalType().getTypePtr());
}

const ClassTemplateSpecializationDecl* GetTapaStreamsDecl(const Type* type) {
  if (type != nullptr) {
    if (const auto record = type->getAsRecordDecl()) {
      if (const auto decl = dyn_cast<ClassTemplateSpecializationDecl>(record)) {
        if (IsTapaType(decl, "(i|o)?streams")) {
          return decl;
        }
      }
    }
  }
  return nullptr;
}

const ClassTemplateSpecializationDecl* GetTapaStreamsDecl(
    const QualType& qual_type) {
  return GetTapaStreamsDecl(
      qual_type.getUnqualifiedType().getCanonicalType().getTypePtr());
}
