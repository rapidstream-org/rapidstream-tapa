#ifndef TAPA_COROUTINE_H_
#define TAPA_COROUTINE_H_

#include <string>

namespace tapa {
namespace internal {
void yield(const std::string& msg);
}  // namespace internal
}  // namespace tapa

#endif  // TAPA_COROUTINE_H_
