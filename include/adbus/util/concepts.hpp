#pragma once

#include <concepts>
#include <cstdint>

#include <glaze/concepts/container_concepts.hpp>

namespace adbus {

namespace detail {

template <typename T, typename = void>
struct has_tuple_size : std::false_type {};

template <typename T>
struct has_tuple_size<T, std::void_t<typename std::tuple_size<T>::type>> : std::true_type {};

template <typename T>
concept fixed_size = has_tuple_size<T>::value &&
                     std::convertible_to<decltype(std::tuple_size<T>::value), std::size_t>;

}  // namespace detail

template <typename T>
concept num_t = glz::detail::num_t<T>;

template <typename T>
concept arithmetic = std::is_arithmetic_v<std::decay_t<T>>;

template <typename T>
concept has_size = glz::has_size<T>;

template <typename T>
concept string_like = glz::detail::string_like<T>;

template <typename T>
concept vector_like = glz::detail::vector_like<T> && !string_like<T>;

template <typename T>
concept map_like = glz::detail::map_subscriptable<T>;

template <typename T>
concept array_like = detail::fixed_size<T> && glz::detail::accessible<T> && glz::has_data<T> && !string_like<T>;

template <typename T>
concept container = glz::range<T> && !string_like<T>;

template <class T>
concept has_clear = requires(T v) { v.clear(); };

template <typename T>
concept is_header = requires {
  { T::message_header } -> std::convertible_to<const bool&>;
};

template <typename T>
concept has_co_await = requires(T t) {
  { operator co_await(t) } -> std::convertible_to<std::coroutine_handle<>>;
};

namespace type {

// check if type has static constexpr boolean named dbus_signature
template <typename T>
concept is_signature = requires {
  requires std::is_class_v<T>;
  requires T::dbus_signature;
};

// The fixed types are basic types whose values have a fixed length,
// namely BYTE, BOOLEAN, DOUBLE, UNIX_FD, and signed or unsigned integers of length 16, 32 or 64 bits.
template <typename type_t>
concept fixed = [] {
  using t = std::remove_cvref_t<type_t>;
  return requires { requires glz::detail::num_t<t> && !std::same_as<std::int8_t, t> || std::same_as<t, bool>; };
}();
static_assert(!fixed<std::int8_t>);  // int8_t is not supported by the specification
static_assert(fixed<std::uint8_t>);
static_assert(fixed<bool>);
static_assert(fixed<std::int16_t>);
static_assert(fixed<std::uint16_t>);
static_assert(fixed<std::int32_t>);
static_assert(fixed<std::uint32_t>);
static_assert(fixed<std::int64_t>);
static_assert(fixed<std::uint64_t>);
static_assert(fixed<double>);

template <typename type_t>
concept basic = fixed<type_t> || glz::detail::string_like<type_t>;

// ARRAY, STRUCT, DICT_ENTRY, VARIANT
// Todo make this more generalized
// template <typename type_t>
// concept container = [] {
//   using t = std::remove_cvref_t<type_t>;
//   return requires {
//     requires is_specialization_v<t, std::vector> || is_specialization_v<t, std::map> || is_specialization_v<t, std::unordered_map> ||
//                  is_specialization_v<t, std::tuple> || is_specialization_v<t, std::variant>;
//   };
// }();

}  // namespace type

}  // namespace adbus
