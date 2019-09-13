#ifndef TLP_TYPE_H_
#define TLP_TYPE_H_

#include <sstream>
#include <string>

#include "clang/AST/AST.h"

inline std::string GetCanonicalTypeName(
    clang::QualType type, clang::PrintingPolicy policy = clang::LangOptions()) {
  std::string name;
  llvm::raw_string_ostream oss{name};
  type.getCanonicalType().print(oss, policy);
  return name;
}

#endif  // TLP_TYPE_H_