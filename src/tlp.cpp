#include "tlp.h"

#include <cstring>

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/fixedsize_stack.hpp>

#include <sys/resource.h>
#include <time.h>

using std::condition_variable;
using std::function;
using std::future;
using std::get;
using std::mutex;
using std::queue;
using std::runtime_error;
using std::unordered_map;
using std::vector;

using unique_lock = std::unique_lock<mutex>;

using pull_type = boost::coroutines2::coroutine<void>::pull_type;
using push_type = boost::coroutines2::coroutine<void>::push_type;

using boost::coroutines2::fixedsize_stack;

namespace tlp {

namespace internal {

thread_local pull_type* current_handle;

void yield() { (*current_handle)(); }

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
  unordered_map<bool, std::list<push_type>> coroutines;

  // dict mapping coroutine to handle
  unordered_map<push_type*, pull_type*> handle_table;

  queue<std::tuple<bool, function<void()>>> tasks;
  mutex mtx;
  condition_variable task_cv;
  condition_variable wait_cv;
  bool done = false;
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
            auto last = l.empty() ? l.end() : std::prev(l.end());
            auto call_back = [this, &l, last, f](pull_type& handle) {
              // must have a stable pointer
              auto& coroutine = last == l.end() ? *l.begin() : *std::next(last);
              this->handle_table[&coroutine] = current_handle = &handle;
              f();
            };
            l.emplace_back(fixedsize_stack(stack_size), call_back);
          }
        }

        // iterate over all tasks and their coroutines
        for (auto it = this->coroutines.begin();
             it != this->coroutines.end();) {
          auto& coroutines = it->second;
          for (auto it = coroutines.begin(); it != coroutines.end();) {
            if (auto& coroutine = *it) {
              current_handle = this->handle_table[&coroutine];
              auto coroutine_begin = get_time_ns();
              coroutine();
              auto coroutine_end = get_time_ns();
              auto delta = coroutine_end - coroutine_begin;
            }
            it = *it ? std::next(it) : coroutines.erase(it);
          }

          // clean up
          if (coroutines.empty()) {
            // response to wait requests
            {
              unique_lock lock(this->mtx);
              it = this->coroutines.erase(it);
            }
            this->wait_cv.notify_all();
          } else {
            ++it;
          }
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
      return this->tasks.empty() && this->coroutines.count(false) == 0;
    });
  }

  ~worker() {
    {
      unique_lock lock(this->mtx);
      this->done = true;
    }
    this->task_cv.notify_all();
    this->thread.join();
  }
};

class thread_pool {
  mutex worker_mtx;
  std::list<worker> workers;
  decltype(workers)::iterator it;

  void clean_up() {
    unique_lock lock(this->worker_mtx);
    this->workers.clear();
  }

 public:
  thread_pool(size_t worker_count = 0) {
    if (worker_count == 0) {
      if (auto concurrency = getenv("TLP_CONCURRENCY")) {
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
    it->add_task(detach, f);
    ++it;
    if (it == this->workers.end()) it = this->workers.begin();
  }

  void wait() {
    for (auto& w : this->workers) w.wait();
    clean_up();
  }

  ~thread_pool() { clean_up(); }
};

thread_pool* pool = nullptr;
mutex mtx;

}  // namespace internal

task::task() : is_top(false) {
  unique_lock lock(internal::mtx);
  if (internal::pool == nullptr) {
    internal::pool = new internal::thread_pool;
    this->is_top = true;
  }
}

task::~task() {
  internal::pool->wait();
  if (this->is_top) {
    unique_lock lock(internal::mtx);
    delete internal::pool;
    internal::pool = nullptr;
  }
}

void task::schedule(bool detach, const function<void()>& f) {
  internal::pool->add_task(detach, f);
}

}  // namespace tlp
