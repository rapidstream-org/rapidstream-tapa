// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "clang/AST/AST.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/StringExtras.h"

#include "../rewriter/mmap.h"
#include "../rewriter/stream.h"
#include "../rewriter/type.h"

#define ADD_FOR_FUNC_ARGS_DEF                                           \
  const clang::FunctionDecl *func,                                      \
      const std::function<void(llvm::StringRef)>&add_line,              \
      const std::function<void(std::initializer_list<llvm::StringRef>)> \
          &add_pragma

#define ADD_FOR_PARAMS_ARGS_DEF                                         \
  const clang::ParmVarDecl *param,                                      \
      const std::function<void(llvm::StringRef)>&add_line,              \
      const std::function<void(std::initializer_list<llvm::StringRef>)> \
          &add_pragma

#define REWRITE_FUNC_ARGS_DEF \
  const clang::FunctionDecl *func, clang::Rewriter &rewriter
#define REWRITE_DECL_ARGS_DEF \
  const clang::Decl *decl, const clang::Attr *attr, clang::Rewriter &rewriter
#define REWRITE_STMT_ARGS_DEF \
  const clang::Stmt *stmt, const clang::Attr *attr, clang::Rewriter &rewriter

#define ADD_FOR_FUNC_ARGS func, add_line, add_pragma
#define ADD_FOR_PARAMS_ARGS param, add_line, add_pragma
#define REWRITE_FUNC_ARGS func, rewriter
#define REWRITE_DECL_ARGS decl, attr, rewriter
#define REWRITE_STMT_ARGS stmt, attr, rewriter

namespace tapa {
namespace internal {

class Target {
 public:
  // for (.*Level)?[Type], override [Type] for all levels,
  //     or (.*Level)[Type] for each level.
  // for example, override AddCodeForStream for all levels,
  //     or AddCodeForTopLevelStream for the top level only.
  virtual void AddCodeForTopLevelFunc(ADD_FOR_FUNC_ARGS_DEF) = 0;
  virtual void AddCodeForStream(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForTopLevelStream(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForOtherStream(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForMiddleLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForOtherMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForTopLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForMiddleLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForOtherAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForScalar(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForLowerLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void AddCodeForOtherScalar(ADD_FOR_PARAMS_ARGS_DEF) = 0;
  virtual void RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void RewriteOtherFunc(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void RewriteTopLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void RewriteMiddleLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void RewriteLowerLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void RewriteOtherFuncArguments(REWRITE_FUNC_ARGS_DEF) = 0;
  virtual void ProcessNonCurrentTask(REWRITE_FUNC_ARGS_DEF,
                                     bool IsTopTapaTopLevel) = 0;
  virtual void RewritePipelinedDecl(REWRITE_DECL_ARGS_DEF,
                                    const clang::Stmt* body) = 0;
  virtual void RewritePipelinedStmt(REWRITE_STMT_ARGS_DEF,
                                    const clang::Stmt* body) = 0;
  virtual void RewriteUnrolledDecl(REWRITE_DECL_ARGS_DEF,
                                   const clang::Stmt* body) = 0;
  virtual void RewriteUnrolledStmt(REWRITE_STMT_ARGS_DEF,
                                   const clang::Stmt* body) = 0;

  static tapa::internal::Target* GetInstance() = delete;
  Target(Target const&) = delete;
  void operator=(Target const&) = delete;

 protected:
  Target() {}
};

class BaseTarget : public Target {
 public:
  virtual void AddCodeForTopLevelFunc(ADD_FOR_FUNC_ARGS_DEF);

  virtual void AddCodeForStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForTopLevelStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForOtherStream(ADD_FOR_PARAMS_ARGS_DEF);

  virtual void AddCodeForMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForOtherMmap(ADD_FOR_PARAMS_ARGS_DEF);

  virtual void AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForTopLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForOtherAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF);

  virtual void AddCodeForScalar(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForLowerLevelScalar(ADD_FOR_PARAMS_ARGS_DEF);
  virtual void AddCodeForOtherScalar(ADD_FOR_PARAMS_ARGS_DEF);

  virtual std::vector<std::string> GenerateCodeForTopLevelFunc(
      const clang::FunctionDecl* func);
  virtual std::vector<std::string> GenerateCodeForMiddleLevelFunc(
      const clang::FunctionDecl* func);
  virtual std::vector<std::string> GenerateCodeForLowerLevelFunc(
      const clang::FunctionDecl* func);
  virtual std::vector<std::string> GenerateCodeForOtherFunc(
      const clang::FunctionDecl* func);

  virtual void RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteOtherFunc(REWRITE_FUNC_ARGS_DEF);
  virtual clang::SourceRange ExtendAttrRemovalRange(clang::Rewriter& rewriter,
                                                    clang::SourceRange range);

  virtual void RewriteTopLevelFuncArguments(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteMiddleLevelFuncArguments(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteLowerLevelFuncArguments(REWRITE_FUNC_ARGS_DEF);
  virtual void RewriteOtherFuncArguments(REWRITE_FUNC_ARGS_DEF);
  virtual void ProcessNonCurrentTask(REWRITE_FUNC_ARGS_DEF,
                                     bool IsTopTapaTopLevel);
  virtual void RewritePipelinedDecl(REWRITE_DECL_ARGS_DEF,
                                    const clang::Stmt* body);
  virtual void RewritePipelinedStmt(REWRITE_STMT_ARGS_DEF,
                                    const clang::Stmt* body);

  virtual void RewriteUnrolledDecl(REWRITE_DECL_ARGS_DEF,
                                   const clang::Stmt* body);
  virtual void RewriteUnrolledStmt(REWRITE_STMT_ARGS_DEF,
                                   const clang::Stmt* body);

  static tapa::internal::Target* GetInstance() = delete;
  BaseTarget(BaseTarget const&) = delete;
  void operator=(BaseTarget const&) = delete;

 protected:
  BaseTarget() {}
};

void AddDummyStreamRW(ADD_FOR_PARAMS_ARGS_DEF, bool qdma);
void AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS_DEF);

void AddPragmaToBody(clang::Rewriter& rewriter, const clang::Stmt* body,
                     std::string pragma);
void AddPragmaAfterStmt(clang::Rewriter& rewriter, const clang::Stmt* stmt,
                        std::string pragma);

}  // namespace internal
}  // namespace tapa
