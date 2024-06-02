#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

#include <adbus/protocol/meta.hpp>
#include <adbus/core/context.hpp>

namespace adbus::protocol {

template <typename explicit_name_t>
struct name {
  static constexpr std::size_t max_length{ 255 };
  static constexpr std::size_t min_length{ 2 };
  static constexpr error validate(std::string_view input) noexcept {
    if (input.size() <= min_length) {
      return error{ .code = error_code::too_short, .index = 0 };
    }
    if (input.size() > max_length) {
      return error{ .code = error_code::too_long, .index = input.size() };
    }
    auto err{ explicit_name_t::validate_inner(input) };
    if (err) {
      return err;
    }
    if (input.back() == '.') {
      return error{ .code = error_code::trailing_dot, .index = input.size() - 1 };
    }
    for (auto it = input.begin() + 1; it != input.end(); ++it) {
      // We have already checked the first character so this should be safe.
      if (*it == '.' && *(it - 1) == '.') {
        return error{ .code = error_code::multiple_dots,
                      .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
    }
    return error{};
  }

  static constexpr std::expected<explicit_name_t, error> make(std::string_view input) {
    auto err = validate(input);
    if (err) {
      return std::unexpected{ err };
    }
    return name{ .value = std::string{ input } };
  }

  std::string value{};
};

struct interface_name : name<interface_name> {
  // - Interface names are composed of 2 or more elements separated by a period ('.') character. All elements must contain at
  // least one character.
  // - Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_" and must not begin with a digit.
  // - Interface names must not exceed the maximum name length.
  static constexpr error validate_inner(std::string_view input) noexcept {
    auto const beginning_letter{ *input.begin() };
    if (!((beginning_letter >= 'A' && beginning_letter <= 'Z') || (beginning_letter >= 'a' && beginning_letter <= 'z') ||
          beginning_letter == '_')) {
      return error{ .code = error_code::invalid_character, .index = 0 };
    }
    for (auto it = input.begin() + 1; it != input.end(); ++it) {
      if (!((*it >= 'A' && *it <= 'Z') || (*it >= 'a' && *it <= 'z') || (*it >= '0' && *it <= '9') || *it == '_' ||
            *it == '.')) {
        return error{ .code = error_code::invalid_character,
                      .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
    }
    return error{};
  }
};

struct bus_name : name<bus_name> {
  // - Bus names that start with a colon (':') character are unique connection names. Other bus names are called well-known
  // bus names.
  // - Bus names are composed of 1 or more elements separated by a period ('.') character. All elements must contain at least
  // one character.
  // - Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_-", with "-" discouraged in new bus names. Only
  // elements that are part of a unique connection name may begin with a digit, elements in other bus names must not begin
  // with a digit.
  // - Bus names must contain at least one '.' (period) character (and thus at least two elements).
  // - Bus names must not begin with a '.' (period) character.
  // - Bus names must not exceed the maximum name length.
  static constexpr error validate_inner(std::string_view input) noexcept {
    auto const beginning_letter{ *input.begin() };
    if (!((beginning_letter >= 'A' && beginning_letter <= 'Z') || (beginning_letter >= 'a' && beginning_letter <= 'z') ||
          beginning_letter == '_' || beginning_letter == ':')) {
      return error{ .code = error_code::invalid_character, .index = 0 };
    }
    for (auto it = input.begin() + 1; it != input.end(); ++it) {
      if (!((*it >= 'A' && *it <= 'Z') || (*it >= 'a' && *it <= 'z') || (*it >= '0' && *it <= '9') || *it == '_' ||
            *it == '.')) {
        return error{ .code = error_code::invalid_character,
                      .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
    }
    return error{};
  }
};

struct member_name : name<member_name> {
  // - Must only contain the ASCII characters "[A-Z][a-z][0-9]_" and may not begin with a digit.
  // - Must not contain the '.' (period) character.
  // - Must not exceed the maximum name length.
  // - Must be at least 1 byte in length.
  static constexpr error validate_inner(std::string_view input) noexcept {
    auto const beginning_letter{ *input.begin() };
    if (!((beginning_letter >= 'A' && beginning_letter <= 'Z') || (beginning_letter >= 'a' && beginning_letter <= 'z') ||
          beginning_letter == '_')) {
      return error{ .code = error_code::invalid_character, .index = 0 };
    }
    for (auto it = input.begin() + 1; it != input.end(); ++it) {
      if (!((*it >= 'A' && *it <= 'Z') || (*it >= 'a' && *it <= 'z') || (*it >= '0' && *it <= '9') || *it == '_')) {
        return error{ .code = error_code::invalid_character,
                      .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
    }
    return error{};
  }
};

struct error_name : interface_name {
  // Error names have the same restrictions as interface names.
};

static_assert(interface_name::validate("org.freedesktop.DBus") == error{});
static_assert(interface_name::validate("org.freedesktop.DBus.") == error{.code=error_code::trailing_dot, .index=20});
static_assert(interface_name::validate("org.freedesktop..DBus") == error{.code=error_code::multiple_dots, .index=16});
static_assert(interface_name::validate("org.freedesktop.DBus-Local") == error{.code=error_code::invalid_character, .index=20});

static_assert(bus_name::validate("org.freedesktop.DBus") == error{});
static_assert(bus_name::validate("org.freedesktop.DBus.") == error{.code=error_code::trailing_dot, .index=20});
static_assert(bus_name::validate("org.freedesktop..DBus") == error{.code=error_code::multiple_dots, .index=16});
static_assert(bus_name::validate("org.freedesktop.DBus-Local") == error{.code=error_code::invalid_character, .index=20});

static_assert(error_name::validate("org.freedesktop.DBus") == error{});
static_assert(error_name::validate("org.freedesktop.DBus.") == error{.code=error_code::trailing_dot, .index=20});
static_assert(error_name::validate("org.freedesktop..DBus") == error{.code=error_code::multiple_dots, .index=16});
static_assert(error_name::validate("org.freedesktop.DBus-Local") == error{.code=error_code::invalid_character, .index=20});

static_assert(member_name::validate("orgfreedesktopDBus") == error{});
static_assert(member_name::validate("org.freedesktop.DBus") == error{.code=error_code::invalid_character, .index=3});
static_assert(member_name::validate("org-freedesktop..DBus") == error{.code=error_code::invalid_character, .index=3});

}  // namespace adbus::protocol

template<>
struct adbus::protocol::meta<adbus::protocol::interface_name> {
  static constexpr std::string_view name{ "adbus::protocol::interface_name" };
  static constexpr auto value{ &adbus::protocol::interface_name::value };
};

template<>
struct adbus::protocol::meta<adbus::protocol::bus_name> {
  static constexpr std::string_view name{ "adbus::protocol::bus_name" };
  static constexpr auto value{ &adbus::protocol::bus_name::value };
};

template<>
struct adbus::protocol::meta<adbus::protocol::member_name> {
  static constexpr std::string_view name{ "adbus::protocol::member_name" };
  static constexpr auto value{ &adbus::protocol::member_name::value };
};
