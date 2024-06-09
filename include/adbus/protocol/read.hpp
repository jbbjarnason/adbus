#pragma once

#include <glaze/util/expected.hpp>

#include <adbus/core/context.hpp>

namespace adbus::protocol {

namespace detail {

template <typename T>
struct from_dbus_binary;

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
