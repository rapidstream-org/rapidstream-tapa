#ifndef TLP_COROUTINE_H_
#define TLP_COROUTINE_H_

#include <cstring>

#include <stdexcept>

#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/fixedsize_stack.hpp>

#include <sys/resource.h>

namespace tlp {

using pull_type = boost::coroutines2::coroutine<void>::pull_type;
using push_type = boost::coroutines2::coroutine<void>::push_type;
using stack_t = boost::coroutines2::fixedsize_stack;

extern pull_type* current_handle;

inline stack_t make_stack_allocator() {
  rlimit rl;
  if (getrlimit(RLIMIT_STACK, &rl) != 0) {
    throw std::runtime_error(std::strerror(errno));
  }
  return stack_t(rl.rlim_cur);
}

}  // namespace tlp

#endif  // TLP_COROUTINE_H_
