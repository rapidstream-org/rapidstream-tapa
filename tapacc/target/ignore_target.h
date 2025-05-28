// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "base_target.h"

namespace tapa {
namespace internal {

// An ignore target simply removes the function body so that the function
// will not be handled by the downstream compiler.
class IgnoreTarget : public BaseTarget {
 public:
  virtual void AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelScalar(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteOtherFunc(REWRITE_FUNC_ARGS_DEF);

  static tapa::internal::Target* GetInstance() {
    static IgnoreTarget instance;
    return &instance;
  }

  IgnoreTarget(IgnoreTarget const&) = delete;
  void operator=(IgnoreTarget const&) = delete;

 protected:
  IgnoreTarget() {}
};

}  // namespace internal
}  // namespace tapa
