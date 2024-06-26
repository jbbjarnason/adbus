#pragma once

#include <cstdint>

#include <glaze/core/common.hpp>

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
  string_too_long,
  array_too_long,
  invalid_enum_conversion, // to string
  // read errors
  out_of_range, // if buffer is smaller than the expected input is
  unexpected_enum, // from string
  unexpected_variant, // Any of the given variant type types do not match the signature from buffer
  // remember to add to glaze enumerate below
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

template <>
struct glz::meta<adbus::protocol::error_code> {
  using enum adbus::protocol::error_code;
  static constexpr auto value{ glz::enumerate(
  "no_error", no_error,
  "empty", empty,
  "path_not_absolute", path_not_absolute,
  "trailing_slash", trailing_slash,
  "invalid_character", invalid_character,
  "multiple_slashes", multiple_slashes,
  "too_short", too_short,
  "too_long", too_long,
  "trailing_dot", trailing_dot,
  "multiple_dots", multiple_dots,
  "buffer_too_small", buffer_too_small,
  "string_too_long", string_too_long,
  "array_too_long", array_too_long,
  "invalid_enum_conversion", invalid_enum_conversion,
  "out_of_range", out_of_range,
  "unexpected_enum", unexpected_enum,
  "unexpected_variant", unexpected_variant
  ) };
};

namespace adbus::protocol {

constexpr auto format_as(error_code err) noexcept -> std::string_view {
  return glz::detail::get_enum_name(err);
}

constexpr auto format_as(error const& err) noexcept -> std::string_view {
  // todo format more useful info
  return format_as(err.code);
}

}  // namespace adbus::protocol
