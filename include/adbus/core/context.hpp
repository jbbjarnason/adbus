#pragma once

#include <cstdint>

namespace adbus::protocol {

enum struct error_code : std::uint8_t {
  no_error = 0,
  // path/name errors
  empty,
  path_not_absolute,  // does not start with slash
  trailing_slash,
  invalid_character,
  multiple_slashes,
  too_short,
  too_long,
  trailing_dot,
  multiple_dots,
  // write errors
  buffer_too_small,
};

struct error final {
  using index_t = std::size_t;

  constexpr explicit operator bool() const noexcept { return code != error_code::no_error; }
  constexpr bool operator==(error const&) const noexcept = default;

  error_code code{ error_code::no_error };
  index_t index{ 0 };
};

struct context final {
  error err{};
};

template <class T>
concept is_context = std::same_as<std::decay_t<T>, context>;

struct options final {
  bool enum_as_string{ false }; // todo implement
};

}  // namespace adbus::protocol
