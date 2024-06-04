#pragma once

#include <algorithm>
#include <expected>
#include <type_traits>

#include <glaze/concepts/container_concepts.hpp>

#include <adbus/core/context.hpp>

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
template <typename T>
concept trivially_copyable = std::is_trivially_copyable_v<T>;

constexpr void dbus_marshall(trivially_copyable auto&& value,[[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
  using V = std::decay_t<decltype(value)>;
  constexpr auto n = sizeof(V);
  if constexpr (glz::resizable<std::decay_t<decltype(buffer)>>) {
    if (idx + n > buffer.size()) [[unlikely]] {
      buffer.resize((std::max)(buffer.size() * 2, idx + n));
    }
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
    std::copy_n(&temp, n, buffer.data() + idx);
  } else {
    std::copy_n(&value, n, buffer.data() + idx);
  }

  idx += n;
}


template <typename type_t>
struct to_dbus_binary;

template <>
struct to_dbus_binary<bool>
{
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept
  {

  }
};

template <>
struct to_dbus_binary<std::uint8_t>
{
  template <options Opts>
  static constexpr void op(auto&& value,[[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept
  {
    dbus_marshall(std::forward<decltype(value)>(value), std::forward<decltype(ctx)>(ctx), std::forward<decltype(buffer)>(buffer), std::forward<decltype(idx)>(idx));
  }
};

}  // namespace detail

constexpr auto write_dbus_binary(auto&& value, auto&& buffer) noexcept -> error
{
  context ctx{};
  std::size_t idx{ 0 };
  detail::to_dbus_binary<std::decay_t<decltype(value)>>::template op<{}>(std::forward<decltype(value)>(value), ctx, buffer, idx);
  return error{.code = error_code::no_error};
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

namespace test {

static constexpr auto uint8_test { adbus::protocol::write_dbus_binary<std::array<std::uint8_t, 1>>(std::uint8_t{ 0x66 }) };
static_assert(uint8_test.has_value());
static_assert(uint8_test.value()[0] == 0x66);

}
