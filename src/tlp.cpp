#include "tlp.h"

#include <functional>
#include <thread>

using std::function;

namespace tlp {

thread_local uint64_t last_signal_timestamp = 0;
function<void(int)> signal_handler_func;

void signal_handler_wrapper(int signal) { signal_handler_func(signal); }

task::task() : main_thread_id{std::this_thread::get_id()} {
  signal_handler_func = [this](int signal) { signal_handler(signal); };
  signal(SIGINT, signal_handler_wrapper);
}

void task::signal_handler(int signal) {
  if (std::this_thread::get_id() == main_thread_id) {
    uint64_t signal_timestamp = get_time_ns();
    if (last_signal_timestamp != 0 &&
        signal_timestamp - last_signal_timestamp < kSignalThreshold) {
      LOG(INFO) << "caught SIGINT twice in " << kSignalThreshold / 1000000
                << " ms; exit";
      exit(EXIT_FAILURE);
    }
    LOG(INFO) << "caught SIGINT";
    last_signal_timestamp = signal_timestamp;
    for (auto& t : threads[current_step]) {
      if (t.joinable()) {
        pthread_kill(t.native_handle(), signal);
      }
    }
  } else {
    last_signal_timestamp = get_time_ns();
  }
}

}  // namespace tlp