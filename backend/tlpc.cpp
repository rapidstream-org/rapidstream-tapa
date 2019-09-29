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

const char kUtilFuncs[] = R"(
#include <cstdint>

#ifndef __SYNTHESIS__

#include <thread>
#include <vector>

#include <glog/logging.h>

#define HLS_STREAM_THREAD_SAFE

#else  // __SYNTHESIS__

struct dummy {
   template<typename T>
   dummy& operator<<(const T&) { return *this; }
 };

 #define LOG(level) dummy()
 #define LOG_IF(level, cond) dummy()
 #define LOG_EVERY_N(level, n) dummy()
 #define LOG_IF_EVERY_N(level, cond, n) dummy()
 #define LOG_FIRST_N(level, n) dummy()

 #define DLOG(level) dummy()
 #define DLOG_IF(level, cond) dummy()
 #define DLOG_EVERY_N(level, n) dummy()

 #define CHECK(cond) assert(cond); dummy()
 #define CHECK_NE(lhs, rhs) assert((lhs) != (rhs)); dummy()
 #define CHECK_EQ(lhs, rhs) assert((lhs) != (rhs)); dummy()
 #define CHECK_NOTNULL(ptr) (ptr)
 #define CHECK_STREQ(lhs, rhs) dummy()
 #define CHECK_STRNE(lhs, rhs) dummy()
 #define CHECK_STRCASEEQ(lhs, rhs) dummy()
 #define CHECK_STRCASENE(lhs, rhs) dummy()
 #define CHECK_DOUBLE_EQ(lhs, rhs) dummy()

 #define VLOG_IS_ON(level) false
 #define VLOG(level) dummy()
 #define VLOG_IF(level, cond) dummy()
 #define VLOG_EVERY_N(level, n) dummy()
 #define VLOG_IF_EVERY_N(level, cond, n) dummy()
 #define VLOG_FIRST_N(level, n) dummy()

#endif  // __SYNTHESISI__

#include <hls_stream.h>

 namespace tlp{

template <typename T>
struct data_t {
  bool eos;
  T val;
};

template <typename T>
inline T read_fifo(data_t<T>& value, bool valid, bool* valid_ptr,
                    const T& def) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  if (valid_ptr) {
    *valid_ptr = valid;
  }
  return valid ? value.val : def;
}

template <typename T>
inline T try_read_fifo(hls::stream<data_t<T>>& fifo, data_t<T>& value,
                       bool& valid) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  T val = value.val;
  if (valid) {
    valid = fifo.read_nb(value);
  }
  return val;
}

template <typename T>
inline bool eos_fifo(hls::stream<data_t<T>>& fifo, data_t<T>& value,
                     bool& valid) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return valid ? value.eos : (valid = true, value = fifo.read()).eos;
}

template <typename T>
inline T read_fifo(hls::stream<data_t<T>>& fifo, data_t<T>& value,
                   bool& valid) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  auto val = value.val;
  return valid ? (valid = fifo.read_nb(value), val) : fifo.read().val;
}

template <typename T>
inline void write_fifo(hls::stream<data_t<T>>& fifo, const T& value) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  fifo.write({false, value});
}

template <typename T>
inline void close_fifo(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  fifo.write({true, {}});
}

}  // namespace tlp

)";

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
    const regex pattern{R"(#\s*include\s*(<\s*tlp\.h\s*>|"\s*tlp\.h\s*"))"};
    *output = regex_replace(*output, pattern, kUtilFuncs);
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
  if (ret == 0) {
    llvm::outs() << *output;
  }
  delete output;
  return ret;
}
