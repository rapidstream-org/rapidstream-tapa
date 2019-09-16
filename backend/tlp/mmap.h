#ifndef TLP_MMAP_H_
#define TLP_MMAP_H_

#include <string>

#include "clang/AST/AST.h"

#include "type.h"

inline bool IsMmap(const clang::NamedDecl* decl) {
  return decl != nullptr && decl->getQualifiedNameAsString() == "tlp::mmap";
}

inline bool IsMmap(clang::QualType type) {
  return IsMmap(type.getCanonicalType().getTypePtr()->getAsRecordDecl());
}

inline std::string GetMmapElemType(const clang::ParmVarDecl* param) {
  if (auto decl =
          param->getType().getCanonicalType().getTypePtr()->getAsRecordDecl()) {
    if (IsMmap(decl)) {
      return GetCanonicalTypeName(
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)
              ->getTemplateArgs()[0]
              .getAsType());
    }
  }
  return "";
}

std::vector<const clang::CXXOperatorCallExpr*> GetTlpMmapOps(
    const clang::Stmt* stmt);

#endif  // TLP_MMAP_H_
