#pragma once

#include <cstdint>
#include <glaze/core/common.hpp>

enum struct enum_as_number : std::uint8_t {
  a = 1,
  b = 2,
  c = 3,
};

enum struct enum_as_string : std::uint8_t {
  a = 1,
  b = 2,
  c = 3,
};
template <>
struct glz::meta<enum_as_string> {
  using enum enum_as_string;
  static constexpr auto value{ glz::enumerate("a", a, "b", b, "c", c) };
};
