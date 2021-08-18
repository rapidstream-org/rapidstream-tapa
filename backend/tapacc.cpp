#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/Support/raw_ostream.h"

#include "nlohmann/json.hpp"

#include "tapa/task.h"

using std::make_shared;
using std::regex;
using std::regex_match;
using std::regex_replace;
using std::shared_ptr;
using std::stoi;
using std::string;
using std::tie;
using std::tuple;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

using clang::ASTConsumer;
using clang::ASTContext;
using clang::ASTFrontendAction;
using clang::CompilerInstance;
using clang::FunctionDecl;
using clang::Rewriter;
using clang::StringRef;
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

class Consumer : public ASTConsumer {
 public:
  explicit Consumer(ASTContext& context, vector<const FunctionDecl*>& funcs)
      : visitor_{context, funcs, rewriters_, metadata_}, funcs_{funcs} {}
  void HandleTranslationUnit(ASTContext& context) override {
    // First pass traversal extracts all global functions as potential tasks.
    // this->funcs_ stores all potential tasks.
    visitor_.TraverseDecl(context.getTranslationUnitDecl());

    // Look for the top-level task. Starting from there, find all tasks using
    // DFS.
    // Note that task functions cannot share the same name.
    auto& diagnostics_engine = context.getDiagnostics();
    if (top_name == nullptr) {
      static const auto diagnostic_id = diagnostics_engine.getCustomDiagID(
          clang::DiagnosticsEngine::Fatal, "top not set");
      diagnostics_engine.Report(diagnostic_id);
    }
    unordered_map<string, vector<const FunctionDecl*>> func_table;
    for (auto func : funcs_) {
      auto func_name = func->getNameAsString();
      // If a key exists, its corresponding value won't be empty.
      func_table[func_name].push_back(func);
    }
    static const auto task_redefined = diagnostics_engine.getCustomDiagID(
        clang::DiagnosticsEngine::Error, "task '%0' re-defined");
    static const auto task_defined_here = diagnostics_engine.getCustomDiagID(
        clang::DiagnosticsEngine::Note, "task '%0' defined here");
    auto report_task_redefinition =
        [&](const vector<const FunctionDecl*>& decls) {
          if (decls.size() > 1) {
            {
              auto diagnostics_builder =
                  diagnostics_engine.Report(task_redefined);
              diagnostics_builder.AddString(decls[0]->getNameAsString());
            }
            /*
            for (auto decl : decls) {
              auto diagnostics_builder = diagnostics_engine.Report(
                  decl->getBeginLoc(), task_defined_here);
              diagnostics_builder.AddSourceRange(
                  CharSourceRange::getCharRange(decl->getSourceRange()));
              diagnostics_builder.AddString(decl->getNameAsString());
            }
            */
          }
        };
    if (func_table.count(*top_name)) {
      auto tasks = FindAllTasks(func_table[*top_name][0]);
      funcs_.clear();
      for (auto task : tasks) {
        auto task_name = task->getNameAsString();
        report_task_redefinition(func_table[task_name]);
        funcs_.push_back(func_table[task_name][0]);
        rewriters_[task] =
            Rewriter(context.getSourceManager(), context.getLangOpts());
      }
    } else {
      static const auto top_not_found = diagnostics_engine.getCustomDiagID(
          clang::DiagnosticsEngine::Fatal, "top-level task '%0' not found");
      auto diagnostics_builder = diagnostics_engine.Report(top_not_found);
      diagnostics_builder.AddString(*top_name);
    }

    // funcs_ has been reset to only contain the tasks.
    // Traverse the AST for each task and obtain the transformed source code.
    for (auto task : funcs_) {
      visitor_.VisitTask(task);
    }
    unordered_map<const FunctionDecl*, string> code_table;
    json code;
    for (auto task : funcs_) {
      auto task_name = task->getNameAsString();
      raw_string_ostream oss{code_table[task]};
      rewriters_[task]
          .getEditBuffer(rewriters_[task].getSourceMgr().getMainFileID())
          .write(oss);
      oss.flush();
      code["tasks"][task_name]["code"] = code_table[task];
      bool is_upper = GetTapaTask(task->getBody()) != nullptr;
      code["tasks"][task_name]["level"] = is_upper ? "upper" : "lower";
      code["tasks"][task_name].update(metadata_[task]);
    }
    code["top"] = *top_name;
    std::cout << code;
  }

 private:
  Visitor visitor_;
  vector<const FunctionDecl*>& funcs_;
  unordered_map<const FunctionDecl*, Rewriter> rewriters_;
  unordered_map<const FunctionDecl*, json> metadata_;
};

class Action : public ASTFrontendAction {
 public:
  unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
                                            StringRef file) override {
    return llvm::make_unique<Consumer>(compiler.getASTContext(), funcs_);
  }

 private:
  vector<const FunctionDecl*> funcs_;
};

}  // namespace internal
}  // namespace tapa

static OptionCategory tapa_option_category("TAPA Compiler Companion");
static llvm::cl::opt<string> tapa_opt_top_name(
    "top", NumOccurrencesFlag::Required, ValueExpected::ValueRequired,
    llvm::cl::desc("Top-level task name"), llvm::cl::cat(tapa_option_category));

int main(int argc, const char** argv) {
  CommonOptionsParser parser{argc, argv, tapa_option_category};
  ClangTool tool{parser.getCompilations(), parser.getSourcePathList()};
  string top_name{tapa_opt_top_name.getValue()};
  tapa::internal::top_name = &top_name;
  int ret = tool.run(newFrontendActionFactory<tapa::internal::Action>().get());
  return ret;
}
