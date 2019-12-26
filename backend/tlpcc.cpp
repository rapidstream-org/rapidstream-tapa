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

#endif  // __SYNTHESIS__

#include <ap_utils.h>
#include <hls_stream.h>

 namespace tlp{

template <typename T>
struct data_t {
  bool eos;
  T val;
};

// tlp::stream<T>::try_eos(bool&)
template <typename T>
inline bool try_eos(hls::stream<data_t<T>>& fifo, data_t<T> peek_val,
                    bool& eos) {
#pragma HLS inline
#pragma HLS protocol
  return fifo.empty() ? false : (eos = peek_val.eos, true);
}

// tlp::stream<T>::eos()
template <typename T>
inline bool eos(hls::stream<data_t<T>>& peek_fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return peek_fifo.read().eos;
}

// tlp::stream<T>::eos(bool&)
template <typename T>
inline bool eos(hls::stream<data_t<T>>& fifo, data_t<T> peek_val, bool& valid) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return fifo.empty() ? (valid = false) : (valid = true, peek_val.eos);
}

// tlp::stream<T>::try_peek(T&)
template <typename T>
inline bool try_peek(hls::stream<data_t<T>>& fifo, data_t<T> peek_val,
                     T& val) {
#pragma HLS inline
#pragma HLS protocol
  return fifo.empty() ? false : (val = peek_val.val, true);
}

// tlp::stream<T>::peek()
template <typename T>
inline T peek(hls::stream<T>& peek_fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return peek_fifo.read();
}
template <typename T>
inline T peek(hls::stream<data_t<T>>& peek_fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return peek_fifo.read().val;
}

// tlp::stream<T>::peek(bool&)
template <typename T>
inline T peek(hls::stream<T>& fifo, T peek_val, bool& succeeded) {
#pragma HLS inline
#pragma HLS protocol
  T tmp;
  (succeeded = !fifo.empty()) && (tmp = peek_val);
  return tmp;
}
template <typename T>
inline T peek(hls::stream<data_t<T>>& fifo, data_t<T> peek_val,
              bool& succeeded) {
#pragma HLS inline
#pragma HLS protocol
  data_t<T> tmp;
  (succeeded = !fifo.empty()) && (tmp = peek_val);
  return tmp.val;
}

// tlp::stream<T>::try_read(T&)
template <typename T>
inline bool try_read(hls::stream<T>& fifo, T& value) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return fifo.read_nb(value);
}

// tlp::stream<T>::read()
template <typename T>
inline T read(hls::stream<T>& fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return fifo.read();
}
template <typename T>
inline T read(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  return fifo.read().val;
}

// tlp::stream<T>::read(bool&)
template <typename T>
inline T read(hls::stream<T>& fifo, bool& succeeded) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  T value;
  succeeded = fifo.read_nb(value);
  return value;
}
template <typename T>
inline T read(hls::stream<data_t<T>>& fifo, bool& succeeded) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  data_t<T> value;
  succeeded = fifo.read_nb(value);
  return value.val;
}

// tlp::stream<T>::open()
template <typename T>
inline void open(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  assert(fifo.read().eos);
}

// tlp::stream<T>::write(const T&)
template <typename T>
inline void write(hls::stream<data_t<T>>& fifo, const T& value) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  fifo.write({false, value});
}

// tlp::stream<T>::close()
template <typename T>
inline void close(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
#pragma HLS latency min = 1 max = 1
  fifo.write({true, {}});
}

}  // namespace tlp

#define TLP_WHILE_NOT_EOS(fifo)                                  \
  for (bool tlp_##fifo##_valid;                                  \
       !tlp::eos(fifo, tlp_##fifo##_peek, tlp_##fifo##_valid) || \
       !tlp_##fifo##_valid;)                                     \
    if (tlp_##fifo##_valid)

#define TLP_WHILE_NEITHER_EOS(fifo1, fifo2)                          \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid;                \
       (!tlp::eos(fifo1, tlp_##fifo1##_peek, tlp_##fifo1##_valid) || \
        !tlp_##fifo1##_valid) &&                                     \
       (!tlp::eos(fifo2, tlp_##fifo2##_peek, tlp_##fifo2##_valid) || \
        !tlp_##fifo2##_valid);)                                      \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid)

#define TLP_WHILE_NONE_EOS(fifo1, fifo2, fifo3)                            \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid, tlp_##fifo3##_valid; \
       (!tlp::eos(fifo1, tlp_##fifo1##_peek, tlp_##fifo1##_valid) ||       \
        !tlp_##fifo1##_valid) &&                                           \
       (!tlp::eos(fifo2, tlp_##fifo2##_peek, tlp_##fifo2##_valid) ||       \
        !tlp_##fifo2##_valid) &&                                           \
       (!tlp::eos(fifo3, tlp_##fifo3##_peek, tlp_##fifo3##_valid) ||       \
        !tlp_##fifo3##_valid);)                                            \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid && tlp_##fifo3##_valid)

)";

namespace tlp {
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
      Visitor::current_task = task;
      visitor_.TraverseDecl(context.getTranslationUnitDecl());
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
      // Remove inclusion of tlp.h and insert utilty functions.
      const regex pattern{R"(#\s*include\s*(<\s*tlp\.h\s*>|"\s*tlp\.h\s*"))"};
      code_table[task] = regex_replace(code_table[task], pattern, kUtilFuncs);
      code["tasks"][task_name]["code"] = code_table[task];
      bool is_upper = GetTlpTask(task->getBody()) != nullptr;
      code["tasks"][task_name]["level"] = is_upper ? "upper" : "lower";
      if (is_upper) {
        code["tasks"][task_name].update(metadata_[task]);
      }
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
}  // namespace tlp

static OptionCategory tlp_option_category("Task-Level Parallelization for HLS");
static llvm::cl::opt<string> tlp_opt_top_name(
    "top", NumOccurrencesFlag::Required, ValueExpected::ValueRequired,
    llvm::cl::desc("Top-level task name"), llvm::cl::cat(tlp_option_category));

int main(int argc, const char** argv) {
  CommonOptionsParser parser{argc, argv, tlp_option_category};
  ClangTool tool{parser.getCompilations(), parser.getSourcePathList()};
  string top_name{tlp_opt_top_name.getValue()};
  tlp::internal::top_name = &top_name;
  int ret = tool.run(newFrontendActionFactory<tlp::internal::Action>().get());
  return ret;
}
