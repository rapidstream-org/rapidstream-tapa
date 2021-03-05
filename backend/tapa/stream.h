#ifndef TAPA_STREAM_H_
#define TAPA_STREAM_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "type.h"

// Enum of different operations performed on a stream.
enum class StreamOpEnum : uint64_t {
  kNotStreamOperation = 0,
  kIsDestructive = 1 << 0,  // Changes FIFO state.
  kIsNonBlocking = 1 << 1,
  kIsBlocking = 1 << 2,
  kIsDefaulted = 1 << 3,
  kIsProducer = 1 << 4,  // Must be performed on the producer side.
  kIsConsumer = 1 << 5,  // Must be performed on the consumer side.
  kNeedPeeking = 1 << 6,
  kNeedEos = 1 << 7,

  kTestEmpty = 1ULL << 32 | kIsConsumer,
  kTryEos = 2ULL << 32 | kIsConsumer | kIsNonBlocking | kNeedPeeking | kNeedEos,
  kBlockingEos =
      3ULL << 32 | kIsConsumer | kIsBlocking | kNeedPeeking | kNeedEos,
  kNonBlockingEos =
      4ULL << 32 | kIsConsumer | kIsNonBlocking | kNeedPeeking | kNeedEos,
  kTryPeek = 5ULL << 32 | kIsConsumer | kIsNonBlocking | kNeedPeeking,
  kNonBlockingPeek = 6ULL << 32 | kIsNonBlocking | kIsConsumer | kNeedPeeking,
  kNonBlockingPeekWithEos =
      7ULL << 32 | kIsNonBlocking | kIsConsumer | kNeedPeeking | kNeedEos,
  kTryRead = 8ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kBlockingRead = 9ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
  kNonBlockingRead =
      10ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kNonBlockingDefaultedRead = 11ULL << 32 | kIsDestructive | kIsNonBlocking |
                              kIsDefaulted | kIsConsumer,
  kTryOpen = 12ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kOpen = 13ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
  kTestFull = 14ULL << 32 | kIsProducer,
  kTryWrite = 15ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
  kWrite = 16ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer,
  kTryClose = 17ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
  kClose = 18ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer
};

// Information about a particular stream used in a task.
struct StreamInfo : public ObjectInfo {
  StreamInfo(const std::string& name, const std::string& type, bool is_producer,
             bool is_consumer)
      : ObjectInfo(name, type),
        is_producer(is_producer),
        is_consumer(is_consumer) {}
  // List of operations performed.
  std::vector<StreamOpEnum> ops{};
  // List of AST nodes. Each of them corresponds to a stream operation.
  std::vector<const clang::CXXMemberCallExpr*> call_exprs{};
  bool is_producer{false};
  bool is_consumer{false};
  bool is_blocking{false};
  bool is_non_blocking{false};
  bool need_peeking{false};
  bool need_eos{false};

  std::string PeekVar() const { return name + ".peek_val"; }
  std::string FifoVar() const { return name + ".fifo"; }
  static std::string ProceedVar() { return "tapa_proceed"; }
};

StreamOpEnum GetStreamOp(const clang::CXXMemberCallExpr* call_expr);

void GetStreamInfo(clang::Stmt* root, std::vector<StreamInfo>& streams,
                   clang::DiagnosticsEngine& diagnostics_engine);

const clang::ClassTemplateSpecializationDecl* GetTapaStreamDecl(
    const clang::Type* type);
const clang::ClassTemplateSpecializationDecl* GetTapaStreamDecl(
    const clang::QualType& qual_type);
const clang::ClassTemplateSpecializationDecl* GetTapaStreamsDecl(
    const clang::Type* type);
const clang::ClassTemplateSpecializationDecl* GetTapaStreamsDecl(
    const clang::QualType& qual_type);
std::vector<const clang::CXXMemberCallExpr*> GetTapaStreamOps(
    const clang::Stmt* stmt);

// Test if a loop contains FIFO operations but not sub-loops.
class RecursiveInnermostLoopsVisitor
    : public clang::RecursiveASTVisitor<RecursiveInnermostLoopsVisitor> {
 public:
  // If has a sub-loop, stop recursion.
  bool VisitDoStmt(clang::DoStmt* stmt) {
    return stmt == self_ ? true : !(has_loop_ = true);
  }
  bool VisitForStmt(clang::ForStmt* stmt) {
    return stmt == self_ ? true : !(has_loop_ = true);
  }
  bool VisitWhileStmt(clang::WhileStmt* stmt) {
    return stmt == self_ ? true : !(has_loop_ = true);
  }

  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr* expr) {
    if (GetTapaStreamDecl(expr->getImplicitObjectArgument()->getType())) {
      has_fifo_ = true;
    }
    return true;
  }

  bool IsInnermostLoop(const clang::Stmt* stmt) {
    switch (stmt->getStmtClass()) {
      case clang::Stmt::DoStmtClass:
      case clang::Stmt::ForStmtClass:
      case clang::Stmt::WhileStmtClass: {
        self_ = stmt;
        has_loop_ = false;
        has_fifo_ = false;
        TraverseStmt(const_cast<clang::Stmt*>(stmt));
        if (!has_loop_ && has_fifo_) {
          return true;
        }
      }
      default: {
      }
    }
    return false;
  }

 private:
  const clang::Stmt* self_{nullptr};
  bool has_loop_{false};
  bool has_fifo_{false};
};

template <typename T>
inline bool IsStreamInterface(T obj) {
  return IsTapaType(obj, "(i|o)stream");
}
template <typename T>
inline bool IsStreamInstance(T obj) {
  return IsTapaType(obj, "stream");
}
template <typename T>
inline bool IsStream(T obj) {
  return IsTapaType(obj, "(i|o)?stream");
}

inline std::string GetStreamElemType(const clang::ParmVarDecl* param) {
  if (IsTapaType(param, "(i|o)streams?")) {
    if (auto arg = GetTemplateArg(param->getType(), 0)) {
      return GetTemplateArgName(*arg);
    }
  }
  return "";
}

#endif  // TAPA_STREAM_H_
