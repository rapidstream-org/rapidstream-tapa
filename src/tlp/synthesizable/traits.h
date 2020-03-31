#ifndef TLP_SYNTHESIZABLE_TRAITS_H_
#define TLP_SYNTHESIZABLE_TRAITS_H_

#include "../stream.h"

namespace tlp {

template <typename T>
struct elem_type {};
template <typename T>
struct elem_type<istream<T>&> {
  using type = T;
};
template <typename T>
struct elem_type<ostream<T>&> {
  using type = T;
};
template <typename T, uint64_t S>
struct elem_type<istreams<T, S>&> {
  using type = T;
};
template <typename T, uint64_t S>
struct elem_type<ostreams<T, S>&> {
  using type = T;
};

}  // namespace tlp

#define TLP_ELEM_TYPE(variable) \
  typename ::tlp::elem_type<decltype(variable)>::type

#endif  // TLP_SYNTHESIZABLE_TRAITS_H_