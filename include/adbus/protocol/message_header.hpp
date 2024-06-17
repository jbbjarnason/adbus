#pragma once

#include <bit>
#include <bitset>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <glaze/core/common.hpp>

#include <adbus/protocol/name.hpp>
#include <adbus/protocol/path.hpp>
#include <adbus/protocol/signature.hpp>
#include <adbus/protocol/read.hpp>

namespace glz {
template <typename T>
struct meta;
}  // namespace glz

// from https://gitlab.freedesktop.org/dbus/dbus/-/blob/dbus-1.14/dbus/dbus-marshal-header.h?ref_type=heads
/**
 * Message header data and some cached details of it.
 *
 * A message looks like this:
 *
 * @code
 *  | 0     | 1     | 2     | 3    | 4   | 5   | 6   | 7   | <- index % 8
 *  |-------|-------|-------|------|-----|-----|-----|-----|
 *  | Order | Type  | Flags | Vers | Body length           |
 *  | Serial                       | Fields array length  [A]
 * [A] Code |Sig.len| Signature + \0           | Content...| <- first field
 *  | Content ...                  | Pad to 8-byte boundary|
 *  | Code  |Sig.len| Signature + \0     | Content...      | <- second field
 * ...
 *  | Code  |Sig.len| Signature    | Content...            | <- last field
 *  | Content ...  [B] Padding to 8-byte boundary         [C]
 * [C] Body ...                                            |
 * ...
 *  | Body ...              [D]           <- no padding after natural length
 * @endcode
 *
 * Each field is a struct<byte,variant>. All structs have 8-byte alignment,
 * so each field is preceded by 0-7 bytes of padding to an 8-byte boundary
 * (for the first field it happens to be 0 bytes). The overall header
 * is followed by 0-7 bytes of padding to align the body.
 *
 * Key to content, with variable name references for _dbus_header_load():
 *
 * Order: byte order, currently 'l' or 'B' (byte_order)
 * Type: message type such as DBUS_MESSAGE_TYPE_METHOD_CALL
 * Flags: message flags such as DBUS_HEADER_FLAG_NO_REPLY_EXPECTED
 * Vers: D-Bus wire protocol version, currently always 1
 * Body length: Distance from [C] to [D]
 * Serial: Message serial number
 * Fields array length: Distance from [A] to [B] (fields_array_len)
 *
 * To understand _dbus_header_load():
 *
 * [A] is FIRST_FIELD_OFFSET.
 * header_len is from 0 to [C].
 * padding_start is [B].
 * padding_len is the padding from [B] to [C].
 */

namespace adbus::protocol::header {

namespace details {
static constexpr auto serialize_endian() -> char {
  if constexpr (std::endian::native == std::endian::little) {
    return 'l';
  } else if constexpr (std::endian::native == std::endian::big) {
    return 'B';
  } else {
    []<bool flag = false> {
      static_assert(flag, "Mixed endian unsupported");
    };
  }
}
}  // namespace details

enum struct message_type_e : std::uint8_t {
  invalid = 0,    // This is an invalid type.
  method_call,    // Method call. This message type may prompt a reply.
  method_return,  // Method reply with returned data.
  error,          // Error reply. If the first argument exists and is a string, it is an error message.
  signal,         // Signal emission.
};

constexpr auto format_as(message_type_e const& t) noexcept -> std::string_view {
  switch (t) {
    case message_type_e::invalid:
      return "invalid";
    case message_type_e::method_call:
      return "method_call";
    case message_type_e::method_return:
      return "method_return";
    case message_type_e::error:
      return "error";
    case message_type_e::signal:
      return "signal";
    default:
      return "unknown";
  }
}

// enum struct message_field_e : std::uint8_t {
//   invalid = 0,  // Not a valid field name (error if it appears in a message)
//   path,         // The object to send a call to, or the object a signal is emitted from.
//   interface,    // The interface to invoke a method call on, or that a signal is emitted from.
//   member,       // The member, either the method name or signal name.
//   error_name,   // The name of the error that occurred, for errors
//   reply_serial,  // The serial number of the message this message is a reply to. (The serial number is the second UINT32
//   in the header.) destination,   // The name of the connection this message is intended for. sender,        // Unique name
//   of the sending connection. signature,     // The signature of the message body. unix_fds,      // The number of Unix
//   file descriptors that accompany the message.
// };

struct flags_t {
  /// This message does not expect method return replies or error replies, even if it is of a type that can have a reply;
  /// the reply should be omitted.
  bool no_reply_expected : 1 { false };
  /// The bus must not launch an owner for the destination name in response to this message.
  bool no_auto_start : 1 { false };
  /// This flag may be set on a method call message to inform the receiving side that the caller
  /// is prepared to wait for interactive authorization, which might take a considerable time to complete.
  /// For instance, if this flag is set, it would be appropriate to query the user for passwords or
  /// confirmation via Polkit or a similar framework.
  ///
  /// This flag is only useful when unprivileged code calls a more privileged method call,
  /// and an authorization framework is deployed that allows possibly interactive authorization.
  /// If no such framework is deployed it has no effect.
  /// This flag should not be set by default by client implementations.
  /// If it is set, the caller should also set a suitably long timeout on the method call to
  /// make sure the user interaction may complete. This flag is only valid for method call messages,
  /// and shall be ignored otherwise.
  bool allow_interactive_authorization : 1 { false };

  std::uint8_t reserved : 5 { 0 };

  constexpr explicit operator std::byte() const noexcept {
    auto const bits{
      std::bitset<8>{}.set(0, no_reply_expected).set(1, no_auto_start).set(2, allow_interactive_authorization)
    };
    return static_cast<std::byte>(bits.to_ulong());
  }
  constexpr auto operator==(const flags_t&) const noexcept -> bool = default;
};
static_assert(sizeof(flags_t) == 1);
static_assert(std::is_standard_layout_v<flags_t>);
static_assert(static_cast<std::byte>(flags_t{ .no_reply_expected = true }) == std::byte{ 0x1 });
static_assert(static_cast<std::byte>(flags_t{ .no_auto_start = true }) == std::byte{ 0x2 });
static_assert(static_cast<std::byte>(flags_t{ .allow_interactive_authorization = true }) == std::byte{ 0x4 });

constexpr auto format_as(flags_t const& f) noexcept -> std::string {
  return fmt::format("no_reply_expected: {}, no_auto_start: {}, allow_interactive_author: {}", f.no_reply_expected,
                     f.no_auto_start, f.allow_interactive_authorization);
}

template <std::byte code_v, typename variant_t, message_type_e... required_in>
struct header_field {
  using type = variant_t;
  static constexpr std::byte code{ code_v };
  static constexpr std::array<message_type_e, sizeof...(required_in)> required{ required_in... };
  type value{};
  constexpr auto operator==(const header_field&) const noexcept -> bool = default;
};

// The object to send a call to, or the object a signal is emitted from. The special path /org/freedesktop/DBus/Local is
// reserved; implementations should not send messages with this path, and the reference implementation of the bus daemon will
// disconnect any application that attempts to do so. This header field is controlled by the message sender.
using field_path = header_field<std::byte{ 1 }, path, message_type_e::method_call, message_type_e::signal>;
// The interface to invoke a method call on, or that a signal is emitted from. Optional for method calls, required for
// signals. The special interface org.freedesktop.DBus.Local is reserved; implementations should not send messages with this
// interface, and the reference implementation of the bus daemon will disconnect any application that attempts to do so. This
// header field is controlled by the message sender.
using field_interface = header_field<std::byte{ 2 }, interface_name, message_type_e::signal>;
// The member, either the method name or signal name. This header field is controlled by the message sender.
using field_member = header_field<std::byte{ 3 }, member_name, message_type_e::method_call, message_type_e::signal>;
// The name of the error that occurred, for errors
using field_error_name = header_field<std::byte{ 4 }, error_name, message_type_e::error>;
// The serial number of the message this message is a reply to. (The serial number is the second UINT32 in the header.) This
// header field is controlled by the message sender.
using field_reply_serial = header_field<std::byte{ 5 }, std::uint32_t, message_type_e::error, message_type_e::method_return>;
// The name of the connection this message is intended for. This field is usually only meaningful in combination with the
// message bus (see the section called “Message Bus Specification”), but other servers may define their own meanings for it.
// This header field is controlled by the message sender.
using field_destination = header_field<std::byte{ 6 }, bus_name>;
// Unique name of the sending connection. This field is usually only meaningful in combination with the message bus, but
// other servers may define their own meanings for it. On a message bus, this header field is controlled by the message bus,
// so it is as reliable and trustworthy as the message bus itself. Otherwise, this header field is controlled by the message
// sender, unless there is out-of-band information that indicates otherwise.
using field_sender = header_field<std::byte{ 7 }, std::string>;
// The signature of the message body. If omitted, it is assumed to be the empty signature "" (i.e. the body must be
// 0-length). This header field is controlled by the message sender.
using field_signature = header_field<std::byte{ 8 }, type::signature>;
// The number of Unix file descriptors that accompany the message. If omitted, it is assumed that no Unix file descriptors
// accompany the message. The actual file descriptors need to be transferred via platform specific mechanism out-of-band.
// They must be sent at the same time as part of the message itself. They may not be sent before the first byte of the
// message itself is transferred or after the last byte of the message itself. This header field is controlled by the message
// sender.
using field_unix_fds = header_field<std::byte{ 9 }, std::uint32_t>;

struct field {
  // default constructor is only used for reading binary to this struct
  field() = default;
  field(auto&& one_of_variant_t)
      : code{ make_code(one_of_variant_t) }, value{ std::forward<decltype(one_of_variant_t)>(one_of_variant_t) } {}
  using variant_t = std::variant<field_path,
                                 field_interface,
                                 field_member,
                                 field_error_name,
                                 field_reply_serial,
                                 field_destination,
                                 field_sender,
                                 field_signature,
                                 field_unix_fds>;
  const std::byte code{};
  variant_t value;
  constexpr auto operator==(const field&) const noexcept -> bool = default;
private:
  template <class T>
  static constexpr auto make_code(T&&) -> std::byte {
    return std::decay_t<T>::code;
  }
};

constexpr auto format_as(field const& f) noexcept -> std::string {
  return fmt::format("code: {}, value: {}", f.code, std::visit([](auto&& val) { return fmt::format("{}", val); }, f.value));
}

/*
The signature of the header is: "yyyyuua(yv)"
Written out more readably, this is:
BYTE, BYTE, BYTE, BYTE, UINT32, UINT32, ARRAY of STRUCT of (BYTE,VARIANT)
*/

struct header {
  // Endianness flag; ASCII 'l' for little-endian or ASCII 'B' for big-endian. Both header and body are in this endianness.
  const std::byte endian{ details::serialize_endian() };
  // Message type. Unknown types must be ignored.
  message_type_e type{ message_type_e::invalid };
  // Bitwise OR of flags_t.
  flags_t flags{};
  // Major protocol version of the sending application. If the major protocol version of the receiving application does not
  // match, the applications will not be able to communicate and the D-Bus connection must be disconnected. The major
  // protocol version for this version of the specification is 1.
  const std::byte version{ 1 };
  // Length in bytes of the message body, starting from the end of the header. The header ends after its alignment padding to
  // an 8-boundary.
  std::uint32_t body_length{ 0 };
  // The serial of this message, used as a cookie by the sender to identify the reply corresponding to this request. This
  // must not be zero.
  std::uint32_t serial{ 0 };
  // An array of zero or more header fields where the byte is the field code, and the variant is the field value. The message
  // type determines which fields are required.
  std::vector<field> fields{};

  static constexpr auto message_header = true;
  constexpr auto operator==(const header&) const noexcept -> bool = default;
};

constexpr auto format_as(header const& h) noexcept -> std::string {
  return fmt::format("endian: {}, type: {}, flags: {}, version: {}, body_length: {}, serial: {}, fields: {}", h.endian,
                     h.type, h.flags, h.version, h.body_length, h.serial, "foo"
                     // fmt::join(h.fields, ", ")
  );
}

}  // namespace adbus::protocol::header

template <std::byte code_v, typename variant_t, auto... required_in>
struct glz::meta<adbus::protocol::header::header_field<code_v, variant_t, required_in...>> {
  using T = adbus::protocol::header::header_field<code_v, variant_t, required_in...>;
  static constexpr auto value{ &T::value };
};

template <>
struct glz::meta<adbus::protocol::header::field> {
  using T = adbus::protocol::header::field;
  static constexpr auto value{ glz::object("code", &T::code, "value", &T::value) };
};

template <>
struct glz::meta<adbus::protocol::header::header> {
  using T = adbus::protocol::header::header;
  static constexpr auto value{ glz::object(
      "endian",
      &T::endian,
      "type",
      &T::type,
      "flags",
      [](auto&& self) -> auto& { return *reinterpret_cast<const std::uint8_t*>(&self.flags); },
      "version",
      &T::version,
      "body_length",
      &T::body_length,
      "serial",
      &T::serial,
      "fields",
      &T::fields) };
};

namespace adbus::protocol::detail {
template <typename T>
struct from_dbus_binary;

template <>
struct from_dbus_binary<header::field> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    // A struct must start on an 8-byte boundary regardless of the type of the struct fields.
    detail::skip_padding<std::uint64_t>(ctx, begin, it, end);
    from_dbus_binary<decltype(value.code)>::template op<Opts>(value.code, ctx, begin, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    namespace header = adbus::protocol::header;
    auto const extract_variant{ [&]<typename field_type>(field_type&&) {
      type::signature read_signature{};
      from_dbus_binary<type::signature>::template op<Opts>(read_signature, ctx, begin, it, end);
      if (ctx.err) [[unlikely]] {
        return;
      }
      if (read_signature != type::signature_v<field_type>) [[unlikely]] {
        ctx.err = { .code = error_code::unexpected_variant, .index = static_cast<error::index_t>(std::distance(begin, it)) };
        return;
      }
      value.value.template emplace<field_type>();
      from_dbus_binary<decltype(field_type{}.value)>::template op<Opts>(std::get<field_type>(value.value).value, ctx, begin, it, end);
    } };
    switch (value.code) {
      case header::field_path::code: {
        extract_variant(header::field_path{});
        return;
      }
      case header::field_interface::code: {
        extract_variant(header::field_interface{});
        return;
      }
      case header::field_member::code: {
        extract_variant(header::field_member{});
        return;
      }
      case header::field_error_name::code: {
        extract_variant(header::field_error_name{});
        return;
      }
      case header::field_reply_serial::code: {
        extract_variant(header::field_reply_serial{});
        return;
      }
      case header::field_destination::code: {
        extract_variant(header::field_destination{});
        return;
      }
      case header::field_sender::code: {
        extract_variant(header::field_sender{});
        return;
      }
      case header::field_signature::code: {
        extract_variant(header::field_signature{});
        return;
      }
      case header::field_unix_fds::code: {
        extract_variant(header::field_unix_fds{});
        return;
      }
      default:
        ctx.err = { .code = error_code::unexpected_variant, .index = static_cast<error::index_t>(std::distance(begin, it)) };
        return;
    }
  }
};

}  // namespace adbus::protocol::detail
