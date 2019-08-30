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

#include "tlp/task.h"

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
using clang::Rewriter;
using clang::StringRef;
using clang::tooling::ClangTool;
using clang::tooling::CommonOptionsParser;
using clang::tooling::newFrontendActionFactory;

using llvm::make_unique;
using llvm::raw_string_ostream;
using llvm::cl::OptionCategory;

class TlpConsumer : public ASTConsumer {
 public:
  explicit TlpConsumer(ASTContext& context, Rewriter& rewriter)
      : visitor_{context, rewriter} {}
  void HandleTranslationUnit(ASTContext& context) override {
    visitor_.TraverseDecl(context.getTranslationUnitDecl());
  }

 private:
  TlpVisitor visitor_;
};

// Carries the rewritten code.
static string* output;

class TlpAction : public ASTFrontendAction {
 public:
  unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler,
                                            StringRef file) override {
    rewriter_.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
    return make_unique<TlpConsumer>(compiler.getASTContext(), rewriter_);
  }

  void EndSourceFileAction() override {
    raw_string_ostream os{*output};
    rewriter_.getEditBuffer(rewriter_.getSourceMgr().getMainFileID()).write(os);
    os.flush();
    // Instead of #include <tlp.h>, #include <hls_stream.h>.
    const regex pattern{R"(#include\s*(<\s*tlp\.h\s*>|"\s*tlp\.h\s*"))"};
    *output = regex_replace(*output, pattern, "#include <hls_stream.h>");
  }

 private:
  Rewriter rewriter_;
};

static OptionCategory TaskLevelParallelization(
    "Task-Level Parallelization for HLS");

int main(int argc, const char** argv) {
  output = new string{};
  CommonOptionsParser parser{argc, argv, TaskLevelParallelization};
  ClangTool tool{parser.getCompilations(), parser.getSourcePathList()};
  Rewriter rewriter;
  int ret = tool.run(newFrontendActionFactory<TlpAction>().get());
  llvm::outs() << *output;
  delete output;
  return ret;
}
