#include "tlp.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <thread>

using std::atomic_bool;
using std::function;
using std::string;
using std::unordered_map;

namespace tlp {

mutex thread_name_mtx;
unordered_map<std::thread::id, string> thread_name_table;

thread_local uint64_t last_signal_timestamp = 0;
function<void(int)> signal_handler_func;
atomic_bool sleeping;

void signal_handler_wrapper(int signal) { signal_handler_func(signal); }
void sleep_forever_handler(int signal) {
  sleeping = true;
  std::this_thread::sleep_for(
      std::chrono::nanoseconds(std::numeric_limits<int64_t>::max()));
}

task::task() : main_thread_id{std::this_thread::get_id()} {
  signal_handler_func = [this](int signal) { signal_handler(signal); };
  signal(SIGINT, signal_handler_wrapper);
  signal(SIGUSR1, sleep_forever_handler);
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