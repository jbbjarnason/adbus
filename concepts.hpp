#pragma once

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

namespace type {
// The fixed types are basic types whose values have a fixed length,
// namely BYTE, BOOLEAN, DOUBLE, UNIX_FD, and signed or unsigned integers of length 16, 32 or 64 bits.
template <typename type_t>
concept fixed = [] {
  using t = std::remove_cvref_t<type_t>;
  return requires {
    requires explicit_unsigned_integral<std::uint8_t, t> || std::same_as<t, bool> || explicit_signed_integral<std::int16_t, t> ||
        explicit_unsigned_integral<std::uint16_t, t> || explicit_signed_integral<std::int32_t, t> || explicit_unsigned_integral<std::uint32_t, t> ||
        explicit_signed_integral<std::int64_t, t> || explicit_unsigned_integral<std::uint64_t, t> || std::same_as<t, double>;
  };
}();
static_assert(!fixed<signed char>);  // int
static_assert(fixed<std::uint8_t>);
static_assert(fixed<bool>);
static_assert(fixed<std::int16_t>);
static_assert(fixed<std::uint16_t>);
static_assert(fixed<std::int32_t>);
static_assert(fixed<std::uint32_t>);
static_assert(fixed<std::int64_t>);
static_assert(fixed<std::uint64_t>);
static_assert(fixed<double>);
}  // namespace type

// from
// https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector
template <class, template <class...> class>
constexpr bool is_specialization_v = false;

template <template <class...> class T, class... Args>
constexpr bool is_specialization_v<T<Args...>, T> = true;

}  // namespace adbus::concepts
