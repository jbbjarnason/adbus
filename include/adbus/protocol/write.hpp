#pragma once

#include <cstring>
#include <expected>
#include <type_traits>

#include <glaze/concepts/container_concepts.hpp>

#include <adbus/core/context.hpp>
#include <adbus/util/concepts.hpp>

namespace adbus::protocol {

namespace detail {

template <typename type_t>
struct write;

/**
 * Summary of D-Bus Marshalling
 *
 * Given all this, the types are marshaled on the wire as follows:
 *
 * Conventional Name   Encoding                                                                     Alignment
 * ----------------------------------------------------------------------------------------------------------
 * INVALID             Not applicable; cannot be marshaled.                                         N/A
 * BYTE                A single 8-bit byte.                                                         1
 * BOOLEAN             As for UINT32, but only 0 and 1 are valid values.                            4
 * INT16               16-bit signed integer in the message's byte order.                           2
 * UINT16              16-bit unsigned integer in the message's byte order.                         2
 * INT32               32-bit signed integer in the message's byte order.                           4
 * UINT32              32-bit unsigned integer in the message's byte order.                         4
 * INT64               64-bit signed integer in the message's byte order.                           8
 * UINT64              64-bit unsigned integer in the message's byte order.                         8
 * DOUBLE              64-bit IEEE 754 double in the message's byte order.                          8
 * STRING              A UINT32 indicating the string's length in bytes excluding its
 *                     terminating nul, followed by non-nul string data of the given length,
 *                     followed by a terminating nul byte.                                          4 (for the length)
 * OBJECT_PATH         Exactly the same as STRING except the content must be a valid object path.   4 (for the length)
 * SIGNATURE           The same as STRING except the length is a single
 * byte (thus signatures have a maximum length of 255) and the content must be a valid signature.  1
 * ARRAY               A UINT32 giving the length of the array data in bytes, followed by
 *                     alignment padding to the alignment boundary of the array element type,
 *                     followed by each array element.                                             4 (for the length)
 * STRUCT              A struct must start on an 8-byte boundary regardless of the type of the
 *                     struct fields. The struct value consists of each field marshaled in sequence
 *                     starting from that 8-byte alignment boundary.                               8
 * VARIANT             The marshaled SIGNATURE of a single complete type, followed by a marshaled
 *                     value with the type given in the signature.                                 1 (alignment of the signature)
 * DICT_ENTRY          Identical to STRUCT.                                                        8
 * UNIX_FD             32-bit unsigned integer in the message's byte order. The actual file
 *                     descriptors need to be transferred out-of-band via some platform specific
 *                     mechanism. On the wire, values of this type store the index to the file
 *                     descriptor in the array of file descriptors that accompany the message.     4
 *
 * As an exception to natural alignment, STRUCT and DICT_ENTRY values are always aligned to an 8-byte boundary, regardless of the alignments of their contents.
*/

constexpr void resize(auto&& buffer, auto&& idx, auto&& n) noexcept {
  if (idx + n > buffer.size()) [[unlikely]] {
    buffer.resize((std::max)(buffer.size() * 2, idx + n));
  }
}

template <typename T>
struct padding : std::false_type {};

template <glz::detail::num_t T>
struct padding<T> {
  static constexpr std::size_t value{ sizeof(T) };
};

template <glz::detail::string_like T>
struct padding<T> {
  static constexpr std::size_t value{ sizeof(std::uint32_t) };
};

template <adbus::type::is_signature T>
struct padding<T> {
  static constexpr std::size_t value{ sizeof(std::uint8_t) };
};

template <typename T>
constexpr void pad(auto&& buffer, auto&& idx) noexcept {
  constexpr auto alignment{ padding<T>::value };
  // idx % alignment: This computes the offset of idx from the nearest previous alignment boundary.
  // alignment - (idx % alignment): This calculates how much padding is needed to reach the next alignment boundary.
  // (alignment - (idx % alignment)) % alignment: This ensures that if idx is already aligned (i.e., idx % alignment == 0), the padding is 0 instead of alignment.
  const auto padding = (alignment - (idx % alignment)) % alignment;
  resize(buffer, idx, padding);
  std::memset(buffer.data() + idx, 0, padding);
  idx += padding;
}

constexpr void dbus_marshall(arithmetic auto&& value,[[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
  using V = std::decay_t<decltype(value)>;
  pad<V>(buffer, idx);
  constexpr auto n = sizeof(V);
  if constexpr (glz::resizable<std::decay_t<decltype(buffer)>>) {
    resize(buffer, idx, n);
  }
  else {
    if (idx + n > buffer.size()) [[unlikely]] {
      ctx.err = error{.code = error_code::buffer_too_small};
      return;
    }
  }

  constexpr auto is_volatile = std::is_volatile_v<std::remove_reference_t<decltype(value)>>;

  if constexpr (is_volatile) {
    const V temp{ value };
    std::memcpy(buffer.data() + idx, &temp, n);
  } else {
    std::memcpy(buffer.data() + idx, &value, n);
  }

  idx += n;
}

template <typename type_t>
struct to_dbus_binary : std::false_type {};

template <>
struct to_dbus_binary<bool>
{
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept
  {
    dbus_marshall(static_cast<std::uint32_t>(value), std::forward<decltype(ctx)>(ctx), std::forward<decltype(buffer)>(buffer), std::forward<decltype(idx)>(idx));
  }
};

template <glz::detail::num_t number_t>
struct to_dbus_binary<number_t>
{
  template <options Opts>
  static constexpr void op(auto&& value,[[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept
  {
    dbus_marshall(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), std::forward<decltype(buffer)>(buffer), std::forward<decltype(idx)>(idx));
  }
};

template <glz::detail::string_like string_t>
struct to_dbus_binary<string_t> {
  template <options Opts>
  static constexpr void op(auto&& value,[[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]] {
      ctx.err = error{.code = error_code::string_too_long};
      return;
    }
    const auto n{ static_cast<std::uint32_t>(value.size()) };
    dbus_marshall(n, ctx, buffer, idx);
    // +1 for the null terminator
    resize(buffer, idx, n + 1);
    std::memcpy(buffer.data() + idx, value.data(), n);
    idx += n;
    buffer[idx++] = '\0';
  }
};

template <adbus::type::is_signature sign_t>
struct to_dbus_binary<sign_t> {
  template <options Opts>
  static constexpr void op(auto&& value,[[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    const auto n{ static_cast<std::uint8_t>(value.size()) };
    dbus_marshall(n, ctx, buffer, idx);
    resize(buffer, idx, n + 1);
    std::memcpy(buffer.data() + idx, value.data(), n);
    idx += n;
    buffer[idx++] = '\0';
  }
};

template <string_like T>
constexpr auto calculate_size_in_bytes(T&& value) noexcept -> std::size_t {
  // sizeof(std::uint32_t) for the length
  // + value.size() for the string
  // + 1 for the null terminator
  return sizeof(std::uint32_t) + value.size() + 1;
}

template <arithmetic T>
constexpr auto calculate_size_in_bytes(T&&) noexcept -> std::size_t {
  return sizeof(T);
}

template <single_element_container T>
    requires has_size<typename std::decay_t<T>::value_type>
constexpr auto calculate_size_in_bytes(T&& value) noexcept -> std::size_t {
  std::size_t n{};
  for (const auto& v : value) {
    n += calculate_size_in_bytes(v);
  }
  return n;
}

template <single_element_container T>
  requires arithmetic<typename std::decay_t<T>::value_type>
constexpr auto calculate_size_in_bytes(T&& value) noexcept -> std::size_t {
  using value_type = typename std::decay_t<T>::value_type;
  return sizeof(value_type) * value.size();
}

// example: std::vector<std::uint64_t>{ 10, 20, 30 };
// little endian
// | Length (UINT32) | Padding     | Element 1 (UINT64)        | Element 2 (UINT64)        | Element 3 (UINT64)        |
// |    4 bytes      | 4 bytes     |      8 bytes              |      8 bytes              |      8 bytes              |
// |  18 00 00 00    | 00 00 00 00 | 0A 00 00 00 00 00 00 00   | 14 00 00 00 00 00 00 00   | 1E 00 00 00 00 00 00 00   |
// |      24         |      0      |          10               |          20               |            30             |
template <single_element_container iterable_t>
struct to_dbus_binary<iterable_t> {
  template <options Opts>
  static constexpr void op(auto&& value,[[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    const auto bytes{ calculate_size_in_bytes(value) };
    dbus_marshall(bytes, ctx, buffer, idx);
    // since we have calculated the bytes needed, let's resize accordingly
    resize(buffer, idx, bytes);
    for (const auto& v : value) {
      to_dbus_binary<std::decay_t<decltype(v)>>::template op<Opts>(v, ctx, buffer, idx);
    }
  }
};

}  // namespace detail

constexpr auto write_dbus_binary(auto&& value, auto&& buffer) noexcept -> error
{
  context ctx{};
  std::size_t idx{ buffer.size() };
  detail::to_dbus_binary<std::decay_t<decltype(value)>>::template op<{}>(std::forward<decltype(value)>(value), ctx, buffer, idx);
  if constexpr (glz::resizable<std::decay_t<decltype(buffer)>>) {
    buffer.resize(idx);
  }
  return ctx.err;
}

template <typename buffer_t = std::string>
constexpr auto write_dbus_binary(auto&& value) noexcept -> std::expected<buffer_t, error> {
  buffer_t buffer{};
  if (auto err = write_dbus_binary(std::forward<decltype(value)>(value), buffer)) {
    return std::unexpected(err);
  }
  return buffer;
}

}
