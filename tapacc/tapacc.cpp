// Copyright (c) 2024 RapidStream Design Automation, Inc. ane contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <tuple>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/Support/raw_ostream.h"

#include "nlohmann/json.hpp"

#include "rewriter/task.h"

using std::make_shared;
using std::map;
using std::regex;
using std::regex_match;
using std::regex_replace;
using std::set;
using std::shared_ptr;
using std::stoi;
using std::string;
using std::tie;
using std::tuple;
using std::unique_ptr;
using std::vector;

using clang::ASTConsumer;
using clang::ASTContext;
using clang::ASTFrontendAction;
using clang::CompilerInstance;
using clang::FunctionDecl;
using clang::Rewriter;
using clang::StringRef;
using clang::TapaTargetAttr;
using clang::tooling::ClangTool;
using clang::tooling::CommonOptionsParser;
using clang::tooling::newFrontendActionFactory;

using llvm::raw_string_ostream;
using llvm::cl::NumOccurrencesFlag;
using llvm::cl::OptionCategory;
using llvm::cl::ValueExpected;

using nlohmann::json;

namespace tapa {
namespace internal {

const string* top_name;

TapaTargetAttr::TargetType target = TapaTargetAttr::TargetType::XilinxHLS;

class Consumer : public ASTConsumer {
 public:
  explicit Consumer(ASTContext& context, vector<const FunctionDecl*>& funcs,
                    set<TapaTask>& tapa_tasks)
      : visitor_{context, funcs, tapa_tasks, rewriters_, metadata_},
        funcs_{funcs},
        tapa_tasks_{tapa_tasks} {}
  void HandleTranslationUnit(ASTContext& context) override {
    // First pass traversal extracts all global functions into this->funcs_.
    visitor_.is_first_traversal = true;
    visitor_.TraverseDecl(context.getTranslationUnitDecl());
    visitor_.is_first_traversal = false;

    // Utilities to show diagnostics.
    auto& diagnostics_engine = context.getDiagnostics();
    if (top_name == nullptr) {
      static const auto diagnostic_id = diagnostics_engine.getCustomDiagID(
          clang::DiagnosticsEngine::Fatal, "top not set");
      diagnostics_engine.Report(diagnostic_id);
    }
    map<string, vector<const FunctionDecl*>> func_table;
    for (auto func : funcs_) {
      auto func_name = func->getNameAsString();
      // skip function definitions.
      if (!func->isThisDeclarationADefinition()) continue;
      // If a key exists, its corresponding value won't be empty.
      func_table[func_name].push_back(func);
    }
    static const auto task_redefined = diagnostics_engine.getCustomDiagID(
        clang::DiagnosticsEngine::Error, "task '%0' re-defined");
    auto report_task_redefinition =
        [&](const vector<const FunctionDecl*>& decls) {
          if (decls.size() > 1) {
            auto diagnostics_builder =
                diagnostics_engine.Report(task_redefined);
            diagnostics_builder.AddString(decls[0]->getNameAsString());
          }
        };

    // Look for the top-level task. Starting from there, find all tasks using
    // DFS. Note that task functions cannot share the same name.
    if (func_table.count(*top_name)) {
      auto tasks = FindAllTasks(func_table[*top_name][0]);
      tapa_tasks_.clear();
      for (auto task : tasks) {
        auto task_name = task.func->getNameAsString();
        report_task_redefinition(func_table[task_name]);
        tapa_tasks_.insert(task);
      }
    } else {
      static const auto top_not_found = diagnostics_engine.getCustomDiagID(
          clang::DiagnosticsEngine::Fatal, "top-level task '%0' not found");
      auto diagnostics_builder = diagnostics_engine.Report(top_not_found);
      diagnostics_builder.AddString(*top_name);
    }

    // Create rewriters for all tasks
    for (auto task : tapa_tasks_) {
      rewriters_[task] =
          Rewriter(context.getSourceManager(), context.getLangOpts());
    }

    // tapa_tasks has been reset to only contain the TAPA tasks from the top
    // level task's call graph. Now we can traverse the AST again to extract
    // metadata and rewrite the source code.
    for (auto task : tapa_tasks_) {
      visitor_.VisitTask(task);
    }
    map<TapaTask, string> code_table;
    json code;
    for (auto task : tapa_tasks_) {
      auto task_name = task.func->getNameAsString();
      auto readable_name = task_name;
      // For template specializations, use the mangled name
      if (task.func->isFunctionTemplateSpecialization()) {
        task_name = visitor_.GetMangledFuncName(task.func);
        readable_name = visitor_.GetTemplatedFuncName(task.func);
      }
      raw_string_ostream oss{code_table[task]};
      rewriters_[task]
          .getEditBuffer(rewriters_[task].getSourceMgr().getMainFileID())
          .write(oss);
      oss.flush();
      code["tasks"][task_name]["readable_name"] = readable_name;
      code["tasks"][task_name]["code"] = code_table[task];
      // if a task is ignored, it is a lower-level task.
      bool is_upper = GetTapaTaskObjectExpr(task.func->getBody()) != nullptr &&
                      !IsFuncIgnored(task.func);
      code["tasks"][task_name]["level"] = is_upper ? "upper" : "lower";
      code["tasks"][task_name].update(metadata_[task]);
    }
    code["top"] = *top_name;
    std::cout << code;
  }

 private:
  Visitor visitor_;
  vector<const FunctionDecl*>& funcs_;
  set<TapaTask>& tapa_tasks_;
  map<TapaTask, Rewriter> rewriters_;
  map<TapaTask, json> metadata_;
};

class Action : public ASTFrontendAction {
 public:
  unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
                                            StringRef file) override {
    return std::make_unique<Consumer>(compiler.getASTContext(), funcs_,
                                      tapa_tasks_);
  }

 private:
  vector<const FunctionDecl*> funcs_;
  set<TapaTask> tapa_tasks_;
};

}  // namespace internal
}  // namespace tapa

static OptionCategory tapa_option_category("TAPA Compiler Companion");
static llvm::cl::opt<string> tapa_opt_top_name(
    "top", NumOccurrencesFlag::Required, ValueExpected::ValueRequired,
    llvm::cl::desc("Top-level task name"), llvm::cl::cat(tapa_option_category));
static llvm::cl::opt<TapaTargetAttr::TargetType> tapa_opt_target(
    "target", llvm::cl::desc("The target type, default to HLS"),
    llvm::cl::cat(tapa_option_category),
    llvm::cl::values(
        llvm::cl::OptionEnumValue{"xilinx-aie",
                                  TapaTargetAttr::TargetType::XilinxAIE,
                                  "Xilina AI Engine target"},
        llvm::cl::OptionEnumValue{"xilinx-hls",
                                  TapaTargetAttr::TargetType::XilinxHLS,
                                  "Xilinx HLS target (default)"},
        llvm::cl::OptionEnumValue{"xilinx-vitis",
                                  TapaTargetAttr::TargetType::XilinxVitis,
                                  "Xilinx Vitis target"}));

int main(int argc, const char** argv) {
  auto expected_parser =
      CommonOptionsParser::create(argc, argv, tapa_option_category);
  auto& parser = expected_parser.get();
  ClangTool tool{parser.getCompilations(), parser.getSourcePathList()};

  string top_name{tapa_opt_top_name.getValue()};
  tapa::internal::top_name = &top_name;

  if (tapa_opt_target.getNumOccurrences() > 0) {
    tapa::internal::target = tapa_opt_target.getValue();
  }

  int ret = tool.run(newFrontendActionFactory<tapa::internal::Action>().get());
  return ret;
}
