#ifndef TAPA_UTIL_H_
#define TAPA_UTIL_H_

#include <iostream>

#include "../stream.h"

namespace tapa {

constexpr int join = 0;
constexpr int detach = -1;

template <typename T>
inline constexpr uint64_t widthof() {
  return sizeof(T) * CHAR_BIT;
}

template <uint64_t N>
uint64_t round_up_div(uint64_t i) {
  return ((i - 1) / N + 1);
}

template <uint64_t N>
uint64_t round_up(uint64_t i) {
  return ((i - 1) / N + 1) * N;
}

template <typename T>
T reg(T x) {
#pragma HLS inline off
#pragma HLS interface ap_none register port = x
  return x;
}

template <typename Addr, typename Payload>
struct packet {
  Addr addr;
  Payload payload;
};

template <typename Addr, typename Payload>
inline std::ostream& operator<<(std::ostream& os,
                                const packet<Addr, Payload>& obj) {
  return os << "{addr: " << obj.addr << ", payload: " << obj.payload << "}";
}

}  // namespace tapa

#define TAPA_WHILE_NOT_EOS(fifo)                                \
  for (bool tapa_##fifo##_valid;                                \
       !fifo.eos(tapa_##fifo##_valid) || !tapa_##fifo##_valid;) \
   _Pragma("HLS pipeline II = 1")                               \
    if (tapa_##fifo##_valid)

#define TAPA_WHILE_NEITHER_EOS(fifo1, fifo2)                          \
  for (bool tapa_##fifo1##_valid, tapa_##fifo2##_valid;               \
       (!fifo1.eos(tapa_##fifo1##_valid) || !tapa_##fifo1##_valid) && \
       (!fifo2.eos(tapa_##fifo2##_valid) || !tapa_##fifo2##_valid);)  \
  _Pragma("HLS pipeline II = 1")                                      \
    if (tapa_##fifo1##_valid && tapa_##fifo2##_valid)

#define TAPA_WHILE_NONE_EOS(fifo1, fifo2, fifo3)                              \
  for (bool tapa_##fifo1##_valid, tapa_##fifo2##_valid, tapa_##fifo3##_valid; \
       (!fifo1.eos(tapa_##fifo1##_valid) || !tapa_##fifo1##_valid) &&         \
       (!fifo2.eos(tapa_##fifo2##_valid) || !tapa_##fifo2##_valid) &&         \
       (!fifo3.eos(tapa_##fifo3##_valid) || !tapa_##fifo3##_valid);)          \
  _Pragma("HLS pipeline II = 1")                                              \
    if (tapa_##fifo1##_valid && tapa_##fifo2##_valid && tapa_##fifo3##_valid)

#endif  // TAPA_UTIL_H_
