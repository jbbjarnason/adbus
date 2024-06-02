#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

#include "string_view_join.hpp"
#include "path.hpp"

// for testing
#include <array>
#include <concepts.hpp>
#include <set>
#include <tuple>
#include <glaze/tuplet/tuple.hpp>
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
