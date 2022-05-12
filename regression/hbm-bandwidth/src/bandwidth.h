#include <tapa.h>

using Elem = tapa::vec_t<float, 16>;
constexpr int kBankCount = 32;

constexpr uint64_t kRead = 1 << 1;
constexpr uint64_t kWrite = 1 << 2;
