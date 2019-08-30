#include "stream.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"

using std::make_shared;
using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;

using clang::CXXMemberCallExpr;
using clang::DeclRefExpr;
using clang::Expr;
using clang::Stmt;

using llvm::dyn_cast;

// Given a CXXMemberCallExpr, returns its stream operation type as a
// StreamOpEnum (could be kNotStreamOperation).
StreamOpEnum GetStreamOp(const CXXMemberCallExpr* call_expr) {
  // Check that the caller has type tlp::stream.
  if (call_expr->getRecordDecl()->getQualifiedNameAsString() != "tlp::stream") {
    return StreamOpEnum::kNotStreamOperation;
  }
  // Check caller name and number of arguments.
  const string callee = call_expr->getMethodDecl()->getNameAsString();
  const auto num_args = call_expr->getNumArgs();
  if (callee == "empty" && num_args == 0) {
    return StreamOpEnum::kTestEmpty;
  } else if (callee == "eos" && num_args == 0) {
    return StreamOpEnum::kTestEos;
  } else if (callee == "try_peek" && num_args == 1) {
    return StreamOpEnum::kTryPeek;
  } else if (callee == "peek") {
    if (num_args == 0) {
      return StreamOpEnum::kBlockingPeek;
    } else if (num_args == 1) {
      return StreamOpEnum::kNonBlockingPeek;
    }
  } else if (callee == "try_read" && num_args == 0) {
    return StreamOpEnum::kTryRead;
  } else if (callee == "read") {
    if (num_args == 0) {
      return StreamOpEnum::kBlockingRead;
    } else if (num_args == 1) {
      return StreamOpEnum::kNonBlockingRead;
    } else if (num_args == 2) {
      return StreamOpEnum::kNonBlockingDefaultedRead;
    }
  } else if (callee == "try_open" && num_args == 0) {
    return StreamOpEnum::kTryOpen;
  } else if (callee == "open" && num_args == 0) {
    return StreamOpEnum::kOpen;
  } else if (callee == "full" && num_args == 0) {
    return StreamOpEnum::kTestFull;
  } else if (callee == "write" && num_args == 1) {
    return StreamOpEnum::kWrite;
  } else if (callee == "try_write" && num_args == 1) {
    return StreamOpEnum::kTryWrite;
  } else if (callee == "close" && num_args == 0) {
    return StreamOpEnum::kClose;
  } else if (callee == "try_close" && num_args == 0) {
    return StreamOpEnum::kTryClose;
  }

  return StreamOpEnum::kNotStreamOperation;
}

// Given the AST root stmt, returns the name of the first node that has type T
// via DFS, or empty string if no such node is found.
template <typename T>
string GetNameOfFirst(
    const Stmt* stmt,
    shared_ptr<unordered_map<const Stmt*, bool>> visited = nullptr) {
  if (visited == nullptr) {
    visited = make_shared<decltype(visited)::element_type>();
  }
  if ((*visited)[stmt]) {
    // If stmt is visited for the second, it must have returned empty string
    // last time.
    return "";
  }
  (*visited)[stmt] = true;

  if (const auto expr = dyn_cast<T>(stmt)) {
    return expr->getNameInfo().getAsString();
  }
  for (const auto child : stmt->children()) {
    if (stmt == nullptr) {
      continue;
    }
    const auto child_result = GetNameOfFirst<T>(child, visited);
    if (!child_result.empty()) {
      return child_result;
    }
  }
  return "";
}

// Retrive information about the given streams.
void GetStreamInfo(Stmt* root, vector<StreamInfo>& streams,
                   shared_ptr<unordered_map<const Stmt*, bool>> visited) {
  if (visited == nullptr) {
    visited = make_shared<decltype(visited)::element_type>();
  }
  if ((*visited)[root]) {
    return;
  }
  (*visited)[root] = true;

  for (auto stmt : root->children()) {
    if (stmt == nullptr) {
      continue;
    }
    GetStreamInfo(stmt, streams, visited);

    if (const auto call_expr = dyn_cast<CXXMemberCallExpr>(stmt)) {
      const string callee = call_expr->getMethodDecl()->getNameAsString();
      const string caller =
          GetNameOfFirst<DeclRefExpr>(call_expr->getImplicitObjectArgument());
      for (auto& stream : streams) {
        if (stream.name == caller) {
          const StreamOpEnum op = GetStreamOp(call_expr);

          // TODO: use DiagnosticsEngine
          if (op & StreamOpEnum::kIsConsumer) {
            if (stream.is_producer) {
              llvm::errs() << "stream " + stream.name +
                                  " cannot be both producer and consumer";
              exit(EXIT_FAILURE);
            } else if (!stream.is_consumer) {
              stream.is_consumer = true;
            }
          }
          if (op & StreamOpEnum::kIsProducer) {
            if (stream.is_consumer) {
              llvm::errs() << "stream " + stream.name +
                                  " cannot be both producer and consumer";
              exit(EXIT_FAILURE);

            } else if (!stream.is_producer) {
              stream.is_producer = true;
            }
          }
          if (op & StreamOpEnum::kIsBlocking) {
            if (stream.is_non_blocking) {
              llvm::errs() << "stream " + stream.name +
                                  " cannot be both blocking and non-blocking";
              exit(EXIT_FAILURE);
            } else if (!stream.is_blocking) {
              stream.is_blocking = true;
            }
          }
          if (op & StreamOpEnum::kIsNonBlocking) {
            if (stream.is_blocking) {
              llvm::errs() << "stream " + stream.name +
                                  " cannot be both blocking and non-blocking";
              exit(EXIT_FAILURE);
            } else if (!stream.is_non_blocking) {
              stream.is_non_blocking = true;
            }
          }

          stream.ops.push_back(op);
          stream.call_exprs.push_back(call_expr);

          break;
        }
      }
    }
  }
}
