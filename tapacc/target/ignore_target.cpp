// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "ignore_target.h"

#include "../rewriter/mmap.h"
#include "../rewriter/stream.h"
#include "../rewriter/type.h"

#include <cctype>
#include <stdexcept>
#include <string>

using llvm::StringRef;

namespace tapa {
namespace internal {

void IgnoreTarget::AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddDummyStreamRW(ADD_FOR_PARAMS_ARGS, false);
}

void IgnoreTarget::AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS);
}

void IgnoreTarget::AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS);
}

void IgnoreTarget::AddCodeForLowerLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS);
}

void IgnoreTarget::RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  throw std::runtime_error(
      "The top-level function should be rewritten by a specific target, "
      "instead of being ignored.");
}

void IgnoreTarget::RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  throw std::runtime_error(
      "please report a bug: all ignore targets should be treated as "
      "lower-level tasks.");
}

void IgnoreTarget::RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  auto lines = GenerateCodeForLowerLevelFunc(func);
  rewriter.ReplaceText(func->getBody()->getSourceRange(),
                       "{\n" + llvm::join(lines, "\n") + "}\n");
}

void IgnoreTarget::RewriteOtherFunc(REWRITE_FUNC_ARGS_DEF) {
  // clear all non-task functions when generating the shell for the ignore
  // target.
  rewriter.ReplaceText(func->getBody()->getSourceRange(), "{}\n");
}

}  // namespace internal
}  // namespace tapa
