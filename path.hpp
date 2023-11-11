#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace adbus::protocol {

// * The path may be of any length.
// * The path must begin with an ASCII '/' (integer 47) character, and must consist of elements separated by slash
// characters.
// * Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_"
// * No element may be the empty string.
// * Multiple '/' characters cannot occur in sequence.
// * A trailing '/' character is not allowed unless the path is the root path (a single '/' character).
struct path {
  struct error {
    enum struct code_e : std::uint8_t {
      no_error = 0,
      empty,
      not_absolute,  // does not start with slash
      trailing_slash,
      invalid_character,
      multiple_slashes,
    };
    using index_t = std::size_t;

    constexpr explicit operator bool() const noexcept { return code != code_e::no_error; }
    constexpr bool operator==(error const&) const noexcept = default;

    code_e code{ code_e::no_error };
    index_t index{ 0 };
  };

  static constexpr error validate(std::string_view input) {
    using enum error::code_e;
    if (input.empty()) {
      return error{ .code = empty, .index = 0 };
    }
    if (input.front() != '/') {
      return error{ .code = not_absolute, .index = 0 };
    }
    if (input.size() == 1) {
      return error{};
    }
    if (input.back() == '/') {
      return error{ .code = trailing_slash, .index = input.size() - 1 };
    }
    for (auto it = input.begin() + 1; it != input.end(); ++it) {
      if (*it == '/' && *(it - 1) == '/') {
        return error{ .code = multiple_slashes, .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
      // We have already checked the first character so this should be safe.
      if (!((*it >= 'A' && *it <= 'Z') || (*it >= 'a' && *it <= 'z') || (*it >= '0' && *it <= '9') || *it == '_' ||
            *it == '/')) {
        return error{ .code = invalid_character, .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
    }
    return error{};
  }

  static constexpr std::expected<path, error> make(std::string_view input) {
    auto err = validate(input);
    if (err) {
      return std::unexpected{ err };
    }
    return path{ .buffer = std::string{ input } };
  }
  std::string buffer{};
};

namespace test {

static_assert(path::validate("/foo/bar") == path::error{});
static_assert(path::validate("/") == path::error{});
static_assert(path::validate("/a") == path::error{});
static_assert(path::validate("") == path::error{ .code = path::error::code_e::empty, .index = 0 });
static_assert(path::validate("//") == path::error{ .code = path::error::code_e::trailing_slash, .index = 1 });
static_assert(path::validate("///") == path::error{ .code = path::error::code_e::trailing_slash, .index = 2 });
static_assert(path::validate("/ab/") == path::error{ .code = path::error::code_e::trailing_slash, .index = 3 });
static_assert(path::validate("///a") == path::error{ .code = path::error::code_e::multiple_slashes, .index = 1 });
static_assert(path::validate("/a.b") == path::error{ .code = path::error::code_e::invalid_character, .index = 2 });

}  // namespace test
}  // namespace adbus
