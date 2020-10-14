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
std::vector<const clang::CXXOperatorCallExpr*> GetTapaMmapOps(
    const clang::Stmt* stmt);

#endif  // TAPA_MMAP_H_
