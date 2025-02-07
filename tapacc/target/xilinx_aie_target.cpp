// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "xilinx_aie_target.h"

#include "../rewriter/mmap.h"
#include "../rewriter/stream.h"
#include "../rewriter/type.h"

#include <cctype>
#include <fstream>
#include <string>
using clang::TapaTargetAttr;
using llvm::StringRef;

void aie_log_out(std::string path, std::string str, bool append) {
  std::ofstream file(path, append ? std::ios::app : std::ios::out);
  file << str << std::endl;
}

namespace tapa {
namespace internal {

extern bool vitis_mode;

static void AddDummyStreamRW(ADD_FOR_PARAMS_ARGS_DEF, bool qdma) {}

static void AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS_DEF) {}

void XilinxAIETarget::AddCodeForTopLevelFunc(ADD_FOR_FUNC_ARGS_DEF) {
  // Set top-level control to s_axilite for Vitis mode.
}

void XilinxAIETarget::AddCodeForTopLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {}

void XilinxAIETarget::AddCodeForTopLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS);
}

void XilinxAIETarget::AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {}

void XilinxAIETarget::AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {}

void XilinxAIETarget::AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS);
  AddDummyStreamRW(ADD_FOR_PARAMS_ARGS, false);
}

void XilinxAIETarget::AddCodeForMiddleLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS);
}

void XilinxAIETarget::AddCodeForMiddleLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS);
}

void XilinxAIETarget::AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS);
}

void XilinxAIETarget::AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  assert(IsTapaType(param, "(i|o)streams?"));
  ;
}

void XilinxAIETarget::AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {}

void XilinxAIETarget::AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {}

void XilinxAIETarget::RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  // Remove the TapaTargetAttr defintion.
  auto attr = func->getAttr<TapaTargetAttr>();
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));

  // Rewrite the function arguments for AIE target.
  // RewriteFuncArguments(func, rewriter, true);

  // Remove the function directly.
  rewriter.ReplaceText(func->getSourceRange(), ";\n");
}

void XilinxAIETarget::RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF) {}

static void RewriteReadWriteCalls(const clang::Stmt* stmt,
                                  clang::Rewriter& rewriter) {
  if (stmt == nullptr) return;
  for (const auto* child : stmt->children()) {
    // Recursively traverse the children
    RewriteReadWriteCalls(child, rewriter);
  }
  //::task_log_out("write_calls.txt", name, true);
  if (const auto* callExpr = llvm::dyn_cast<clang::CXXMemberCallExpr>(stmt)) {
    if (const auto* methodDecl = callExpr->getMethodDecl()) {
      if (methodDecl->getNameAsString() == "write") {
        // This statement is to replace XXX.write(YYY) with writeincr(XXX, YYY)
        if (const auto* memberExpr =
                llvm::dyn_cast<clang::MemberExpr>(callExpr->getCallee())) {
          const clang::Expr* base = memberExpr->getBase();  // `<yyy>`
          const clang::Expr* arg = callExpr->getArg(0);     // `<xxx>`
          // Get rewritten text for the base and argument
          std::string baseText =
              rewriter.getRewrittenText(base->getSourceRange());
          std::string argText =
              rewriter.getRewrittenText(arg->getSourceRange());
          // Construct the replacement text
          std::string replacement =
              "writeincr(" + baseText + ", " + argText + ")";
          // Replace the original call with the new expression
          rewriter.ReplaceText(callExpr->getSourceRange(), replacement);
        }
      }
      if (methodDecl->getNameAsString() == "read") {
        // This statement is to replace XXX.read() with readincr(XXX)
        if (const auto* memberExpr =
                llvm::dyn_cast<clang::MemberExpr>(callExpr->getCallee())) {
          const clang::Expr* base = memberExpr->getBase();  // `<yyy>`
          // Get rewritten text for the base and argument
          std::string baseText =
              rewriter.getRewrittenText(base->getSourceRange());
          // Construct the replacement text
          std::string replacement = "readincr(" + baseText + ")";
          // Replace the original call with the new expression
          rewriter.ReplaceText(callExpr->getSourceRange(), replacement);
        }
      }
    }
  }
}

void XilinxAIETarget::RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  RewriteReadWriteCalls(func->getBody(), rewriter);

  // Remove the TapaTargetAttr defintion.
  auto attr = func->getAttr<TapaTargetAttr>();
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));

  // Rewrite the function arguments for AIE target.
  RewriteFuncArguments(func, rewriter, false);

  auto lines = GenerateCodeForLowerLevelFunc(func);
  rewriter.InsertTextAfterToken(func->getBody()->getBeginLoc(),
                                llvm::join(lines, "\n"));
}

void XilinxAIETarget::ProcessNonCurrentTask(REWRITE_FUNC_ARGS_DEF,
                                            bool IsTapaTopLevel) {
  // Remove the TapaTargetAttr defintion.
  auto attr = func->getAttr<TapaTargetAttr>();
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));

  // Remove the function directly.
  if (IsTapaTopLevel == true) {
    rewriter.ReplaceText(func->getBody()->getSourceRange(), ";\n");
    RewriteFuncArguments(func, rewriter, false);
    ::aie_log_out("aielog.txt", func->getNameAsString() + " is top task", true);
  } else {
    rewriter.RemoveText(func->getSourceRange());
    ::aie_log_out("aielog.txt", func->getNameAsString() + " is not top task",
                  true);
  }
}
void XilinxAIETarget::RewriteFuncArguments(const clang::FunctionDecl* func,
                                           clang::Rewriter& rewriter,
                                           bool top) {
  // bool adf_header_inserted = false;

  // Replace mmaps arguments with 64-bit base addresses.
  for (const auto param : func->parameters()) {
    const std::string param_name = param->getNameAsString();
    if (IsTapaType(param, "immap")) {
      int width =
          param->getASTContext()
              .getTypeInfo(GetTemplateArg(param->getType(), 0)->getAsType())
              .Width;
      rewriter.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "input_window<uint" + std::to_string(width) + ">* restrict");
    } else if (IsTapaType(param, "ommap")) {
      int width =
          param->getASTContext()
              .getTypeInfo(GetTemplateArg(param->getType(), 0)->getAsType())
              .Width;
      rewriter.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "output_window<uint" + std::to_string(width) + ">* restrict");
    } else if (IsTapaType(param, "(async_)?mmap")) {
      int width =
          param->getASTContext()
              .getTypeInfo(GetTemplateArg(param->getType(), 0)->getAsType())
              .Width;
      rewriter.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "input_window<uint" + std::to_string(width) + ">* restrict");
    } else if (IsTapaType(param, "((async_)?mmaps|hmap)")) {
      ;
    } else if (IsTapaType(param, "istream")) {
      int width =
          param->getASTContext()
              .getTypeInfo(GetTemplateArg(param->getType(), 0)->getAsType())
              .Width;
      rewriter.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "input_stream<uint" + std::to_string(width) + ">* ");
      // if (!adf_header_inserted) {
      //   rewriter.InsertText(func->getBeginLoc(), "#include <adf.h>\n", true);
      //   adf_header_inserted = true;
      // }
    } else if (IsTapaType(param, "ostream")) {
      int width =
          param->getASTContext()
              .getTypeInfo(GetTemplateArg(param->getType(), 0)->getAsType())
              .Width;
      rewriter.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "output_stream<uint" + std::to_string(width) + ">* ");
      // if (!adf_header_inserted) {
      //   rewriter.InsertText(func->getBeginLoc(), "#include <adf.h>\n", true);
      //   adf_header_inserted = true;
      // }
    }
  }
}

static void AddPragmaToBody(clang::Rewriter& rewriter, const clang::Stmt* body,
                            std::string pragma) {
  if (auto compound = llvm::dyn_cast<clang::CompoundStmt>(body)) {
    rewriter.InsertText(compound->getLBracLoc(), std::string(pragma + "\n"));
  }
}

static void AddPipelinePragma(clang::Rewriter& rewriter,
                              const clang::Stmt* body) {
  std::string pragma = "chess_prepare_for_pipelining";
  AddPragmaToBody(rewriter, body, pragma);
}

void XilinxAIETarget::RewritePipelinedDecl(REWRITE_DECL_ARGS_DEF,
                                           const clang::Stmt* body) {
  if (auto pipeline = llvm::dyn_cast<clang::TapaPipelineAttr>(attr)) {
    if (body) AddPipelinePragma(rewriter, body);
  }
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));
}

void XilinxAIETarget::RewritePipelinedStmt(REWRITE_STMT_ARGS_DEF,
                                           const clang::Stmt* body) {
  if (auto pipeline = llvm::dyn_cast<clang::TapaPipelineAttr>(attr)) {
    if (body) AddPipelinePragma(rewriter, body);
  }
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));
}

void XilinxAIETarget::RewriteUnrolledStmt(REWRITE_STMT_ARGS_DEF,
                                          const clang::Stmt* body) {}

}  // namespace internal
}  // namespace tapa
