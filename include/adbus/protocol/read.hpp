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
  static constexpr void read(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept {
  }
};

}  // namespace detail

template <typename T, typename Buffer>
[[nodiscard]] constexpr auto read_dbus_binary(T&& value, Buffer&& buffer) noexcept -> error {

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
