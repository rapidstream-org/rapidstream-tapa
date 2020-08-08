#ifndef TAPA_ASYNC_MMAP_H_
#define TAPA_ASYNC_MMAP_H_

#include <string>

#include "clang/AST/AST.h"

#include "mmap.h"
#include "stream.h"
#include "type.h"

enum class AsyncMmapOpEnum : uint64_t {
  kNotAsyncMmapOperation = 0,
  kFence = 1ULL << 8,
  kWrite = 1ULL << 9,
  kRead = 1ULL << 10,
  kAddr = 1ULL << 11,
  kData = 1ULL << 12,

  kWriteAddrTryWrite =
      kWrite | kAddr | static_cast<uint64_t>(StreamOpEnum::kTryWrite),
  kWriteAddrWrite =
      kWrite | kAddr | static_cast<uint64_t>(StreamOpEnum::kWrite),
  kWriteDataTryWrite =
      kWrite | kData | static_cast<uint64_t>(StreamOpEnum::kTryWrite),
  kWriteDataWrite =
      kWrite | kData | static_cast<uint64_t>(StreamOpEnum::kWrite),
  kReadAddrTryWrite =
      kRead | kAddr | static_cast<uint64_t>(StreamOpEnum::kTryWrite),
  kReadDataEmpty =
      kRead | kData | static_cast<uint64_t>(StreamOpEnum::kTestEmpty),
  kReadDataTryRead =
      kRead | kData | static_cast<uint64_t>(StreamOpEnum::kTryRead)
};

struct AsyncMmapInfo : public ObjectInfo {
  AsyncMmapInfo(const std::string& name, const std::string& type)
      : ObjectInfo(name, type) {}

  std::vector<AsyncMmapOpEnum> ops;
  std::vector<const clang::CXXMemberCallExpr*> call_exprs;

  bool has_fence = false;
  bool is_write = false;
  bool is_read = false;
  bool is_addr = false;
  bool is_data = false;

  std::string ReadAddrVar() const { return name + ".read_addr"; }
  std::string ReadDataVar() const { return name + ".read_data"; }
  std::string ReadDataPeekVar() const { return name + ".read_peek"; }
  std::string WriteAddrVar() const { return name + ".write_addr"; }
  std::string WriteDataVar() const { return name + ".write_data"; }

  void GetAsyncMmapInfo(const clang::Stmt* root,
                        clang::DiagnosticsEngine& diagnostics_engine);

  std::string GetReplacedText() const;
};

inline bool IsAsyncMmap(const clang::RecordDecl* decl) {
  return decl != nullptr &&
         decl->getQualifiedNameAsString() == "tapa::async_mmap";
}

inline bool IsAsyncMmap(clang::QualType type) {
  return IsAsyncMmap(type->getAsRecordDecl());
}

inline bool IsAsyncMmap(const clang::ParmVarDecl* param) {
  return IsAsyncMmap(param->getType());
}

// Works for both mmap and async_mmap.
inline std::string GetMmapElemType(const clang::ParmVarDecl* param) {
  if (IsTapaType(param, "(async_)?mmaps?")) {
    return GetTemplateArgName(
        param->getType()->getAs<clang::TemplateSpecializationType>()->getArg(
            0));
  }
  return "";
}

#endif  // TAPA_ASYNC_MMAP_H_
