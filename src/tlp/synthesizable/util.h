#ifndef TLP_UTIL_H_
#define TLP_UTIL_H_

#include <iostream>

#include "../stream.h"

namespace tlp {

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
#pragma HLS inline self off
#pragma HLS interface ap_none register port = return
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

}  // namespace tlp

#define TLP_WHILE_NOT_EOS(fifo)                               \
  for (bool tlp_##fifo##_valid;                               \
       !fifo.eos(tlp_##fifo##_valid) || !tlp_##fifo##_valid;) \
    if (tlp_##fifo##_valid)

#define TLP_WHILE_NEITHER_EOS(fifo1, fifo2)                         \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid;               \
       (!fifo1.eos(tlp_##fifo1##_valid) || !tlp_##fifo1##_valid) && \
       (!fifo2.eos(tlp_##fifo2##_valid) || !tlp_##fifo2##_valid);)  \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid)

#define TLP_WHILE_NONE_EOS(fifo1, fifo2, fifo3)                            \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid, tlp_##fifo3##_valid; \
       (!fifo1.eos(tlp_##fifo1##_valid) || !tlp_##fifo1##_valid) &&        \
       (!fifo2.eos(tlp_##fifo2##_valid) || !tlp_##fifo2##_valid) &&        \
       (!fifo3.eos(tlp_##fifo3##_valid) || !tlp_##fifo3##_valid);)         \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid && tlp_##fifo3##_valid)

#endif  // TLP_UTIL_H_