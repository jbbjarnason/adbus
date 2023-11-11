#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

#include "string_view_join.hpp"

// for testing
#include <array>
#include <concepts.hpp>
#include <set>
#include <tuple>
#include <tuplet/tuplet.hpp>
#include <vector>

namespace adbus::protocol::type {

using std::string_view_literals::operator""sv;

template <typename type_t>
struct signature : std::false_type {};

template <typename type_t>
static constexpr auto signature_v{ signature<type_t>::value };

template <>
struct signature<std::uint8_t> {
  static constexpr auto value{ "y"sv };
};

template <std::same_as<bool> value_t>
struct signature<value_t> {
  static constexpr auto value{ "b"sv };
};

template <concepts::explicit_integral<std::int16_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "n"sv };
};

template <concepts::explicit_integral<std::uint16_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "q"sv };
};

template <concepts::explicit_integral<std::int32_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "i"sv };
};

template <concepts::explicit_integral<std::uint32_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "u"sv };
};

template <concepts::explicit_integral<std::int64_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "x"sv };
};

template <concepts::explicit_integral<std::uint64_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "t"sv };
};

template <std::same_as<double> value_t>
struct signature<value_t> {
  static constexpr auto value{ "d"sv };
};

// unix_fd ?

template <typename type_t>
  requires(std::same_as<std::remove_cvref_t<type_t>, std::string_view> ||
           std::same_as<std::remove_cvref_t<type_t>, std::string>)
struct signature<type_t> {
  static constexpr auto value{ "s"sv };
};

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

    constexpr bool operator()() const noexcept { return code != code_e::no_error; }
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
    if (err()) {
      return std::unexpected{ err };
    }
    return path{ .buffer = std::string{ input } };
  }
  std::string buffer{};
};

static_assert(path::validate("/foo/bar") == path::error{});
static_assert(path::validate("/") == path::error{});
static_assert(path::validate("/a") == path::error{});
static_assert(path::validate("") == path::error{ .code = path::error::code_e::empty, .index = 0 });
static_assert(path::validate("//") == path::error{ .code = path::error::code_e::trailing_slash, .index = 1 });
static_assert(path::validate("///") == path::error{ .code = path::error::code_e::trailing_slash, .index = 2 });
static_assert(path::validate("/ab/") == path::error{ .code = path::error::code_e::trailing_slash, .index = 3 });
static_assert(path::validate("///a") == path::error{ .code = path::error::code_e::multiple_slashes, .index = 1 });
static_assert(path::validate("/a.b") == path::error{ .code = path::error::code_e::invalid_character, .index = 2 });

template <typename type_t>
  requires std::same_as<std::remove_cvref_t<type_t>, path>
struct signature<type_t> {
  static constexpr auto value{ "o"sv };
};

template <typename type_t>
concept has_signature = requires {
  signature<type_t>::value;
  requires std::same_as<std::remove_const_t<decltype(signature<type_t>::value)>, std::string_view>;
};

static_assert(has_signature<std::uint8_t>);
static_assert(has_signature<bool>);
static_assert(has_signature<std::int16_t>);
static_assert(has_signature<std::uint16_t>);
static_assert(has_signature<std::int32_t>);
static_assert(has_signature<std::uint32_t>);
static_assert(has_signature<std::int64_t>);
static_assert(has_signature<std::uint64_t>);
static_assert(has_signature<double>);
static_assert(has_signature<std::string_view>);
static_assert(has_signature<std::string>);
static_assert(has_signature<path>);

static_assert(not has_signature<char*>);
static_assert(not has_signature<const char*>);
static_assert(not has_signature<std::int8_t>);

static_assert(signature_v<std::uint8_t> == "y"sv);
static_assert(signature_v<bool> == "b"sv);
static_assert(signature_v<std::int16_t> == "n"sv);
static_assert(signature_v<std::uint16_t> == "q"sv);
static_assert(signature_v<std::int32_t> == "i"sv);
static_assert(signature_v<std::uint32_t> == "u"sv);
static_assert(signature_v<std::int64_t> == "x"sv);
static_assert(signature_v<std::uint64_t> == "t"sv);
static_assert(signature_v<double> == "d"sv);
static_assert(signature_v<std::string_view> == "s"sv);
static_assert(signature_v<std::string> == "s"sv);
static_assert(signature_v<path> == "o"sv);

template <has_signature type_t, has_signature... types_t>
struct signature<std::variant<type_t, types_t...>> {
  static constexpr auto value{ "v"sv };
};

static_assert(signature_v<std::variant<std::uint8_t>> == "v"sv);
static_assert(signature_v<std::variant<std::uint8_t, std::string>> == "v"sv);
static_assert(has_signature<std::variant<std::uint8_t, std::string>>);
static_assert(!has_signature<std::variant<std::int8_t>>);

template <std::ranges::range type_t>
  requires has_signature<std::ranges::range_value_t<type_t>>
struct signature<type_t> {
  static constexpr auto prefix{ "a"sv };
  static constexpr auto value{ details::join_v<prefix, signature_v<std::ranges::range_value_t<type_t>>> };
};

static_assert(signature_v<std::vector<int>> == "ai"sv);
static_assert(signature_v<std::array<int, 10>> == "ai"sv);
static_assert(signature_v<std::set<int>> == "ai"sv);

template <typename tuple_t>
  requires concepts::is_specialization_v<tuple_t, std::tuple>
struct signature<tuple_t> {
  static constexpr auto prefix{ "("sv };
  static constexpr auto postfix{ ")"sv };
  template <typename type_t>
  struct join_impl;
  template <typename... types_t>
  struct join_impl<std::tuple<types_t...>> {
    static constexpr auto value{ details::join_v<prefix, signature_v<types_t>..., postfix> };
  };
  static constexpr auto value{ join_impl<tuple_t>::value };
};

static_assert(signature_v<std::tuple<int, std::string>> == "(is)"sv);
static_assert(signature_v<std::tuple<int, std::string, std::uint8_t>> == "(isy)"sv);
static_assert(signature_v<std::tuple<int, std::array<std::uint8_t, 10>>> == "(iay)"sv);
static_assert(signature_v<std::tuple<int, std::tuple<std::uint8_t, std::string>>> == "(i(ys))"sv);
static_assert(signature_v<std::tuple<int, std::tuple<std::uint8_t, std::tuple<std::string, std::uint8_t>>>> ==
              "(i(y(sy)))"sv);

template <typename dict_t>
  requires(concepts::is_specialization_v<dict_t, std::map> || concepts::is_specialization_v<dict_t, std::unordered_map>) &&
          concepts::type::basic<typename dict_t::key_type>
struct signature<dict_t> {
  static constexpr auto prefix{ "a{"sv };
  static constexpr auto postfix{ "}"sv };
  static constexpr auto value{
    details::join_v<prefix, signature_v<typename dict_t::key_type>, signature_v<typename dict_t::mapped_type>, postfix>
  };
};
static_assert(signature_v<std::map<int, std::string>> == "a{is}"sv);
static_assert(signature_v<std::unordered_map<int, std::string>> == "a{is}"sv);
static_assert(signature_v<std::map<std::string, std::tuple<int, std::string>>> == "a{s(is)}"sv);
// nested dicts
static_assert(signature_v<std::map<std::string, std::map<std::string, std::string>>> == "a{sa{ss}}"sv);

template <has_signature... types_t>
std::string_view constexpr composed_signature(types_t&&... types) {
  return {};  // todo implement
}

}  // namespace adbus::protocol::type
