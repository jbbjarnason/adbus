#pragma once

#include <bitset>
#include <bit>
#include <variant>
#include <vector>

#include <adbus/protocol/meta.hpp>
#include <adbus/protocol/name.hpp>
#include <adbus/protocol/path.hpp>

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
}

enum struct message_type_e : std::uint8_t {
  invalid = 0,    // This is an invalid type.
  method_call,    // Method call. This message type may prompt a reply.
  method_return,  // Method reply with returned data.
  error,          // Error reply. If the first argument exists and is a string, it is an error message.
  signal,         // Signal emission.
};
// enum struct message_field_e : std::uint8_t {
//   invalid = 0,  // Not a valid field name (error if it appears in a message)
//   path,         // The object to send a call to, or the object a signal is emitted from.
//   interface,    // The interface to invoke a method call on, or that a signal is emitted from.
//   member,       // The member, either the method name or signal name.
//   error_name,   // The name of the error that occurred, for errors
//   reply_serial,  // The serial number of the message this message is a reply to. (The serial number is the second UINT32 in the header.)
//   destination,   // The name of the connection this message is intended for.
//   sender,        // Unique name of the sending connection.
//   signature,     // The signature of the message body.
//   unix_fds,      // The number of Unix file descriptors that accompany the message.
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
};
static_assert(sizeof(flags_t) == 1);
static_assert(std::is_standard_layout_v<flags_t>);
static_assert(static_cast<std::byte>(flags_t{ .no_reply_expected = true }) == std::byte{ 0x1 });
static_assert(static_cast<std::byte>(flags_t{ .no_auto_start = true }) == std::byte{ 0x2 });
static_assert(static_cast<std::byte>(flags_t{ .allow_interactive_authorization = true }) == std::byte{ 0x4 });

template <std::byte code_v, typename variant_t, message_type_e... required_in>
struct header_field {
  using type = variant_t;
  static constexpr std::byte code{ code_v };
  static constexpr std::array<message_type_e, sizeof...(required_in)> required{ required_in... };
  type value{};
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
using field_destination = header_field<std::byte{ 6 }, std::string>;
// Unique name of the sending connection. This field is usually only meaningful in combination with the message bus, but
// other servers may define their own meanings for it. On a message bus, this header field is controlled by the message bus,
// so it is as reliable and trustworthy as the message bus itself. Otherwise, this header field is controlled by the message
// sender, unless there is out-of-band information that indicates otherwise.
using field_sender = header_field<std::byte{ 7 }, std::string>;
// The signature of the message body. If omitted, it is assumed to be the empty signature "" (i.e. the body must be
// 0-length). This header field is controlled by the message sender.
using field_signature = header_field<std::byte{ 8 }, std::string>;
// The number of Unix file descriptors that accompany the message. If omitted, it is assumed that no Unix file descriptors
// accompany the message. The actual file descriptors need to be transferred via platform specific mechanism out-of-band.
// They must be sent at the same time as part of the message itself. They may not be sent before the first byte of the
// message itself is transferred or after the last byte of the message itself. This header field is controlled by the message
// sender.
using field_unix_fds = header_field<std::byte{ 9 }, std::uint32_t>;

struct field {
  constexpr auto code() -> std::byte {
    return std::visit([]<typename value_t>(value_t&&) -> std::byte {
      return std::decay_t<value_t>::code;
    }, value);
  }
  std::variant<field_path,
               field_interface,
               field_member,
               field_error_name,
               field_reply_serial,
               field_destination,
               field_sender,
               field_signature,
               field_unix_fds>
      value;
};


/*
The signature of the header is: "yyyyuua(yv)"
Written out more readably, this is:
BYTE, BYTE, BYTE, BYTE, UINT32, UINT32, ARRAY of STRUCT of (BYTE,VARIANT)
*/

struct header {
  // Endianness flag; ASCII 'l' for little-endian or ASCII 'B' for big-endian. Both header and body are in this endianness.
  static constexpr auto endian{ details::serialize_endian() };
  // Message type. Unknown types must be ignored.
  message_type_e type{ message_type_e::invalid };
  // Bitwise OR of flags_t.
  flags_t flags{};
  // Major protocol version of the sending application. If the major protocol version of the receiving application does not
  // match, the applications will not be able to communicate and the D-Bus connection must be disconnected. The major
  // protocol version for this version of the specification is 1.
  static constexpr std::byte version{ 1 };
  // Length in bytes of the message body, starting from the end of the header. The header ends after its alignment padding to
  // an 8-boundary.
  std::uint32_t body_length{ 0 };
  // The serial of this message, used as a cookie by the sender to identify the reply corresponding to this request. This
  // must not be zero.
  std::uint32_t serial{ 0 };
  // An array of zero or more header fields where the byte is the field code, and the variant is the field value. The message
  // type determines which fields are required.
  std::vector<field> fields{};
};

}  // namespace adbus::protocol

template <>
struct adbus::protocol::meta<adbus::protocol::header::header> {
  using type = adbus::protocol::header::header;
  static constexpr std::string_view name{ "header" };
  static constexpr auto value{
    pack(&type::endian, &type::type, &type::flags, &type::version, &type::body_length, &type::serial, &type::fields)
  };
};
