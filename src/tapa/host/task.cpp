#include "tapa/host/tapa.h"

#include <csignal>
#include <cstring>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#include <sched.h>
#include <sys/mman.h>

#if TAPA_ENABLE_COROUTINE

#include <boost/algorithm/string/predicate.hpp>
#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/fixedsize_stack.hpp>
#include <boost/stacktrace.hpp>

#include <sys/resource.h>
#include <time.h>

using std::condition_variable;
using std::function;
using std::mutex;
using std::runtime_error;
using std::string;
using std::unordered_map;

using unique_lock = std::unique_lock<mutex>;

using pull_type = boost::coroutines2::coroutine<void>::pull_type;
using push_type = boost::coroutines2::coroutine<void>::push_type;

using boost::algorithm::ends_with;
using boost::algorithm::starts_with;
using boost::coroutines2::fixedsize_stack;

namespace tapa {

namespace internal {

namespace {

thread_local pull_type* current_handle = nullptr;
thread_local bool debug = false;
mutex debug_mtx;  // Print stacktrace one-by-one.

}  // namespace

void yield(const string& msg) {
  if (debug) {
    unique_lock l(debug_mtx);
    LOG(INFO) << msg;
#if TAPA_ENABLE_STACKTRACE
    for (auto& frame : boost::stacktrace::stacktrace()) {
      const auto line = frame.source_line();
      const auto file = frame.source_file();
      auto name = frame.name();
      if (line == 0 || file == __FILE__ ||
          // Ignore STL functions.
          starts_with(name, "void std::") || starts_with(name, "std::") ||
          // Ignore TAPA channel functions.
          ends_with(file, "/tapa/mmap.h") ||
          ends_with(file, "/tapa/stream.h")) {
        continue;
      }
      name = name.substr(0, name.find('('));
      const auto space_pos = name.find(' ');
      if (space_pos != string::npos) name = name.substr(space_pos + 1);
      LOG(INFO) << "  in " << name << "(...) from " << file << ":" << line;
    }
#endif  // TAPA_ENABLE_STACKTRACE
  }
  if (current_handle == nullptr) {
    std::this_thread::yield();
  } else {
    (*current_handle)();
  }
}

namespace {

uint64_t get_time_ns() {
  timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return static_cast<uint64_t>(tp.tv_sec) * 1000000000 + tp.tv_nsec;
}

rlim_t get_stack_size() {
  rlimit rl;
  if (getrlimit(RLIMIT_STACK, &rl) != 0) {
    throw runtime_error(std::strerror(errno));
  }
  return rl.rlim_cur;
}

class worker {
  // dict mapping detach to list of coroutine
  // list is used because the stable pointer can be used as key in handle_table
  unordered_map<bool, std::list<push_type>> coroutines;

  // dict mapping coroutine to handle
  unordered_map<push_type*, pull_type*> handle_table;

  std::queue<std::tuple<bool, function<void()>>> tasks;
  mutex mtx;
  condition_variable task_cv;
  condition_variable wait_cv;
  bool done = false;
  std::atomic_int signal{0};
  std::thread thread;

 public:
  worker() {
    auto stack_size = get_stack_size();
    this->thread = std::thread([this, stack_size]() {
      for (;;) {
        // accept new tasks
        {
          unique_lock lock(this->mtx);
          this->task_cv.wait(lock, [this] {
            return this->done || !this->coroutines.empty() ||
                   !this->tasks.empty();
          });

          // stop worker if it is done
          if (this->done && this->tasks.empty()) break;

          // create coroutines
          while (!this->tasks.empty()) {
            bool detach;
            function<void()> f;
            std::tie(detach, f) = this->tasks.front();
            this->tasks.pop();

            auto& l = this->coroutines[detach];  // list of coroutines
            auto coroutine = new push_type*;
            auto call_back = [this, &l, f, coroutine](pull_type& handle) {
              this->handle_table[*coroutine] = current_handle = &handle;
              delete coroutine;
              f();
            };
            l.emplace_back(fixedsize_stack(stack_size), call_back);
            *coroutine = &l.back();
          }
        }

        // iterate over all tasks and their coroutines
        bool active = false;
        bool debugging = this->signal;
        if (debugging) debug = true;
        for (auto& pair : this->coroutines) {
          bool detach = pair.first;
          auto& coroutines = pair.second;
          for (auto it = coroutines.begin(); it != coroutines.end();) {
            if (auto& coroutine = *it) {
              current_handle = this->handle_table[&coroutine];
              coroutine();
            }

            if (*it) {
              if (!detach) active = true;
              ++it;
            } else {
              unique_lock lock(this->mtx);
              it = coroutines.erase(it);
            }
          }
        }
        if (debugging) {
          debug = false;
          this->signal = 0;
        }
        // response to wait requests
        if (!active) this->wait_cv.notify_all();
      }
    });
  }

  void add_task(bool detach, const function<void()>& f) {
    {
      unique_lock lock(this->mtx);
      this->tasks.emplace(detach, f);
    }
    this->task_cv.notify_one();
  }

  void wait() {
    unique_lock lock(this->mtx);
    this->wait_cv.wait(lock, [this] {
      return this->tasks.empty() && this->coroutines[false].empty();
    });
  }

  void send(int signal) { this->signal = signal; }

  ~worker() {
    {
      unique_lock lock(this->mtx);
      this->done = true;
    }
    this->task_cv.notify_all();
    this->thread.join();
  }
};

void signal_handler(int signal);

class thread_pool {
  mutex worker_mtx;
  std::list<worker> workers;
  decltype(workers)::iterator it;

 public:
  thread_pool(size_t worker_count = 0) {
    signal(SIGINT, signal_handler);
    if (worker_count == 0) {
      if (auto concurrency = getenv("TAPA_CONCURRENCY")) {
        worker_count = atoi(concurrency);
      } else {
        worker_count = std::thread::hardware_concurrency();
      }
    }
    this->add_worker(worker_count);
    it = workers.begin();
  }

  void add_worker(size_t count = 1) {
    unique_lock lock(this->worker_mtx);
    for (size_t i = 0; i < count; ++i) {
      this->workers.emplace_back();
    }
  }

  void add_task(bool detach, const function<void()>& f) {
    unique_lock lock(this->worker_mtx);
    it->add_task(detach, f);
    ++it;
    if (it == this->workers.end()) it = this->workers.begin();
  }

  void wait() {
    for (auto& w : this->workers) w.wait();
  }

  void send(int signal) {
    for (auto& worker : this->workers) worker.send(signal);
  }

  ~thread_pool() {
    unique_lock lock(this->worker_mtx);
    this->workers.clear();
  }
};

thread_pool* pool = nullptr;
const task* top_task = nullptr;
mutex mtx;

// How the signal handler works:
//
// 1. The main thread receives the signal;
// 2. Each worker sets `this->signal`;
// 3. Each worker prints debug info in next iteration of coroutines;
// 4. Each worker clears `this->signal`.
constexpr int64_t kSignalThreshold = 500 * 1000 * 1000;  // 500 ms
int64_t last_signal_timestamp = 0;
void signal_handler(int signal) {
  if (signal == SIGINT) {
    const int64_t signal_timestamp = get_time_ns();
    if (last_signal_timestamp != 0 &&
        signal_timestamp - last_signal_timestamp < kSignalThreshold) {
      LOG(INFO) << "caught SIGINT twice in " << kSignalThreshold / 1000000
                << " ms; exit";
      exit(EXIT_FAILURE);
    }
    LOG(INFO) << "caught SIGINT";
    last_signal_timestamp = signal_timestamp;
    pool->send(signal);
  } else {
    last_signal_timestamp = get_time_ns();
  }
}

}  // namespace

void schedule(bool detach, const function<void()>& f) {
  pool->add_task(detach, f);
}

}  // namespace internal

task::task() {
  unique_lock lock(internal::mtx);
  if (internal::pool == nullptr) {
    internal::pool = new internal::thread_pool;
    internal::top_task = this;
  }
}

task::~task() {
  if (this == internal::top_task) {
    internal::pool->wait();
    unique_lock lock(internal::mtx);
    delete internal::pool;
    internal::pool = nullptr;
  }
}

}  // namespace tapa

#else  // TAPA_ENABLE_COROUTINE

namespace tapa {
namespace internal {

void yield(const std::string& msg) { std::this_thread::yield(); }

namespace {

std::deque<std::thread>* threads = nullptr;
const task* top_task = nullptr;
int active_task_count = 0;
std::mutex mtx;

}  // namespace

void schedule(bool detach, const std::function<void()>& f) {
  if (detach) {
    std::thread(f).detach();
  } else {
    std::unique_lock<std::mutex> lock(internal::mtx);
    threads->emplace_back(f);
  }
}

}  // namespace internal

task::task() {
  std::unique_lock<std::mutex> lock(internal::mtx);
  ++internal::active_task_count;
  if (internal::top_task == nullptr) {
    internal::top_task = this;
  }
  if (internal::threads == nullptr) {
    internal::threads = new std::deque<std::thread>;
  }
}

task::~task() {
  if (this == internal::top_task) {
    for (;;) {
      std::thread t;
      {
        std::unique_lock<std::mutex> lock(internal::mtx, std::defer_lock);
        if (internal::active_task_count == 1 && lock.try_lock()) {
          if (internal::threads->empty()) {
            break;
          }
          t = std::move(internal::threads->front());
          internal::threads->pop_front();
        }
      }
      if (t.joinable()) {
        t.join();
      }
      std::this_thread::yield();
    }
    internal::top_task = nullptr;
  }
  std::unique_lock<std::mutex> lock(internal::mtx);
  --internal::active_task_count;
}

}  // namespace tapa

#endif  // TAPA_ENABLE_COROUTINE

namespace tapa {
namespace internal {

void* allocate(size_t length) {
  void* addr = ::mmap(nullptr, length, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, /*fd=*/-1, /*offset=*/0);
  if (addr == MAP_FAILED) throw std::bad_alloc();
  return addr;
}
void deallocate(void* addr, size_t length) {
  if (::munmap(addr, length) != 0) throw std::bad_alloc();
}

}  // namespace internal
}  // namespace tapa
