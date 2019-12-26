#ifndef TLP_MMAP_H_
#define TLP_MMAP_H_

#include <string>

#include "clang/AST/AST.h"

#include "type.h"

struct MmapInfo : public ObjectInfo {
  MmapInfo(const std::string& name, const std::string& type)
      : ObjectInfo(name, type) {}
};

// Does NOT include tlp::async_mmap.
inline bool IsMmap(const clang::RecordDecl* decl) {
  return decl != nullptr && decl->getQualifiedNameAsString() == "tlp::mmap";
}

// Does NOT include tlp::async_mmap.
inline bool IsMmap(clang::QualType type) {
  return IsMmap(type->getAsRecordDecl());
}

// Does NOT include tlp::async_mmap.
inline bool IsMmap(const clang::ParmVarDecl* param) {
  return IsMmap(param->getType());
}

// Does NOT include tlp::async_mmap.
std::vector<const clang::CXXOperatorCallExpr*> GetTlpMmapOps(
    const clang::Stmt* stmt);

#endif  // TLP_MMAP_H_
