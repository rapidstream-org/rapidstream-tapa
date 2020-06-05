#include "tlp.h"

#include <cstring>

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#if BOOST_VERSION > 105900

#include <boost/coroutine2/coroutine.hpp>
#include <boost/coroutine2/fixedsize_stack.hpp>

#include <sys/resource.h>
#include <time.h>

using std::condition_variable;
using std::function;
using std::mutex;
using std::runtime_error;
using std::unordered_map;
using std::vector;

using unique_lock = std::unique_lock<mutex>;

using pull_type = boost::coroutines2::coroutine<void>::pull_type;
using push_type = boost::coroutines2::coroutine<void>::push_type;

using boost::coroutines2::fixedsize_stack;

namespace tlp {

namespace internal {

static thread_local pull_type* current_handle;

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

  std::queue<std::tuple<bool, function<void()>>> tasks;
  mutex mtx;
  condition_variable task_cv;
  condition_variable wait_cv;
  bool done = false;
  std::atomic_bool started{false};
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
        bool active = false;
        for (auto& pair : this->coroutines) {
          bool detach = pair.first;
          auto& coroutines = pair.second;
          for (auto it = coroutines.begin(); it != coroutines.end();) {
            if (auto& coroutine = *it) {
              current_handle = this->handle_table[&coroutine];
              coroutine();
            }

            if (*it || !this->started) {
              if (!detach) active = true;
              ++it;
            } else {
              unique_lock lock(this->mtx);
              it = coroutines.erase(it);
            }
          }
        }
        // response to wait requests
        if (!active) this->wait_cv.notify_all();
      }
    });
  }

  void add_task(bool detach, const function<void()>& f) {
    if (this->started) {
      throw std::runtime_error("new task added after worker started");
    }
    {
      unique_lock lock(this->mtx);
      this->tasks.emplace(detach, f);
    }
    this->task_cv.notify_one();
  }

  void wait() {
    this->started = true;
    unique_lock lock(this->mtx);
    this->wait_cv.wait(lock, [this] {
      return this->tasks.empty() && this->coroutines[false].empty();
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
    unique_lock lock(this->worker_mtx);
    ++it;
    if (it == this->workers.end()) it = this->workers.begin();
  }

  void wait() {
    for (auto& w : this->workers) w.wait();
  }

  ~thread_pool() {
    unique_lock lock(this->worker_mtx);
    this->workers.clear();
  }
};

static thread_pool* pool = nullptr;
static const task* top_task = nullptr;
static mutex mtx;

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

void task::schedule(bool detach, const function<void()>& f) {
  internal::pool->add_task(detach, f);
}

}  // namespace tlp

#else  // BOOST_VERSION

namespace tlp {
namespace internal {

void yield() { std::this_thread::yield(); }

static std::vector<std::thread> threads;
static const task* top_task = nullptr;

}  // namespace internal

task::task() {
  if (internal::top_task == nullptr) {
    internal::top_task = this;
  }
}

task::~task() {
  if (this == internal::top_task) {
    for (auto& t : internal::threads) t.join();
    internal::top_task = nullptr;
  }
}

void task::schedule(bool detach, const std::function<void()>& f) {
  if (detach) {
    std::thread(f).detach();
  } else {
    internal::threads.emplace_back(f);
  }
}

}  // namespace tlp

#endif  // BOOST_VERSION
