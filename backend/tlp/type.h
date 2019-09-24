#ifndef TLP_TYPE_H_
#define TLP_TYPE_H_

#include <sstream>
#include <string>

#include "clang/AST/AST.h"

inline std::string GetTemplateArgName(const clang::TemplateArgument& arg) {
  std::string name;
  llvm::raw_string_ostream oss{name};
  clang::LangOptions options;
  options.CPlusPlus = true;
  options.Bool = true;
  arg.print(options, oss);
  return name;
}

#endif  // TLP_TYPE_H_