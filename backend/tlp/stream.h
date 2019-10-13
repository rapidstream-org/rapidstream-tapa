#ifndef TLP_STREAM_H_
#define TLP_STREAM_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "type.h"

// Enum of different operations performed on a stream.
enum StreamOpEnum : uint64_t {
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
  kBlockingPeek = 6ULL << 32 | kIsBlocking | kIsConsumer | kNeedPeeking,
  kNonBlockingPeek = 7ULL << 32 | kIsNonBlocking | kIsConsumer | kNeedPeeking,
  kTryRead = 8ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kBlockingRead = 9ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
  kNonBlockingRead =
      10ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kNonBlockingDefaultedRead = 11ULL << 32 | kIsDestructive | kIsNonBlocking |
                              kIsDefaulted | kIsConsumer,
  kTryOpen = 12ULL << 32 | kIsDestructive | kIsNonBlocking | kIsConsumer,
  kOpen = 13ULL << 32 | kIsDestructive | kIsBlocking | kIsConsumer,
  kTestFull = 14ULL << 32 | kIsConsumer,
  kTryWrite = 15ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
  kWrite = 16ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer,
  kTryClose = 17ULL << 32 | kIsDestructive | kIsNonBlocking | kIsProducer,
  kClose = 18ULL << 32 | kIsDestructive | kIsBlocking | kIsProducer
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
  bool need_peeking{false};
  bool need_eos{false};

  std::string PeekVar() const { return "tlp_" + name + "_peek"; }
  std::string ValueVar() const { return "tlp_" + name + "_value"; }
  std::string ValidVar() const { return "tlp_" + name + "_valid"; }
  static std::string ProceedVar() { return "tlp_proceed"; }
};

StreamOpEnum GetStreamOp(const clang::CXXMemberCallExpr* call_expr);

void GetStreamInfo(clang::Stmt* root, std::vector<StreamInfo>& streams,
                   clang::DiagnosticsEngine& diagnostics_engine);

const clang::ClassTemplateSpecializationDecl* GetTlpStreamDecl(
    const clang::Type* type);
const clang::ClassTemplateSpecializationDecl* GetTlpStreamDecl(
    const clang::QualType& qual_type);
std::vector<const clang::CXXMemberCallExpr*> GetTlpStreamOps(
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
    if (GetTlpStreamDecl(expr->getImplicitObjectArgument()->getType())) {
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
      default: {}
    }
    return false;
  }

 private:
  const clang::Stmt* self_{nullptr};
  bool has_loop_{false};
  bool has_fifo_{false};
};

inline bool IsInputStream(const clang::RecordDecl* decl) {
  return decl != nullptr && decl->getQualifiedNameAsString() == "tlp::istream";
}
inline bool IsOutputStream(const clang::RecordDecl* decl) {
  return decl != nullptr && decl->getQualifiedNameAsString() == "tlp::ostream";
}
inline bool IsStreamInstance(const clang::RecordDecl* decl) {
  return decl != nullptr && decl->getQualifiedNameAsString() == "tlp::stream";
}
inline bool IsStreamInterface(const clang::RecordDecl* decl) {
  return IsInputStream(decl) || IsOutputStream(decl);
}
inline bool IsStream(const clang::RecordDecl* decl) {
  return IsStreamInterface(decl) || IsStreamInstance(decl);
}

inline bool IsStreamInstance(clang::QualType type) {
  return IsStreamInstance(type->getAsRecordDecl());
}
inline bool IsStreamInterface(const clang::LValueReferenceType* type) {
  return type != nullptr &&
         IsStreamInterface(type->getPointeeType()->getAsRecordDecl());
}
inline bool IsStreamInterface(clang::QualType type) {
  return IsStreamInterface(type->getAs<clang::LValueReferenceType>());
}
inline bool IsStreamInterface(const clang::ParmVarDecl* param) {
  return IsStreamInterface(param->getType());
}
inline bool IsInputStream(const clang::LValueReferenceType* type) {
  return type != nullptr &&
         IsInputStream(type->getPointeeType()->getAsRecordDecl());
}
inline bool IsInputStream(clang::QualType type) {
  return IsInputStream(type->getAs<clang::LValueReferenceType>());
}
inline bool IsInputStream(const clang::ParmVarDecl* param) {
  return IsInputStream(param->getType());
}
inline bool IsOutputStream(const clang::LValueReferenceType* type) {
  return type != nullptr &&
         IsOutputStream(type->getPointeeType()->getAsRecordDecl());
}
inline bool IsOutputStream(clang::QualType type) {
  return IsOutputStream(type->getAs<clang::LValueReferenceType>());
}
inline bool IsOutputStream(const clang::ParmVarDecl* param) {
  return IsOutputStream(param->getType());
}
inline std::string GetStreamElemType(const clang::ParmVarDecl* param) {
  if (IsStreamInterface(param->getType())) {
    return GetTemplateArgName(param->getType()
                                  ->getAs<clang::LValueReferenceType>()
                                  ->getPointeeType()
                                  ->getAs<clang::TemplateSpecializationType>()
                                  ->getArg(0));
  }
  return "";
}

#endif  // TLP_STREAM_H_
