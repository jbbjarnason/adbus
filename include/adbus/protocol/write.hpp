#pragma once

#include <cstring>
#include <expected>
#include <type_traits>

#include <glaze/concepts/container_concepts.hpp>
#include <glaze/core/common.hpp>
#include <glaze/core/reflection_tuple.hpp>
#include <glaze/util/variant.hpp>

#include <adbus/core/context.hpp>
#include <adbus/util/concepts.hpp>
#include <adbus/protocol/signature.hpp>
#include <adbus/protocol/padding.hpp>

namespace adbus::protocol {

namespace detail {

template <typename type_t>
struct write;

// clang-format off
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
// clang-format on

constexpr void resize(auto&& buffer, auto&& idx, auto&& n) noexcept {
  if (idx + n > buffer.size()) [[unlikely]] {
    buffer.resize((std::max)(buffer.size() * 2, idx + n));
  }
}

template <typename T>
constexpr void pad(auto&& buffer, auto&& idx) noexcept {
  constexpr auto alignment{ padding<T>::value };
  // idx % alignment: This computes the offset of idx from the nearest previous alignment boundary.
  // alignment - (idx % alignment): This calculates how much padding is needed to reach the next alignment boundary.
  // (alignment - (idx % alignment)) % alignment: This ensures that if idx is already aligned (i.e., idx % alignment == 0),
  // the padding is 0 instead of alignment.
  const auto padding = (alignment - (idx % alignment)) % alignment;
  resize(buffer, idx, padding);
  std::memset(buffer.data() + idx, 0, padding);
  idx += padding;
}

constexpr void dbus_marshall(arithmetic auto&& value,
                             [[maybe_unused]] is_context auto&& ctx,
                             auto&& buffer,
                             auto&& idx) noexcept {
  using V = std::decay_t<decltype(value)>;
  pad<V>(buffer, idx);
  constexpr auto n = sizeof(V);
  if constexpr (glz::resizable<std::decay_t<decltype(buffer)>>) {
    resize(buffer, idx, n);
  } else {
    if (idx + n > buffer.size()) [[unlikely]] {
      ctx.err = error{ .code = error_code::buffer_too_small };
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

template <typename T>
struct to_dbus_binary : std::false_type {};

template <>
struct to_dbus_binary<bool> {
  template <options Opts>
  static constexpr void op(auto&& value, auto&&... args) noexcept {
    dbus_marshall(static_cast<std::uint32_t>(value), std::forward<decltype(args)>(args)...);
  }
};

template <num_t number_t>
struct to_dbus_binary<number_t> {
  template <options Opts>
  static constexpr void op(auto&&... args) noexcept {
    dbus_marshall(std::forward<decltype(args)>(args)...);
  }
};

template <typename T>
  requires(std::is_enum_v<T> && !glz::detail::glaze_enum_t<T>)
struct to_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, auto&&... args) noexcept {
    to_dbus_binary<std::underlying_type_t<T>>::template op<Opts>(std::to_underlying(value), std::forward<decltype(args)>(args)...);
  }
};

template <string_like string_t>
struct to_dbus_binary<string_t> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]] {
      ctx.err = error{ .code = error_code::string_too_long };
      return;
    }
    const auto n{ static_cast<std::uint32_t>(value.size()) };
    dbus_marshall(n, ctx, buffer, idx);
    // todo for fixed buffer, we need to check if we still have space
    // +1 for the null terminator
    resize(buffer, idx, n + 1);
    std::memcpy(buffer.data() + idx, value.data(), n);
    idx += n;
    buffer[idx++] = '\0';
  }
};

template <typename T>
 requires(std::is_enum_v<T> && glz::detail::glaze_enum_t<T>)
struct to_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept {
    // copy from glaze json/write.hpp
    using key_t = std::underlying_type_t<T>;
    static constexpr auto frozen_map = glz::detail::make_enum_to_string_map<T>();
    const auto& member_it = frozen_map.find(static_cast<key_t>(value));
    if (member_it != frozen_map.end()) {
      const std::string_view str = {member_it->second.data(), member_it->second.size()};
      to_dbus_binary<std::string_view>::op<Opts>(str, ctx, std::forward<decltype(args)>(args)...);
    }
    else [[unlikely]] {
      ctx.err = error{ .code = error_code::invalid_enum_conversion };
    }
  }
};

template <adbus::type::is_signature sign_t>
struct to_dbus_binary<sign_t> {
  template <options Opts>
  static constexpr void op(auto&& value, [[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    const auto n{ static_cast<std::uint8_t>(value.size()) };
    dbus_marshall(n, ctx, buffer, idx);
    resize(buffer, idx, n + 1);
    std::memcpy(buffer.data() + idx, value.data(), n);
    idx += n;
    buffer[idx++] = '\0';
  }
};

template <container T>
struct to_dbus_binary<T> {
  // Arrays are marshalled as a UINT32 n giving the length of the array data in bytes
  // n does not include the padding after the length, or any padding after the last element. i.e. n should be divisible by
  // the number of elements in the array
  template <options Opts>
  static constexpr void op(auto&& value, [[maybe_unused]] is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    if constexpr (map_like<T>) {
      // the first single complete type (the "key") must be a basic type rather than a container type
#if __cpp_static_assert >= 202306L
      using glz::name_v;
      using util::join_v;
      using util::chars;
      static_assert(adbus::type::basic<std::decay_t<typename T::key_type>>, join_v<chars<"The key type of the map-like type \"">, name_v<T>, chars<"\" must be a basic type as per dbus spec">>);
#else
      static_assert(adbus::type::basic<std::decay_t<typename T::key_type>>, "The key type of the map-like type must be a basic type as per dbus spec");
#endif
    }

    std::uint32_t size_placeholder{};
    pad<decltype(size_placeholder)>(buffer, idx); // let's manually pad here to get the REAL index of the placeholder
    const auto placeholder_idx{ idx };
    dbus_marshall(size_placeholder, ctx, buffer, idx);
    pad<typename std::decay_t<decltype(value)>::value_type>(buffer, idx);  // n does not include the padding after the length
    const auto beginning_of_data_idx{ idx };
    for (const auto& v : value) {
      to_dbus_binary<std::decay_t<decltype(v)>>::template op<Opts>(v, ctx, buffer, idx);
    }
    const auto bytes{ idx - beginning_of_data_idx };
    if (bytes > std::numeric_limits<std::uint32_t>::max()) [[unlikely]] {
      ctx.err = error{ .code = error_code::array_too_long };
      return;
    }
    const auto n{ static_cast<std::uint32_t>(bytes) };
    std::memcpy(buffer.data() + placeholder_idx, &n, sizeof(n));
  }
};

template <glz::detail::pair_t T>
struct to_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& pair, [[maybe_unused]] is_context auto&& ctx, auto&&... args) noexcept {
    // A struct must start on an 8-byte boundary regardless of the type of the struct fields.
    // DICT_ENTRY          Identical to STRUCT.
    pad<std::uint64_t>(args...);
    const auto& [key, value]{ pair };
    to_dbus_binary<typename T::first_type>::template op<Opts>(key, ctx, args...);
    to_dbus_binary<typename T::second_type>::template op<Opts>(value, ctx, args...);
  }
};

template <glz::is_variant T>
struct to_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& variant, auto&&... args) noexcept {
    std::visit([&](auto&& value) {
      using V = std::decay_t<decltype(value)>;
      constexpr auto signature{ type::signature(type::signature_v<V>) };
      to_dbus_binary<decltype(signature)>::op<Opts>(signature, args...);
      to_dbus_binary<V>::template op<Opts>(value, args...);
    }, variant);
  }
};

template <typename T>
  requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
struct to_dbus_binary<T> {
  static constexpr auto N = glz::reflection_count<T>;

  // mostly copied from glaze binary/write.hpp
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    decltype(auto) t = glz::detail::reflection_tuple<T>(value);
    // A struct must start on an 8-byte boundary regardless of the type of the struct fields
    pad<std::uint64_t>(buffer, idx);
    glz::for_each<N>([&](auto I) {
      using Element = glz::detail::glaze_tuple_element<I, N, T>;
      static constexpr size_t member_index = Element::member_index;
      using val_t = std::remove_cvref_t<typename Element::type>;
      if constexpr (std::same_as<val_t, glz::hidden> || std::same_as<val_t, glz::skip>) {
        return;
      } else {
        decltype(auto) member = [&]() -> decltype(auto) {
          if constexpr (glz::detail::reflectable<T>) {
            return std::get<I>(t);
          } else {
            return glz::get<member_index>(glz::get<I>(glz::meta_v<std::decay_t<T>>));
          }
        }();
        auto& member_ref = glz::detail::get_member(value, member);
        to_dbus_binary<std::decay_t<decltype(member_ref)>>::template op<Opts>(member_ref, ctx, buffer, idx);
      }
    });
  }
};

template <glz::detail::glaze_value_t T>
struct to_dbus_binary<T> {
  // mostly copied from glaze binary/write.hpp
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& buffer, auto&& idx) noexcept {
    using V = decltype(glz::detail::get_member(std::declval<T>(), glz::meta_wrapper_v<T>));
    to_dbus_binary<std::decay_t<V>>::template op<Opts>(glz::detail::get_member(value, glz::meta_wrapper_v<T>), ctx, buffer, idx);
  }
};

}  // namespace detail

constexpr auto write_dbus_binary(auto&& value, auto&& buffer) noexcept -> error {
  context ctx{};
  std::size_t idx{ buffer.size() };
  detail::to_dbus_binary<std::decay_t<decltype(value)>>::template op<{}>(std::forward<decltype(value)>(value), ctx, buffer,
                                                                         idx);
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

}  // namespace adbus::protocol
