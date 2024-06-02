#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <adbus/core/context.hpp>

namespace adbus::protocol {

// * The path may be of any length.
// * The path must begin with an ASCII '/' (integer 47) character, and must consist of elements separated by slash
// characters.
// * Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_"
// * No element may be the empty string.
// * Multiple '/' characters cannot occur in sequence.
// * A trailing '/' character is not allowed unless the path is the root path (a single '/' character).
struct path {
  static constexpr error validate(std::string_view input) {
    using enum error_code;
    if (input.empty()) {
      return error{ .code = empty, .index = 0 };
    }
    if (input.front() != '/') {
      return error{ .code = path_not_absolute, .index = 0 };
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
    // todo length check?
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

static_assert(path::validate("/foo/bar") == error{});
static_assert(path::validate("/") == error{});
static_assert(path::validate("/a") == error{});
static_assert(path::validate("") == error{ .code = error_code::empty, .index = 0 });
static_assert(path::validate("//") == error{ .code = error_code::trailing_slash, .index = 1 });
static_assert(path::validate("///") == error{ .code = error_code::trailing_slash, .index = 2 });
static_assert(path::validate("/ab/") == error{ .code = error_code::trailing_slash, .index = 3 });
static_assert(path::validate("///a") == error{ .code = error_code::multiple_slashes, .index = 1 });
static_assert(path::validate("/a.b") == error{ .code = error_code::invalid_character, .index = 2 });
static_assert(path::validate("a/b") == error{ .code = error_code::path_not_absolute, .index = 0 });

}  // namespace test
}  // namespace adbus
