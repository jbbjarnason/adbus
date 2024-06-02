#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace adbus::concepts {
// from glaze, https://github.com/stephenberry/glaze
template <class T>
concept char_t = std::same_as<std::decay_t<T>, char> || std::same_as<std::decay_t<T>, char16_t> ||
                 std::same_as<std::decay_t<T>, char32_t> || std::same_as<std::decay_t<T>, wchar_t>;

template <class T>
concept bool_t = std::same_as<std::decay_t<T>, bool> || std::same_as<std::decay_t<T>, std::vector<bool>::reference>;

template <class T>
concept int_t = std::integral<std::decay_t<T>> && !char_t<std::decay_t<T>> && !bool_t<T>;

template <class T>
concept num_t = std::floating_point<std::decay_t<T>> || int_t<T>;

template <typename presumed_integer_t, typename integer_t>
concept explicit_integral = requires {
  requires int_t<integer_t>;
  requires std::same_as<presumed_integer_t, integer_t>;
};

template <class T>
concept str_t = !std::same_as<std::nullptr_t, T> && std::convertible_to<std::decay_t<T>, std::string_view>;

template <class T>
concept has_push_back = requires(T t, typename T::value_type v) { t.push_back(v); };

// this concept requires that T is string and copies the string in json
template <class T>
concept string_t = str_t<T> && !std::same_as<std::decay_t<T>, std::string_view> && has_push_back<T>;

// this concept requires that T is just a view
template <class T>
concept str_view_t = std::same_as<std::decay_t<T>, std::string_view>;

template <class T>
concept resizeable = requires(T container) { container.resize(0); };

template <class T>
concept erasable = requires(T container) { container.erase(container.cbegin(), container.cend()); };

template <class T>
concept has_size = requires(T container) { container.size(); };

template <class T>
concept has_empty = requires(T container) {
  { container.empty() } -> std::convertible_to<bool>;
};

template <class T>
concept has_data = requires(T container) { container.data(); };

// from https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector
template <class, template <class...> class>
constexpr bool is_specialization_v = false;

template <template <class...> class T, class... Args>
constexpr bool is_specialization_v<T<Args...>, T> = true;

namespace type {
// The fixed types are basic types whose values have a fixed length,
// namely BYTE, BOOLEAN, DOUBLE, UNIX_FD, and signed or unsigned integers of length 16, 32 or 64 bits.
template <typename type_t>
concept fixed = [] {
  using t = std::remove_cvref_t<type_t>;
  return requires { requires num_t<t> && !std::same_as<std::int8_t, t> || std::same_as<t, bool>; };
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
concept string_like = str_t<type_t>;

template <typename type_t>
concept basic = fixed<type_t> || string_like<type_t>;

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

}  // namespace adbus::concepts
