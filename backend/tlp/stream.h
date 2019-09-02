#ifndef TLP_STREAM_H_
#define TLP_STREAM_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"

// Enum of different operations performed on a stream.
enum StreamOpEnum : uint64_t {
  kNotStreamOperation = 0,
  kIsDestructive = 1 << 0,  // Changes FIFO state.
  kIsNonBlocking = 1 << 1,
  kIsBlocking = 1 << 2,
  kIsDefaulted = 1 << 3,
  kIsProducer = 1 << 4,  // Must be performed on the producer side.
  kIsConsumer = 1 << 5,  // Must be performed on the consumer side.

  kTestEmpty = 1ULL << 32 | kIsConsumer,
  kTestEos = 2ULL << 32 | kIsConsumer,
  kTryPeek = 3ULL << 32 | kIsConsumer,
  kBlockingPeek = 4ULL << 32 | kIsBlocking | kIsConsumer,
  kNonBlockingPeek = 5ULL << 32 | kIsNonBlocking | kIsConsumer,
  kTryRead = 6ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kBlockingRead = 7ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
  kNonBlockingRead = 8ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kNonBlockingDefaultedRead =
      9ULL << 32 | kIsDestructive | kIsNonBlocking | kIsDefaulted | kIsConsumer,
  kTryOpen = 10ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kOpen = 11ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
  kTestFull = 12ULL << 32 | kIsConsumer,
  kTryWrite = 13ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
  kWrite = 14ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer,
  kTryClose = 15ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
  kClose = 16ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer
};

// Information about a particular stream used in a task.
struct StreamInfo {
  StreamInfo(const std::string& name, const std::string& type)
      : name{name}, type{type} {
    if (this->type.substr(0, 7) == "struct ") {
      this->type.erase(0, 7);
    } else if (this->type.substr(0, 6) == "class ") {
      this->type.erase(0, 6);
    }
  }
  // Name of the stream in the task.
  std::string name;
  // Type of the stream.
  std::string type;
  // List of operations performed.
  std::vector<StreamOpEnum> ops{};
  // List of AST nodes. Each of them corresponds to a stream operation.
  std::vector<const clang::CXXMemberCallExpr*> call_exprs{};
  bool is_producer{false};
  bool is_consumer{false};
  bool is_blocking{false};
  bool is_non_blocking{false};

  std::string ValueVar() const { return "tlp_" + name + "_value"; }
  std::string ValidVar() const { return "tlp_" + name + "_valid"; }
  static std::string ProceedVar() { return "tlp_proceed"; }
};

StreamOpEnum GetStreamOp(const clang::CXXMemberCallExpr* call_expr);

void GetStreamInfo(clang::Stmt* root, std::vector<StreamInfo>& streams,
                   std::shared_ptr<std::unordered_map<const clang::Stmt*, bool>>
                       visited = nullptr);

#endif  // TLP_STREAM_H_
