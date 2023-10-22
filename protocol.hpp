#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace adbus::concepts {

template <typename presumed_integer_t, typename integer_t>
concept explicit_unsigned_integral = requires {
  requires not std::same_as<std::remove_cv_t<integer_t>, bool>;
  requires std::unsigned_integral<integer_t>;
  requires std::same_as<presumed_integer_t, integer_t>;
};

template <typename presumed_integer_t, typename integer_t>
concept explicit_signed_integral = requires {
  requires not std::same_as<std::remove_cv_t<integer_t>, bool>;
  requires std::signed_integral<integer_t>;
  requires std::same_as<presumed_integer_t, integer_t>;
};

}  // namespace adbus::concepts

namespace adbus::protocol::type {

using std::string_view_literals::operator""sv;

template <typename type_t>
struct signature : std::false_type {};

template <>
struct signature<std::uint8_t> {
  static constexpr auto value{ "y"sv };
};

template <std::same_as<bool> value_t>
struct signature<value_t> {
  static constexpr auto value{ "b"sv };
};

template <concepts::explicit_signed_integral<std::int16_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "n"sv };
};

template <concepts::explicit_unsigned_integral<std::uint16_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "q"sv };
};

template <concepts::explicit_signed_integral<std::int32_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "i"sv };
};

template <concepts::explicit_unsigned_integral<std::uint32_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "u"sv };
};

template <concepts::explicit_signed_integral<std::int64_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "x"sv };
};

template <concepts::explicit_unsigned_integral<std::uint64_t> value_t>
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
  requires std::same_as<std::remove_cvref_t<type_t>, std::filesystem::path>
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
static_assert(has_signature<std::filesystem::path>);

static_assert((not has_signature<char*>));
static_assert((not has_signature<const char*>));
static_assert((not has_signature<std::int8_t>));

static_assert(signature<std::uint8_t>::value == "y"sv);
static_assert(signature<bool>::value == "b"sv);
static_assert(signature<std::int16_t>::value == "n"sv);
static_assert(signature<std::uint16_t>::value == "q"sv);
static_assert(signature<std::int32_t>::value == "i"sv);
static_assert(signature<std::uint32_t>::value == "u"sv);
static_assert(signature<std::int64_t>::value == "x"sv);
static_assert(signature<std::uint64_t>::value == "t"sv);
static_assert(signature<double>::value == "d"sv);
static_assert(signature<std::string_view>::value == "s"sv);
static_assert(signature<std::string>::value == "s"sv);
static_assert(signature<std::filesystem::path>::value == "o"sv);

template <has_signature... types_t>
std::string_view constexpr composed_signature(types_t&&... types) {
  return {}; // todo implement
}

}  // namespace adbus::protocol::type
