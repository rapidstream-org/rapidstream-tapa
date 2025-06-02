// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "tapa/host/tapa.h"

#include <chrono>
#include <csignal>
#include <cstring>

#include <algorithm>
#include <atomic>
#include <deque>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>

#include <frt.h>

#if TAPA_ENABLE_COROUTINE

#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/segmented_stack.hpp>
#include <boost/thread/condition_variable.hpp>

#if TAPA_ENABLE_STACKTRACE
#include <boost/algorithm/string/predicate.hpp>
#include <boost/stacktrace.hpp>
#endif  // TAPA_ENABLE_STACKTRACE

using std::function;
using std::runtime_error;
using std::string;
using std::unordered_map;

using boost::condition_variable;
using boost::mutex;
using boost::coroutines2::segmented_stack;

using pull_type = boost::coroutines2::coroutine<void>::pull_type;
using push_type = boost::coroutines2::coroutine<void>::push_type;
using unique_lock = boost::unique_lock<mutex>;

namespace tapa {

namespace {

void reschedule_this_thread() {
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

}  // namespace

namespace internal {

// Signal handler for SIGINT to kill running kernel instance,
// when the tapa::invoke synchronous kernel is running.
fpga::Instance* frt_sync_kernel_instance = nullptr;
extern "C" void kill_frt_sync_kernel(int) {
  if (frt_sync_kernel_instance) {
    frt_sync_kernel_instance->Kill();
    frt_sync_kernel_instance = nullptr;
  }
  exit(EXIT_FAILURE);
}

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
    using boost::algorithm::ends_with;
    using boost::algorithm::starts_with;
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
    reschedule_this_thread();
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

int get_physical_core_count() {
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  std::set<int> cores;

  while (std::getline(cpuinfo, line)) {
    std::istringstream iss(line);
    std::string token;
    std::string value;
    if (std::getline(iss, token, ':') && std::getline(iss, value)) {
      token.erase(0, token.find_first_not_of(" \t"));
      token.erase(token.find_last_not_of(" \t") + 1);
      value.erase(0, value.find_first_not_of(" \t"));
      value.erase(value.find_last_not_of(" \t") + 1);
      if (token == "core id") cores.insert(std::stoi(value));
    }
  }

  return cores.size();
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
    this->thread = std::thread([this]() {
      for (;;) {
        // accept new tasks
        {
          unique_lock lock(this->mtx);
          this->task_cv.wait(lock, [this] {
            bool pred =
                this->done || !this->coroutines.empty() || !this->tasks.empty();
            // yield to the OS for every idle spin
            if (!pred) reschedule_this_thread();
            return pred;
          });

          // stop worker if it is done
          if (this->done && this->tasks.empty()) break;

          // create coroutines for new tasks
          while (!this->tasks.empty()) {
            bool detach;
            function<void()> f;
            std::tie(detach, f) = this->tasks.front();
            this->tasks.pop();

            auto& l = this->coroutines[detach];  // list of coroutines
            auto coroutine = new push_type*;
            auto call_back = [this, f, coroutine](pull_type& handle) {
              this->handle_table[*coroutine] = current_handle = &handle;
              delete coroutine;
              f();
            };
            l.emplace_back(segmented_stack(), call_back);
            *coroutine = &l.back();
          }
        }

        // iterate over all tasks and their coroutines
        bool active = false;
        bool coroutine_executed = false;

        bool debugging = this->signal;
        if (debugging) debug = true;
        for (auto& pair : this->coroutines) {
          bool detach = pair.first;
          auto& coroutines = pair.second;
          for (auto it = coroutines.begin(); it != coroutines.end();) {
            if (auto& coroutine = *it) {
              current_handle = this->handle_table[&coroutine];
              coroutine();
              coroutine_executed = true;
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
        if (!active) {
          this->wait_cv.notify_all();
        }

        // check if coroutines are executed
        if (!coroutine_executed) {
          // yield to the OS every time the thread is idle
          reschedule_this_thread();
        }
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
      bool pred = this->tasks.empty() && this->coroutines[false].empty();
      if (!pred) {
        // yield to the OS for every idle spin
        reschedule_this_thread();
      }
      return pred;
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

  mutex cleanup_mtx;
  std::list<function<void()>> cleanup_tasks;

 public:
  thread_pool(size_t worker_count = 0) {
    signal(SIGINT, signal_handler);
    if (worker_count == 0) {
      if (auto concurrency = getenv("TAPA_CONCURRENCY")) {
        worker_count = atoi(concurrency);
      } else {
        worker_count = get_physical_core_count();
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

  void add_cleanup_task(const function<void()>& f) {
    unique_lock lock(this->cleanup_mtx);
    this->cleanup_tasks.push_back(f);
  }

  void run_cleanup_tasks() {
    unique_lock lock(this->cleanup_mtx);
    for (auto& task : this->cleanup_tasks) {
      task();
    }
    this->cleanup_tasks.clear();
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
      pool->run_cleanup_tasks();
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

void schedule_cleanup(const function<void()>& f) { pool->add_cleanup_task(f); }

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

// To build library that works with and without asan, we provide definitions for
// symbols reference by boost's ucontext implementation here.
extern "C" {
__attribute__((weak)) void __sanitizer_start_switch_fiber(void**, const void*,
                                                          size_t) {}
__attribute__((weak)) void __sanitizer_finish_switch_fiber(void*, const void**,
                                                           size_t*) {}
}

#else  // TAPA_ENABLE_COROUTINE

namespace tapa {
namespace internal {

void yield(const std::string& msg) { reschedule_this_thread(); }

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
      reschedule_this_thread();
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

task& task::invoke_frt(std::shared_ptr<fpga::Instance> instance) {
  instance->WriteToDevice();
  instance->Exec();
  instance->ReadFromDevice();
  internal::schedule_cleanup([instance]() { instance->Kill(); });
  internal::schedule(
      /*detach=*/false, [instance]() {
        while (!instance->IsFinished()) {
          // Yield to the OS for every idle spin, so that a thread waiting
          // for the FPGA to finish will not spin in a 100% CPU loop.
          reschedule_this_thread();
          internal::yield("fpga::Instance() is not finished");
        }
        instance->Finish();
      });
  return *this;
}

}  // namespace tapa
