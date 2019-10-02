#ifndef TLP_MMAP_H_
#define TLP_MMAP_H_

#include <string>

#include "clang/AST/AST.h"

#include "type.h"

struct MmapInfo {
  MmapInfo(const std::string& name, const std::string& type)
      : name{name}, type{type} {
    if (this->type.substr(0, 7) == "struct ") {
      this->type.erase(0, 7);
    } else if (this->type.substr(0, 6) == "class ") {
      this->type.erase(0, 6);
    }
  }
  // Name of the stream in the task.
  std::string name;
  // Type of the stream.
  std::string type;
};

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
