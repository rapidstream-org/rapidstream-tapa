#include "mmap.h"

#include <string>

#include "clang/AST/AST.h"

#include "type.h"

using std::string;

using clang::ParmVarDecl;

string GetMmapElemType(const ParmVarDecl* param) {
  if (IsTapaType(param, "(async_)?mmaps?")) {
    if (auto arg = GetTemplateArg(param->getType(), 0)) {
      return GetTemplateArgName(*arg);
    }
  }
  return "";
}
