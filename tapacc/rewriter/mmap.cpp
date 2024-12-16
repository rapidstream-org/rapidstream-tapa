// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "mmap.h"

#include <string>

#include "clang/AST/AST.h"

#include "type.h"

using std::string;

using clang::ParmVarDecl;

string GetMmapElemType(const ParmVarDecl* param) {
  if (IsTapaType(param, "((async_)?mmaps?|hmap|immap|ommap)")) {
    if (auto arg = GetTemplateArg(param->getType(), 0)) {
      return GetTemplateArgName(*arg);
    }
  }
  return "";
}
