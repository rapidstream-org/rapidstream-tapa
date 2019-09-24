#ifndef TLP_MMAP_H_
#define TLP_MMAP_H_

#include <string>

#include "clang/AST/AST.h"

#include "type.h"

inline bool IsMmap(const clang::RecordDecl* decl) {
  return decl != nullptr && decl->getQualifiedNameAsString() == "tlp::mmap";
}

inline bool IsMmap(clang::QualType type) {
  return IsMmap(type->getAsRecordDecl());
}

inline bool IsMmap(const clang::ParmVarDecl* param) {
  return IsMmap(param->getType());
}

inline std::string GetMmapElemType(const clang::ParmVarDecl* param) {
  if (IsMmap(param->getType())) {
    return GetTemplateArgName(
        param->getType()->getAs<clang::TemplateSpecializationType>()->getArg(
            0));
  }
  return "";
}

std::vector<const clang::CXXOperatorCallExpr*> GetTlpMmapOps(
    const clang::Stmt* stmt);

#endif  // TLP_MMAP_H_
