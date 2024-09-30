// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_TRAITS_H_
#define TAPA_TRAITS_H_

namespace tapa {

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

}  // namespace tapa

#define TAPA_ELEM_TYPE(variable) \
  typename ::tapa::elem_type<decltype(variable)>::type

#endif  // TAPA_TRAITS_H_
