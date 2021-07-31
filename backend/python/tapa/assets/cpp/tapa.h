#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

#include <climits>
#include <cstdint>

#ifndef __SYNTHESIS__

#include <thread>
#include <vector>

#include <glog/logging.h>

#define HLS_STREAM_THREAD_SAFE

#else  // __SYNTHESIS__

struct dummy {
  template <typename T>
  dummy& operator<<(const T&) {
    return *this;
  }
};

#define LOG(level) dummy()
#define LOG_IF(level, cond) dummy()
#define LOG_EVERY_N(level, n) dummy()
#define LOG_IF_EVERY_N(level, cond, n) dummy()
#define LOG_FIRST_N(level, n) dummy()

#define DLOG(level) dummy()
#define DLOG_IF(level, cond) dummy()
#define DLOG_EVERY_N(level, n) dummy()

#define CHECK(cond) dummy()
#define CHECK_NE(lhs, rhs) dummy()
#define CHECK_EQ(lhs, rhs) dummy()
#define CHECK_GE(lhs, rhs) dummy()
#define CHECK_GT(lhs, rhs) dummy()
#define CHECK_LE(lhs, rhs) dummy()
#define CHECK_LT(lhs, rhs) dummy()
#define CHECK_NOTNULL(ptr) (ptr)
#define CHECK_STREQ(lhs, rhs) dummy()
#define CHECK_STRNE(lhs, rhs) dummy()
#define CHECK_STRCASEEQ(lhs, rhs) dummy()
#define CHECK_STRCASENE(lhs, rhs) dummy()
#define CHECK_DOUBLE_EQ(lhs, rhs) dummy()

#define VLOG_IS_ON(level) false
#define VLOG(level) dummy()
#define VLOG_IF(level, cond) dummy()
#define VLOG_EVERY_N(level, n) dummy()
#define VLOG_IF_EVERY_N(level, cond, n) dummy()
#define VLOG_FIRST_N(level, n) dummy()

#endif  // __SYNTHESIS__

#include <ap_int.h>

#include "tapa/mmap.h"
#include "tapa/stream.h"
#include "tapa/synthesizable/traits.h"
#include "tapa/synthesizable/util.h"
#include "tapa/synthesizable/vec.h"

namespace tapa {

class task;
class seq;

}  // namespace tapa

#endif  // TASK_LEVEL_PARALLELIZATION_H_
