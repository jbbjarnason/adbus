#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

#include <adbus/protocol/path.hpp>
#include <adbus/util/string_literal.hpp>
#include <adbus/util/concepts.hpp>

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

template <has_signature type_t, has_signature... types_t>
struct signature<std::variant<type_t, types_t...>> {
  static constexpr auto value{ "v"sv };
};

template <std::ranges::range type_t>
  requires has_signature<std::ranges::range_value_t<type_t>>
struct signature<type_t> {
  static constexpr auto value{ util::join_v<util::chars<"a">, signature_v<std::ranges::range_value_t<type_t>>> };
};

template <typename tuple_t>
  requires concepts::is_specialization_v<tuple_t, std::tuple>
struct signature<tuple_t> {
  template <typename type_t>
  struct join_impl;
  template <typename... types_t>
  struct join_impl<std::tuple<types_t...>> {
    static constexpr auto value{ util::join_v<util::chars<"(">, signature_v<types_t>..., util::chars<")">> };
  };
  static constexpr auto value{ join_impl<tuple_t>::value };
};

template <typename dict_t>
  requires(concepts::is_specialization_v<dict_t, std::map> || concepts::is_specialization_v<dict_t, std::unordered_map>) &&
          concepts::type::basic<typename dict_t::key_type>
struct signature<dict_t> {
  static constexpr auto value{
    util::join_v<util::chars<"a{">, signature_v<typename dict_t::key_type>, signature_v<typename dict_t::mapped_type>, util::chars<"}">>
  };
};

template <has_signature... types_t>
std::string_view constexpr composed_signature(types_t&&... types) {
  return {};  // todo implement
}

}  // namespace adbus::protocol::type
