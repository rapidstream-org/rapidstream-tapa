#ifndef TAPA_MMAP_H_
#define TAPA_MMAP_H_

#include <string>

#include "clang/AST/AST.h"

#include "type.h"

struct MmapInfo : public ObjectInfo {
  MmapInfo(const std::string& name, const std::string& type)
      : ObjectInfo(name, type) {}
};

// Does NOT include tapa::async_mmap.
inline bool IsMmap(const clang::RecordDecl* decl) {
  return decl != nullptr && decl->getQualifiedNameAsString() == "tapa::mmap";
}

// Does NOT include tapa::async_mmap.
inline bool IsMmap(clang::QualType type) {
  return IsMmap(type->getAsRecordDecl());
}

// Does NOT include tapa::async_mmap.
inline bool IsMmap(const clang::ParmVarDecl* param) {
  return IsMmap(param->getType());
}

// Does NOT include tapa::async_mmap.
std::vector<const clang::CXXOperatorCallExpr*> GetTapaMmapOps(
    const clang::Stmt* stmt);

#endif  // TAPA_MMAP_H_
