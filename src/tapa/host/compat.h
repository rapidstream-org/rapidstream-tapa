#include "tapa/host/stream.h"

#ifdef __SYNTHESIS__
#error hls_compat::stream is not synthesizable
#endif

namespace tapa {
namespace hls_compat {

// An infinite-depth stream that has the same behavior as `hls::stream`.
template <typename T>
using stream = ::tapa::stream<T, ::tapa::internal::kInfiniteDepth>;

}  // namespace hls_compat
}  // namespace tapa
