// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <string>
#include <string_view>

namespace tapa::internal {

std::string StrCat(std::initializer_list<std::string_view> pieces);

// Utilities to obtain function traits.
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};
template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> {
  using return_type = R;
  using params = std::tuple<Args...>;
};
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> {
  using return_type = R;
  using params = std::tuple<Args...>;
};
template <typename R, typename... Args>
struct function_traits<R (&)(Args...)> {
  using return_type = R;
  using params = std::tuple<Args...>;
};

// Utilities to check if a type is callable.
template <typename F>
struct is_callable {
 private:
  typedef char (&yes)[1];
  typedef char (&no)[2];
  template <typename U>
  static yes check(decltype(&U::operator()));
  template <typename U>
  static no check(...);

 public:
  static constexpr bool value =
      std::is_function_v<F> || sizeof(check<F>(0)) == sizeof(yes);
};
template <typename F>
inline constexpr bool is_callable_v = is_callable<F>::value;

}  // namespace tapa::internal
