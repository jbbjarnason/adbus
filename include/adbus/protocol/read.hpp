#pragma once

#include <glaze/util/expected.hpp>

#include <adbus/core/context.hpp>
#include <adbus/util/concepts.hpp>

namespace adbus::protocol {

namespace detail {

template <typename T>
struct from_dbus_binary;

template <num_t T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept {
    using V = std::decay_t<decltype(value)>;
    if (it + sizeof(V) > end) [[unlikely]] {
      ctx.err = error{ error_code::out_of_range };
      return;
    }
    std::memcpy(&value, it, sizeof(V));
    it += sizeof(V);
  }
};

template <typename T>
  requires(std::is_enum_v<T> && !glz::detail::glaze_enum_t<T>)
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, auto&&... args) noexcept {
    using V = std::underlying_type_t<T>;
    // todo reinterpret_cast is not nice, but it provides less code bloat
    from_dbus_binary<V>::template op<Opts>(reinterpret_cast<V&>(value), std::forward<decltype(args)>(args)...);
  }
};

template <>
struct from_dbus_binary<bool> {
  template <options Opts>
  static constexpr void op(auto&& value, auto&&... args) noexcept {
    std::uint32_t substitute{};
    from_dbus_binary<std::uint32_t>::template op<Opts>(substitute, std::forward<decltype(args)>(args)...);
    value = substitute != 0;
  }
};

template <string_like T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept {
    std::uint32_t size{};
    from_dbus_binary<std::uint32_t>::template op<Opts>(size, ctx, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    if (it + size > end) [[unlikely]] {
      ctx.err = error{ error_code::out_of_range };
      return;
    }
    using V = std::decay_t<decltype(value)>;
    if constexpr (glz::resizable<V>) {
      value.resize(size);
      std::memcpy(value.data(), it, size);
    }
    else if constexpr (glz::is_specialization_v<V, std::basic_string_view>) {
      using char_type = typename V::value_type;
      value = std::basic_string_view<char_type>{ reinterpret_cast<const char_type*>(it), size };
    }
    else {
      static_assert(glz::false_v<V>, "unsupported type");
    }
    it += size + 1;  // the +1 is for the null terminator
  }
};

}  // namespace detail

template <typename T, typename Buffer>
[[nodiscard]] constexpr auto read_dbus_binary(T&& value, Buffer&& buffer) noexcept -> error {
  context ctx{};
  detail::from_dbus_binary<std::decay_t<T>>::template op<{}>(value, ctx, std::begin(buffer), std::end(buffer));
  return ctx.err;
}

template <typename T, class Buffer>
[[nodiscard]] constexpr auto read_binary(Buffer&& buffer) noexcept -> glz::expected<T, error> {
  T value{};
  auto err = read_dbus_binary(value, buffer);
  if (err) [[unlikely]] {
    return glz::unexpected(err);
  }
  return value;
}

}  // namespace adbus::protocol
