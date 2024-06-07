#pragma once

#include <cstdint>
#include <expected>
#include <string_view>
#include <variant>
#include <array>
#include <cassert>

#include <glaze/core/common.hpp>
#include <glaze/core/meta.hpp>
#include <glaze/reflection/to_tuple.hpp>
#include <glaze/concepts/container_concepts.hpp>
#include <glaze/util/tuple.hpp>

#include <adbus/protocol/path.hpp>
#include <adbus/util/string_literal.hpp>
#include <adbus/util/concepts.hpp>

namespace adbus::protocol::type {

using std::string_view_literals::operator""sv;

struct signature {
  constexpr explicit signature(std::string_view sv) noexcept {
    assert(sv.size() <= 255 && "signature size must be less than 255");
    size_ = sv.size();
    std::copy(std::begin(sv), std::end(sv), std::begin(data_));
  }
  constexpr explicit operator std::string_view() const noexcept { return { data_.data(), size_ }; }
  [[nodiscard]] constexpr auto size() const noexcept -> std::uint8_t { return size_; }
  [[nodiscard]] constexpr auto data() const noexcept -> decltype(auto) { return data_.data(); }
  static constexpr auto dbus_signature{ true }; // flag to indicate this is a dbus signature for concept
  std::uint8_t size_{};
  std::array<char, 255> data_{};
};

template <typename T = void>
struct signature_meta : std::false_type {};

template <typename type_t>
static constexpr auto signature_v{ signature_meta<type_t>::value };

template <>
struct signature_meta<std::uint8_t> {
  static constexpr auto value{ "y"sv };
};

template <>
struct signature_meta<bool> {
  static constexpr auto value{ "b"sv };
};

template <>
struct signature_meta<std::int16_t> {
  static constexpr auto value{ "n"sv };
};

template <>
struct signature_meta<std::uint16_t> {
  static constexpr auto value{ "q"sv };
};

template <>
struct signature_meta<std::int32_t> {
  static constexpr auto value{ "i"sv };
};

template <>
struct signature_meta<std::uint32_t> {
  static constexpr auto value{ "u"sv };
};

template <>
struct signature_meta<std::int64_t> {
  static constexpr auto value{ "x"sv };
};

template <>
struct signature_meta<std::uint64_t> {
  static constexpr auto value{ "t"sv };
};

template <std::same_as<double> value_t>
struct signature_meta<value_t> {
  static constexpr auto value{ "d"sv };
};

// unix_fd ?

template <glz::detail::string_like type_t>
struct signature_meta<type_t> {
  static constexpr auto value{ "s"sv };
};

template <>
struct signature_meta<path> {
  static constexpr auto value{ "o"sv };
};

template <typename type_t>
concept has_signature = requires {
  signature_meta<type_t>::value;
  requires std::same_as<std::remove_const_t<decltype(signature_meta<type_t>::value)>, std::string_view>;
};

template <has_signature type_t, has_signature... types_t>
struct signature_meta<std::variant<type_t, types_t...>> {
  static constexpr auto value{ "v"sv };
};

template <std::ranges::range type_t>
  requires has_signature<std::ranges::range_value_t<type_t>>
struct signature_meta<type_t> {
  static constexpr auto value{ util::join_v<util::chars<"a">, signature_v<std::ranges::range_value_t<type_t>>> };
};

template <typename tuple_t>
  requires glz::is_std_tuple<tuple_t> // glz::detail::tuple_t<tuple_t> ||
struct signature_meta<tuple_t> {
  template <typename type_t>
  struct join_impl;
  template <typename... types_t>
  struct join_impl<std::tuple<types_t...>> {
    static constexpr auto value{ util::join_v<util::chars<"(">, signature_v<types_t>..., util::chars<")">> };
  };
  static constexpr auto value{ join_impl<tuple_t>::value };
};

template <glz::detail::map_subscriptable dict_t>
  requires adbus::type::basic<typename dict_t::key_type>
struct signature_meta<dict_t> {
  static constexpr auto value{
    util::join_v<util::chars<"a{">, signature_v<typename dict_t::key_type>, signature_v<typename dict_t::mapped_type>, util::chars<"}">>
  };
};

template<typename T>
static consteval bool assert_signature() {
#if __cpp_static_assert >= 202306L
  using glz::name_v;
  using util::join_v;
  using util::chars;
  static_assert(has_signature<T>, join_v<chars<"Missing signature for given type: \"">, name_v<T>, chars<"\", please consider adding it">>);
#else
  static_assert(has_signature<T>, "type does not have a signature");
#endif
  return true;
}

template <glz::detail::reflectable T>
struct signature_meta<T> {
  template <typename... Ts>
  static consteval std::string_view unwrap_tuple(std::tuple<Ts...>) noexcept {
    using util::chars;
    (assert_signature<std::decay_t<Ts>>(), ...);
    return util::join_v<chars<"(">, signature_v<std::decay_t<Ts>>..., chars<")">>;
  }
  static consteval std::string_view impl() noexcept {
    return unwrap_tuple(glz::detail::to_tuple(T{}));
  }
  static constexpr auto value{ impl() };
};

template <glz::detail::glaze_t T>
struct signature_meta<T> {
  // Example of a glaze Object
  // glz::detail::Object<glz::tuplet::tuple<
  //   glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, int my_struct3::*>,
  //   glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > my_struct3::*>,
  //   glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, unsigned char my_struct3::*>,
  //   glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, adbus::protocol::type::my_struct my_struct3::*> > >;
  template <typename unused, typename member_pointer, typename... Ts>
  static consteval std::size_t unwrap_inner_tuple_size(glz::tuplet::tuple<unused, member_pointer, Ts...>) noexcept {
    using member_type = typename glz::member_value<member_pointer>::type;
    assert_signature<std::decay_t<member_type>>();
    return signature_v<std::decay_t<member_type>>.size();
  }
  template <std::size_t N, typename unused, typename member_pointer, typename... Ts>
  static consteval void unwrap_inner_tuple(std::array<char, N>& buffer, std::size_t& idx, glz::tuplet::tuple<unused, member_pointer, Ts...>) noexcept {
    using member_type = typename glz::member_value<member_pointer>::type;
    assert_signature<std::decay_t<member_type>>();
    constexpr std::string_view signature{ signature_v<std::decay_t<member_type>> };
    std::copy(std::begin(signature), std::end(signature), std::begin(buffer) + idx);
    idx += signature.size();
  }
  template <typename... Ts>
  static consteval auto unwrap_tuple(glz::tuplet::tuple<Ts...>) noexcept {
    using util::join_v;
    constexpr std::size_t N{ (unwrap_inner_tuple_size(Ts{}) + ...) };
    // N + 3 for () and null terminator
    std::array<char, N + 3> buffer{};
    buffer[0] = '(';
    buffer[N + 1] = ')';
    std::size_t idx{ 1 };
    (unwrap_inner_tuple(buffer, idx, Ts{}), ...);
    return buffer;
  }
  static consteval auto impl() noexcept {
    constexpr auto wrapper = glz::meta_wrapper_v<T>;
    if constexpr (glz::detail::glaze_object_t<T>) {
      return unwrap_tuple(wrapper.value);
    }
    else if constexpr (glz::detail::glaze_enum_t<T>) {
      // example glz::enumerate("a", a, "b", b, "c", c);
      // this is represented as string
      return std::array{ 's', '\0' };
    }
    else {
#if __cpp_static_assert >= 202306L
      using util::join_v;
      using util::chars;
      static_assert(glz::false_v<T>, join_v<chars<"Todo implement signature for glaze type: \"">, glz::name_v<T>, chars<"\" other glaze types">>);
#else
      static_assert(glz::false_v<T>, "Todo implement signature for other glaze types");
#endif
    }
  }
  static constexpr auto static_arr{ impl() };
  static constexpr std::string_view value{static_arr.data(), static_arr.size() - 1};
};

template <typename T>
concept enum_c = std::is_enum_v<T> && !glz::detail::glaze_enum_t<T>;

// Todo make it optionally number or string
template <enum_c T>
struct signature_meta<T> {
  static constexpr auto value{ signature_v<std::underlying_type_t<T>> };
};

template <has_signature... types_t>
std::string_view constexpr composed_signature(types_t&&... types) {
  return {};  // todo implement
}

}  // namespace adbus::protocol::type
