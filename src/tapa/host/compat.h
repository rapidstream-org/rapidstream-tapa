#include "tapa/host/stream.h"
#include "tapa/host/task.h"

#ifdef __SYNTHESIS__
#error hls_compat::stream is not synthesizable
#endif

namespace tapa {
namespace hls_compat {

// An infinite-depth stream that has the same behavior as `hls::stream`.
template <typename T>
using stream = ::tapa::stream<T, ::tapa::internal::kInfiniteDepth>;

// Interface that accepts both `tapa::stream` and  `tapa::hls_compat::stream`.
template <typename T>
using stream_interface = ::tapa::internal::unbound_stream<T>;

// Same as `tapa::task()`, except that tasks are scheduled sequentially.
struct task : public ::tapa::task {
  explicit task();
};

}  // namespace hls_compat
}  // namespace tapa
