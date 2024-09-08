// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "tapa/host/stream.h"
#include "tapa/host/task.h"

#ifdef __SYNTHESIS__
#error hls_compat::stream is not synthesizable
#endif

namespace tapa {
namespace hls_compat {

/// An infinite-depth stream that has the same behavior as @c hls::stream.
///
/// Intended for defining streams without knowing their depth for synthesis:
/// @code{.cpp}
///  ...
///  #include <tapa.h>
///  #include <tapa/host/compat.h>
///  ...
///  void Top() {
///    tapa::hls_compat::stream<int> data_q("data");
///    ...
///    tapa::task()
///      .invoke(...)
///      .invoke(...)
///      ...
///      ;
///  }
/// @endcode
///
/// Software simulation only; NOT synthesizable.
/// Replace with @c tapa::stream for synthesis.
template <typename T>
using stream = ::tapa::stream<T, ::tapa::internal::kInfiniteDepth>;

/// I/O direction agnostic interface that accepts both @c tapa::stream and
/// @c tapa::hls_compat::stream.
///
/// Intended for declaring parameters without knowing the I/O direction:
/// @code{.cpp}
///  ...
///  #include <tapa.h>
///  #include <tapa/host/compat.h>
///  ...
///  void Compute(tapa::hls_compat::stream_interface<int>& data_in_q) {
///    int data = data_in_q.read();
///    ...
///  }
/// @endcode
///
/// Software simulation only; NOT synthesizable.
/// Replace with @c tapa::istream / @c tapa::ostream for synthesis.
template <typename T>
using stream_interface = ::tapa::internal::unbound_stream<T>;

/// Same as @c tapa::task, except that tasks are scheduled sequentially.
///
/// Intended for debugging code migrated from HLS:
/// @code{.cpp}
///  ...
///  #include <tapa.h>
///  #include <tapa/host/compat.h>
///  ...
///  void Top() {
///    ...
///    tapa::hls_compat::task()
///      .invoke(...)
///      .invoke(...)
///      ...
///      ;
///  }
/// @endcode
///
/// Software simulation only; NOT synthesizable.
/// Replace with @c tapa::task for synthesis.
struct task : public ::tapa::task {
  explicit task();
};

}  // namespace hls_compat
}  // namespace tapa
