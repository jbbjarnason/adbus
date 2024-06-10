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

constexpr auto format_as(enum_as_number e) noexcept -> std::string_view {
  switch (e) {
    case enum_as_number::a: return "a";
    case enum_as_number::b: return "b";
    case enum_as_number::c: return "c";
  }
  return "unknown";
}

constexpr auto format_as(enum_as_string e) noexcept -> std::string_view {
  switch (e) {
    case enum_as_string::a: return "a";
    case enum_as_string::b: return "b";
    case enum_as_string::c: return "c";
  }
  return "unknown";
}
