// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "base_target.h"

namespace tapa {
namespace internal {

class XilinxHLSTarget : public BaseTarget {
 public:
  virtual void AddCodeForTopLevelFunc(ADD_FOR_FUNC_ARGS_DEF);
  virtual void AddCodeForTopLevelStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForTopLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteOtherFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteTopLevelFuncArguments(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteMiddleLevelFuncArguments(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteOtherFuncArguments(REWRITE_FUNC_ARGS_DEF);
  virtual void ProcessNonCurrentTask(REWRITE_FUNC_ARGS_DEF,
                                     bool IsTopTapaTopLevel);
  virtual void RewritePipelinedDecl(REWRITE_DECL_ARGS_DEF,
                                    const clang::Stmt* body);
  virtual void RewritePipelinedStmt(REWRITE_STMT_ARGS_DEF,
                                    const clang::Stmt* body);
  virtual void RewriteUnrolledStmt(REWRITE_STMT_ARGS_DEF,
                                   const clang::Stmt* body);

  static tapa::internal::Target* GetInstance(bool is_vitis) {
    static XilinxHLSTarget instance_vitis(true);
    static XilinxHLSTarget instance_non_vitis(false);
    return is_vitis ? &instance_vitis : &instance_non_vitis;
  }

  XilinxHLSTarget(XilinxHLSTarget const&) = delete;
  void operator=(XilinxHLSTarget const&) = delete;

 protected:
  XilinxHLSTarget(bool is_vitis) : is_vitis(is_vitis) {}

 private:
  bool is_vitis = false;  // True if the target is Vitis instead of Vitis HLS.
};

}  // namespace internal
}  // namespace tapa
