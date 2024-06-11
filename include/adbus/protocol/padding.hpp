#pragma once

#include <cstdint>

#include <glaze/core/common.hpp>

#include <adbus/util/concepts.hpp>

namespace adbus::protocol::detail {

template <typename T>
struct padding;

template <num_t T>
struct padding<T> {
  static constexpr std::size_t value{ sizeof(T) };
};

template <string_like T>
struct padding<T> {
  static constexpr std::size_t value{ sizeof(std::uint32_t) };
};

template <adbus::type::is_signature T>
struct padding<T> {
  static constexpr std::size_t value{ sizeof(std::uint8_t) };
};

template <container T>
struct padding<T> {
  static constexpr std::size_t value{ sizeof(std::uint32_t) };
};

template <typename T>
  requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
struct padding<T> {
  // I do not like the below, doing more reflection_count than needed
  static constexpr auto N = glz::reflection_count<T>;
  using Element = glz::detail::glaze_tuple_element<0, N, T>;  // first element
  using type = std::remove_cvref_t<typename Element::type>;
  static constexpr std::size_t value{ padding<type>::value };
};

template <glz::detail::pair_t T>
struct padding<T> {
  static constexpr std::size_t value{ padding<typename T::first_type>::value };
};

}  // namespace adbus::protocol::detail
