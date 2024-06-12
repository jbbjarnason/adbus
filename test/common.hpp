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

struct foo {
  struct bar {
    std::string a{};
    std::uint64_t b{};
    constexpr auto operator==(bar const&) const noexcept -> bool = default;
  };
  std::uint64_t a{};
  std::vector<bar> bars{};
  std::vector<bar> bars2{};
  std::string b{};
  constexpr auto operator==(foo const&) const noexcept -> bool = default;
};

constexpr auto format_as(foo::bar const& b) -> std::string {
  return fmt::format("a: {}, b: {}", b.a, b.b);
}
constexpr auto format_as(foo const& f) -> std::string {
  return fmt::format("a: {}, b: {}, bars: {}, bars2: {}", f.a, f.b, fmt::join(f.bars, ", "), fmt::join(f.bars2, ", "));
}

struct simple {
  std::uint8_t a{ 42 };
  std::string b{ "dbus" };
  double c{ 1337.42 };
  constexpr auto operator==(simple const&) const noexcept -> bool = default;
};

constexpr auto format_as(simple const& s) -> std::string {
  return fmt::format("a: {}, b: {}, c: {}", s.a, s.b, s.c);
}


