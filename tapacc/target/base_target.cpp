// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "base_target.h"

#include "../rewriter/mmap.h"
#include "../rewriter/stream.h"
#include "../rewriter/type.h"

namespace tapa {
namespace internal {

void BaseTarget::AddCodeForTopLevelFunc(ADD_FOR_FUNC_ARGS_DEF) {}

void BaseTarget::AddCodeForStream(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForStream(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForStream(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForStream(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForOtherStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForStream(ADD_FOR_PARAMS_ARGS);
}

void BaseTarget::AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForOtherAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS);
}

void BaseTarget::AddCodeForMmap(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForOtherMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMmap(ADD_FOR_PARAMS_ARGS);
}

void BaseTarget::AddCodeForScalar(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForScalar(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForScalar(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForScalar(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForOtherScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForScalar(ADD_FOR_PARAMS_ARGS);
}

#define LINES_FUNCTIONS                                                     \
  auto add_line = [&lines](llvm::StringRef line) {                          \
    lines.push_back(line.str());                                            \
  };                                                                        \
  auto add_pragma = [&lines](std::initializer_list<llvm::StringRef> args) { \
    lines.push_back("#pragma " + llvm::join(args, " "));                    \
  };

std::vector<std::string> BaseTarget::GenerateCodeForTopLevelFunc(
    const clang::FunctionDecl* func) {
  std::vector<std::string> lines = {""};
  LINES_FUNCTIONS;

  for (const auto param : func->parameters()) {
    if (IsTapaType(param, "(i|o)streams?")) {
      AddCodeForTopLevelStream(param, add_line, add_pragma);
    } else if (IsTapaType(param, "async_mmaps?")) {
      AddCodeForTopLevelAsyncMmap(param, add_line, add_pragma);
    } else if (IsTapaType(param, "(mmaps?|hmap)")) {
      AddCodeForTopLevelMmap(param, add_line, add_pragma);
    } else {
      AddCodeForTopLevelScalar(param, add_line, add_pragma);
    }
    add_line("");  // Separate each parameter.
  }

  add_line("");
  AddCodeForTopLevelFunc(func, add_line, add_pragma);

  return lines;
}

std::vector<std::string> BaseTarget::GenerateCodeForMiddleLevelFunc(
    const clang::FunctionDecl* func) {
  std::vector<std::string> lines = {""};
  LINES_FUNCTIONS;

  for (const auto param : func->parameters()) {
    if (IsTapaType(param, "(i|o)streams?")) {
      AddCodeForMiddleLevelStream(param, add_line, add_pragma);
    } else if (IsTapaType(param, "async_mmaps?")) {
      AddCodeForMiddleLevelAsyncMmap(param, add_line, add_pragma);
    } else if (IsTapaType(param, "(mmaps?|hmap)")) {
      AddCodeForMiddleLevelMmap(param, add_line, add_pragma);
    } else {
      AddCodeForMiddleLevelScalar(param, add_line, add_pragma);
    }
    add_line("");  // Separate each parameter.
  }

  return lines;
}

std::vector<std::string> BaseTarget::GenerateCodeForLowerLevelFunc(
    const clang::FunctionDecl* func) {
  std::vector<std::string> lines = {""};
  LINES_FUNCTIONS;

  for (const auto param : func->parameters()) {
    if (IsTapaType(param, "(i|o)streams?")) {
      AddCodeForLowerLevelStream(param, add_line, add_pragma);
    } else if (IsTapaType(param, "async_mmaps?")) {
      AddCodeForLowerLevelAsyncMmap(param, add_line, add_pragma);
    } else if (IsTapaType(param, "(mmaps?|hmap)")) {
      AddCodeForLowerLevelMmap(param, add_line, add_pragma);
    } else {
      AddCodeForLowerLevelScalar(param, add_line, add_pragma);
    }
    add_line("");  // Separate each parameter.
  }

  return lines;
}

std::vector<std::string> BaseTarget::GenerateCodeForOtherFunc(
    const clang::FunctionDecl* func) {
  std::vector<std::string> lines = {""};
  LINES_FUNCTIONS;

  for (const auto param : func->parameters()) {
    if (IsTapaType(param, "(i|o)streams?")) {
      AddCodeForOtherStream(param, add_line, add_pragma);
    } else if (IsTapaType(param, "async_mmaps?")) {
      AddCodeForOtherAsyncMmap(param, add_line, add_pragma);
    } else if (IsTapaType(param, "(mmaps?|hmap)")) {
      AddCodeForOtherMmap(param, add_line, add_pragma);
    } else {
      AddCodeForOtherScalar(param, add_line, add_pragma);
    }
    add_line("");  // Separate each parameter.
  }

  return lines;
}

clang::SourceRange BaseTarget::ExtendAttrRemovalRange(
    clang::Rewriter& rewriter, clang::SourceRange range) {
  auto begin = range.getBegin();
  auto end = range.getEnd();

#define BEGIN(OFF) (begin.getLocWithOffset(OFF))
#define END(OFF) (end.getLocWithOffset(OFF))
#define STR_AT(BEGIN, END) \
  (rewriter.getRewrittenText(clang::SourceRange((BEGIN), (END))))
#define IS_IGNORE(STR) ((STR) == "" || std::isspace((STR)[0]))

  // Find the true end of the token
  for (; std::isalpha(STR_AT(END(1), END(1))[0]); end = END(1));

  // Remove all whitespaces around the attribute
  for (; IS_IGNORE(STR_AT(BEGIN(-1), BEGIN(-1))); begin = BEGIN(-1));
  for (; IS_IGNORE(STR_AT(END(1), END(1))); end = END(1));

  // Remove comma if around the attribute
  if (STR_AT(BEGIN(-1), BEGIN(-1)) == ",") {
    begin = BEGIN(-1);
  } else if (STR_AT(END(1), END(1)) == ",") {
    end = END(1);
  } else if (STR_AT(BEGIN(-2), BEGIN(-1)) == "[[" &&
             STR_AT(END(1), END(2)) == "]]") {
    // Check if the attribute is completely removed
    begin = BEGIN(-2);
    end = END(2);
  }

  return clang::SourceRange(begin, end);
}

void BaseTarget::RewriteTopLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  auto lines = GenerateCodeForTopLevelFunc(func);
  rewriter.InsertTextAfterToken(func->getBody()->getBeginLoc(),
                                llvm::join(lines, "\n"));
}
void BaseTarget::RewriteMiddleLevelFunc(REWRITE_FUNC_ARGS_DEF) {
  auto lines = GenerateCodeForMiddleLevelFunc(func);
  rewriter.InsertTextAfterToken(func->getBody()->getBeginLoc(),
                                llvm::join(lines, "\n"));
}
void BaseTarget::RewriteLowerLevelFunc(REWRITE_FUNC_ARGS_DEF) {}
void BaseTarget::RewriteOtherFunc(REWRITE_FUNC_ARGS_DEF) {
  auto lines = GenerateCodeForOtherFunc(func);
  rewriter.InsertTextAfterToken(func->getBody()->getBeginLoc(),
                                llvm::join(lines, "\n"));
}

void BaseTarget::RewriteTopLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) {}
void BaseTarget::RewriteMiddleLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) {}
void BaseTarget::RewriteLowerLevelFuncArguments(REWRITE_FUNC_ARGS_DEF) {}
void BaseTarget::RewriteOtherFuncArguments(REWRITE_FUNC_ARGS_DEF) {}
void BaseTarget::ProcessNonCurrentTask(REWRITE_FUNC_ARGS_DEF,
                                       bool IsTopTapaTopLevel) {}
void BaseTarget::RewritePipelinedDecl(REWRITE_DECL_ARGS_DEF,
                                      const clang::Stmt* body) {}
void BaseTarget::RewritePipelinedStmt(REWRITE_STMT_ARGS_DEF,
                                      const clang::Stmt* body) {}

void BaseTarget::RewriteUnrolledDecl(REWRITE_DECL_ARGS_DEF,
                                     const clang::Stmt* body) {}
void BaseTarget::RewriteUnrolledStmt(REWRITE_STMT_ARGS_DEF,
                                     const clang::Stmt* body) {}

// Add dummy read and write operations for streams, mmap, and scalar, so that
// the function arguments will not be optimized out by the vendor compilers.
void AddDummyStreamRW(ADD_FOR_PARAMS_ARGS_DEF, bool qdma) {
  auto param_name = param->getNameAsString();
  auto add_dummy_read = [&add_line, qdma](std::string name) {
    add_line("{ auto val = " + name + ".read(); }");
    if (!qdma) {
      // For non-qdma streams, peeks are supported and we need to make sure
      // it still exists.
      add_line("{ auto val = " + name + ".peek(nullptr); }");
    }
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
      for (size_t i = 0; i < GetArraySize(param); ++i) {
        add_dummy_read(ArrayNameAt(param_name, i));
      }
    }

  } else if (IsTapaType(param, "ostreams")) {
    if (qdma) {
      add_line("#error ostreams not supported for qdma-based tasks");
    } else {
      for (size_t i = 0; i < GetArraySize(param); ++i) {
        add_dummy_write(ArrayNameAt(param_name, i), GetStreamElemType(param));
      }
    }
  }
}

void AddDummyMmapOrScalarRW(ADD_FOR_PARAMS_ARGS_DEF) {
  auto param_name = param->getNameAsString();
  if (IsTapaType(param, "((async_)?mmaps|hmap)")) {
    for (size_t i = 0; i < GetArraySize(param); ++i) {
      add_line("{ auto val = reinterpret_cast<volatile uint8_t&>(" +
               GetArrayElem(param_name, i) + "); }");
    }
  } else {
    if (IsTapaType(param, "mmap")) param_name += "_offset";
    auto elem_type = param->getType();
    const bool is_const = elem_type.isConstQualified();
    add_line(std::string("{ auto val = reinterpret_cast<volatile ") +
             (is_const ? "const " : "") + "uint8_t&>(" + param_name + "); }");
  }
}

void AddPragmaToBody(clang::Rewriter& rewriter, const clang::Stmt* body,
                     std::string pragma) {
  if (auto compound = llvm::dyn_cast<clang::CompoundStmt>(body)) {
    rewriter.InsertTextAfterToken(compound->getLBracLoc(),
                                  std::string("\n#pragma ") + pragma + "\n");
  } else {
    rewriter.InsertTextBefore(body->getBeginLoc(),
                              std::string("_Pragma(\"") + pragma + "\")");
  }
}

void AddPragmaAfterStmt(clang::Rewriter& rewriter, const clang::Stmt* stmt,
                        std::string pragma) {
  rewriter.InsertTextAfterToken(stmt->getEndLoc(),
                                std::string("\n#pragma ") + pragma + "\n");
}

void AddPipelinePragma(clang::Rewriter& rewriter,
                       const clang::TapaPipelineAttr* attr,
                       const clang::Stmt* body) {
  auto II = attr->getII();
  std::string pragma = "HLS pipeline";
  if (II) pragma += std::string(" II = ") + std::to_string(II);
  AddPragmaToBody(rewriter, body, pragma);
}

void AddStreamDepthPragma(clang::Rewriter& rewriter, const clang::Stmt* stmt,
                          std::string name, int depth) {
  std::string pragma = "HLS STREAM";
  pragma += " variable = " + name + "._";
  pragma += " depth = " + std::to_string(depth);
  AddPragmaAfterStmt(rewriter, stmt, pragma);
}

}  // namespace internal
}  // namespace tapa
