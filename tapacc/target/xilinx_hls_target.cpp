// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "xilinx_hls_target.h"

#include "../rewriter/mmap.h"
#include "../rewriter/stream.h"
#include "../rewriter/type.h"

#include <cctype>
#include <string>

#include <clang/Lex/Lexer.h>

using clang::Lexer;
using llvm::StringRef;

namespace tapa {
namespace internal {

extern bool vitis_mode;

static void AddDummyStreamRW(ADD_FOR_PARAMS_ARGS_DEF, bool qdma) {
  auto param_name = param->getNameAsString();
  auto add_dummy_read = [&add_line](std::string name) {
    add_line("{ auto val = " + name + ".read(); }");
  };
  auto add_dummy_write = [&add_line](std::string name, std::string type) {
    add_line(name + ".write(" + type + "());");
  };

  if (IsTapaType(param, "istream")) {
    add_dummy_read(param_name);

  } else if (IsTapaType(param, "ostream")) {
    auto type = GetStreamElemType(param);
    if (qdma) {
      int width =
          param->getASTContext()
              .getTypeInfo(GetTemplateArg(param->getType(), 0)->getAsType())
              .Width;
      type = "qdma_axis<" + std::to_string(width) + ", 0, 0, 0>";
    }
    add_dummy_write(param_name, type);

  } else if (IsTapaType(param, "istreams")) {
    if (qdma) {
      add_line("#error istreams not supported for qdma-based tasks");
    } else {
      for (int i = 0; i < GetArraySize(param); ++i) {
        add_dummy_read(ArrayNameAt(param_name, i));
      }
    }

  } else if (IsTapaType(param, "ostreams")) {
    if (qdma) {
      add_line("#error ostreams not supported for qdma-based tasks");
    } else {
      for (int i = 0; i < GetArraySize(param); ++i) {
        add_dummy_write(ArrayNameAt(param_name, i), GetStreamElemType(param));
      }
    }
  }
}

static void AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS_DEF) {
  auto param_name = param->getNameAsString();
  if (IsTapaType(param, "((async_)?mmaps|hmap)")) {
    for (int i = 0; i < GetArraySize(param); ++i) {
      add_line("{ auto val = reinterpret_cast<volatile uint8_t&>(" +
               GetArrayElem(param_name, i) + "); }");
    }
  } else {
    auto elem_type = param->getType();
    const bool is_const = elem_type.isConstQualified();
    add_line(std::string("{ auto val = reinterpret_cast<volatile ") +
             (is_const ? "const " : "") + "uint8_t&>(" + param_name + "); }");
  }
}

static void AddPragmaToBody(clang::Rewriter& rewriter, const clang::Stmt* body,
                            std::string pragma) {
  if (auto compound = llvm::dyn_cast<clang::CompoundStmt>(body)) {
    rewriter.InsertTextAfterToken(compound->getLBracLoc(),
                                  std::string("\n#pragma ") + pragma + "\n");
  } else {
    rewriter.InsertTextBefore(body->getBeginLoc(),
                              std::string("_Pragma(\"") + pragma + "\")");
  }
}

static void AddPragmaAfterStmt(clang::Rewriter& rewriter,
                               const clang::Stmt* stmt, std::string pragma) {
  rewriter.InsertTextAfterToken(stmt->getEndLoc(),
                                std::string("\n#pragma ") + pragma + "\n");
}

static void AddPipelinePragma(clang::Rewriter& rewriter,
                              const clang::TapaPipelineAttr* attr,
                              const clang::Stmt* body) {
  auto II = attr->getII();
  std::string pragma = "HLS pipeline";
  if (II) pragma += std::string(" II = ") + std::to_string(II);
  AddPragmaToBody(rewriter, body, pragma);
}

static void AddStreamDepthPragma(clang::Rewriter& rewriter,
                                 const clang::Stmt* stmt, std::string name,
                                 int depth) {
  std::string pragma = "HLS STREAM";
  pragma += " variable = " + name + "._";
  pragma += " depth = " + std::to_string(depth);
  AddPragmaAfterStmt(rewriter, stmt, pragma);
}

void XilinxHLSTarget::AddCodeForTopLevelFunc(ADD_FOR_FUNC_ARGS_DEF) {
  // Set top-level control to s_axilite for Vitis mode.
  if (vitis_mode) {
    add_pragma({"HLS interface s_axilite port = return bundle = control"});
    add_line("");
  }
}

void XilinxHLSTarget::AddCodeForTopLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  if (vitis_mode) {
    add_pragma({"HLS interface axis port =", param->getNameAsString()});
    AddDummyStreamRW(ADD_FOR_PARAMS_ARGS, true);
  } else {
    AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS);
  }
}

void XilinxHLSTarget::AddCodeForTopLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  if (!vitis_mode) {
    add_line("#error top-level async_mmaps not supported in non-Vitis mode");
    return;
  }
  AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS);
}

void XilinxHLSTarget::AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  if (!vitis_mode) {
    add_line("#error top-level mmaps not supported in non-Vitis mode");
    return;
  }
  auto param_name = param->getNameAsString();
  if (IsTapaType(param, "((async_)?mmaps|hmap)")) {
    for (int i = 0; i < GetArraySize(param); ++i) {
      add_pragma({"HLS interface s_axilite port =", GetArrayElem(param_name, i),
                  "bundle = control"});
    }
  } else {
    AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS);
  }

  AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS);
}

void XilinxHLSTarget::AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  if (vitis_mode) {
    add_pragma({"HLS interface s_axilite port =", param->getNameAsString(),
                "bundle = control"});
    AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS);
  } else {
    AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS);
  }
}

void XilinxHLSTarget::AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS);
  AddDummyStreamRW(ADD_FOR_PARAMS_ARGS, false);
}

void XilinxHLSTarget::AddCodeForMiddleLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS);
}

void XilinxHLSTarget::AddCodeForMiddleLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS);
}

void XilinxHLSTarget::AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  // Make sure ap_clk and ap_rst_n are generated for middle-level mmaps and
  // scalars.
  add_pragma(
      {"HLS interface ap_none port =", param->getNameAsString(), "register"});
  AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS);
}

void XilinxHLSTarget::AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  assert(IsTapaType(param, "(i|o)streams?"));
  const auto name = param->getNameAsString();
  add_pragma({"HLS disaggregate variable =", name});

  std::vector<std::string> names;
  if (IsTapaType(param, "(i|o)streams")) {
    add_pragma({"HLS array_partition variable =", name, "complete"});
    const int64_t array_size = GetArraySize(param);
    names.reserve(array_size);
    for (int64_t i = 0; i < array_size; ++i) {
      names.push_back(ArrayNameAt(name, i));
    }
  } else {
    names.push_back(name);
  }

  for (const auto& name : names) {
    const auto fifo_var = GetFifoVar(name);
    add_pragma({"HLS interface ap_fifo port =", fifo_var});
    add_pragma({"HLS aggregate variable =", fifo_var, "bit"});
    if (IsTapaType(param, "istreams?")) {
      const auto peek_var = GetPeekVar(name);
      add_pragma({"HLS interface ap_fifo port =", peek_var});
      add_pragma({"HLS aggregate variable =", peek_var, "bit"});
      add_line("void(" + name + "._.empty());");
      add_line("void(" + name + "._peek.empty());");
    } else {
      add_line("void(" + name + "._.full());");
    }
  }
}

void XilinxHLSTarget::AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  if (IsTapaType(param, "async_mmaps")) {
    add_line("#error async_mmaps not supported for lower level tasks");
    return;
  }

  const auto name = param->getNameAsString();
  add_pragma({"HLS disaggregate variable =", name});
  for (auto tag : {".read_addr", ".read_data", ".write_addr", ".write_data",
                   ".write_resp"}) {
    const auto fifo_var = GetFifoVar(name + tag);
    add_pragma({"HLS interface ap_fifo port =", fifo_var});
    add_pragma({"HLS aggregate variable =", fifo_var, " bit"});
  }
  for (auto tag : {".read_data", ".write_resp"}) {
    add_pragma({"HLS disaggregate variable =", name, tag});
    const auto peek_var = GetPeekVar(name + tag);
    add_pragma({"HLS interface ap_fifo port =", peek_var});
    add_pragma({"HLS aggregate variable =", peek_var, "bit"});
  }
  add_line("void(" + name + ".read_addr._.full());");
  add_line("void(" + name + ".read_data._.empty());");
  add_line("void(" + name + ".read_data._peek.empty());");
  add_line("void(" + name + ".write_addr._.full());");
  add_line("void(" + name + ".write_data._.full());");
  add_line("void(" + name + ".write_resp._.empty());");
  add_line("void(" + name + ".write_resp._peek.empty());");
}

void XilinxHLSTarget::AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  if (IsTapaType(param, "mmaps")) {
    add_line("#error mmaps not supported for lower level tasks");
    return;
  }

  if (IsTapaType(param, "hmap")) {
    add_line("#error hmap not supported for lower level tasks");
    return;
  }

  const auto name = param->getNameAsString();
  add_pragma(
      {"HLS interface m_axi port =", name, "offset = direct bundle =", name});
}

void XilinxHLSTarget::RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  if (vitis_mode) {
    // We need a empty shell for the top-level function body so that TAPA
    // can use the shell to connect the subtasks.
    if (func->isThisDeclarationADefinition()) {
      auto lines = GenerateCodeForTopLevelFunc(func);
      rewriter.ReplaceText(func->getBody()->getSourceRange(),
                           "{\n" + llvm::join(lines, "\n") + "}\n");
    }

    // Vitis only works with extern C kernels. We need to wrap the kernel
    // with extern C.
    auto beginLoc = func->getBeginLoc();
    auto endLoc = func->getEndLoc();

    // If the function is a signature, we need to insert after the semicolon.
    if (!func->isThisDeclarationADefinition()) {
      endLoc = Lexer::findLocationAfterToken(endLoc, clang::tok::semi,
                                             rewriter.getSourceMgr(),
                                             rewriter.getLangOpts(), true);
    }

    rewriter.InsertText(beginLoc, "extern \"C\" {\n\n");
    rewriter.InsertTextAfterToken(endLoc, "\n\n}  // extern \"C\"\n");
  } else {
    // Otherwise, treat it as a normal HLS kernel.
    RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS);
  }
}

void XilinxHLSTarget::RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  auto lines = GenerateCodeForMiddleLevelFunc(func);
  rewriter.ReplaceText(func->getBody()->getSourceRange(),
                       "{\n" + llvm::join(lines, "\n") + "}\n");
}

static void RewriteStreamDefinitions(REWRITE_FUNC_ARGS_DEF) {
  for (const auto child : func->getBody()->children()) {
    if (const auto decl_stmt = llvm::dyn_cast<clang::DeclStmt>(child)) {
      if (const auto var_decl =
              llvm::dyn_cast<clang::VarDecl>(*decl_stmt->decl_begin())) {
        if (auto decl = GetTapaStreamDecl(var_decl->getType())) {
          const auto args = decl->getTemplateArgs().asArray();
          const uint64_t fifo_depth{*args[1].getAsIntegral().getRawData()};
          AddStreamDepthPragma(rewriter, decl_stmt, var_decl->getNameAsString(),
                               fifo_depth);
        }
      }
      // TODO: Support streams
    }
  }
}

void XilinxHLSTarget::RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  auto lines = GenerateCodeForLowerLevelFunc(func);
  rewriter.InsertTextAfterToken(func->getBody()->getBeginLoc(),
                                llvm::join(lines, "\n"));
  RewriteStreamDefinitions(REWRITE_FUNC_ARGS);
}

void XilinxHLSTarget::RewriteOtherFunc(REWRITE_FUNC_ARGS_DEF) {
  BaseTarget::RewriteOtherFunc(REWRITE_FUNC_ARGS);
  RewriteStreamDefinitions(REWRITE_FUNC_ARGS);
}

void XilinxHLSTarget::RewriteTopLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) {
  RewriteMiddleLevelFuncArguments(REWRITE_FUNC_ARGS);

  bool qdma_header_inserted = false;
  // Replace mmaps arguments with 64-bit base addresses.
  for (const auto param : func->parameters()) {
    if (IsTapaType(param, "(i|o)stream") && vitis_mode) {
      // For Vitis mode, replace istream and ostream with qdma_axis.
      // TODO: support streams
      int width =
          param->getASTContext()
              .getTypeInfo(GetTemplateArg(param->getType(), 0)->getAsType())
              .Width;
      rewriter.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "hls::stream<qdma_axis<" + std::to_string(width) + ", 0, 0, 0> >&");
      if (!qdma_header_inserted) {
        rewriter.InsertText(func->getBeginLoc(),
                            "#include \"ap_axi_sdata.h\"\n"
                            "#include \"hls_stream.h\"\n\n",
                            true);
        qdma_header_inserted = true;
      }
    }
  }
}

void XilinxHLSTarget::RewriteMiddleLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) {
  // Replace mmaps arguments with 64-bit base addresses.
  for (const auto param : func->parameters()) {
    const std::string param_name = param->getNameAsString();
    if (IsTapaType(param, "(async_)?mmap")) {
      rewriter.ReplaceText(
          param->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
          "uint64_t");
    } else if (IsTapaType(param, "((async_)?mmaps|hmap)")) {
      std::string rewritten_text;
      for (int i = 0; i < GetArraySize(param); ++i) {
        if (!rewritten_text.empty()) rewritten_text += ", ";
        rewritten_text += "uint64_t " + GetArrayElem(param_name, i);
      }
      rewriter.ReplaceText(param->getSourceRange(), rewritten_text);
    }
  }
}
void XilinxHLSTarget::ProcessNonCurrentTask(const clang::FunctionDecl* func,
                                            clang::Rewriter& rewriter,
                                            bool IsTopTapaTopLevel) {
  // Remove the function body.
  if (func->hasBody()) {
    auto range = func->getBody()->getSourceRange();
    rewriter.ReplaceText(range, ";");
  }
}
void XilinxHLSTarget::RewriteOtherFuncArguments(REWRITE_FUNC_ARGS_DEF) {}

void XilinxHLSTarget::RewritePipelinedDecl(REWRITE_DECL_ARGS_DEF,
                                           const clang::Stmt* body) {
  if (auto pipeline = llvm::dyn_cast<clang::TapaPipelineAttr>(attr)) {
    if (body) AddPipelinePragma(rewriter, pipeline, body);
  }
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));
}

void XilinxHLSTarget::RewritePipelinedStmt(REWRITE_STMT_ARGS_DEF,
                                           const clang::Stmt* body) {
  if (auto pipeline = llvm::dyn_cast<clang::TapaPipelineAttr>(attr)) {
    if (body) AddPipelinePragma(rewriter, pipeline, body);
  }
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));
}

void XilinxHLSTarget::RewriteUnrolledStmt(REWRITE_STMT_ARGS_DEF,
                                          const clang::Stmt* body) {
  if (auto unroll = llvm::dyn_cast<clang::TapaUnrollAttr>(attr)) {
    auto factor = unroll->getFactor();
    std::string pragma = "HLS unroll";
    if (factor) pragma += std::string(" factor = ") + std::to_string(factor);
    AddPragmaToBody(rewriter, body, pragma);
  }
  rewriter.RemoveText(ExtendAttrRemovalRange(rewriter, attr->getRange()));
}

}  // namespace internal
}  // namespace tapa
