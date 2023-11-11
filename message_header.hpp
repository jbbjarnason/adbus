#pragma once

#include <bit>

namespace adbus::message {

namespace details {
static constexpr auto get_endian() -> std::endian {
  if constexpr (std::endian::native == std::endian::little) {
    return std::endian::little;
  } else if constexpr (std::endian::native == std::endian::big) {
    return std::endian::big;
  } else {
    []<bool flag = false> {
      static_assert(flag, "Mixed endian unsupported");
    };
  }
}
enum struct message_type_e : std::uint8_t {
  invalid = 0,    // This is an invalid type.
  method_call,    // Method call. This message type may prompt a reply.
  method_return,  // Method reply with returned data.
  error,          // Error reply. If the first argument exists and is a string, it is an error message.
  signal,         // Signal emission.
};

struct flags {
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
static_assert(sizeof(flags) == 1);
static_assert(std::is_standard_layout_v<flags>);
static_assert(static_cast<std::byte>(flags{ .no_reply_expected = true }) == std::byte{ 0x1 });
static_assert(static_cast<std::byte>(flags{ .no_auto_start = true }) == std::byte{ 0x2 });
static_assert(static_cast<std::byte>(flags{ .allow_interactive_authorization = true }) == std::byte{ 0x4 });

}  // namespace details

struct header {
  // Endianness flag; ASCII 'l' for little-endian or ASCII 'B' for big-endian. Both header and body are in this endianness.
  static constexpr auto endian{ details::get_endian() };
  // Message type. Unknown types must be ignored.
  details::message_type_e type{ details::message_type_e::invalid };
  // Bitwise OR of flags.
  details::flags flags{};
  // Major protocol version of the sending application. If the major protocol version of the receiving application does not
  // match, the applications will not be able to communicate and the D-Bus connection must be disconnected. The major
  // protocol version for this version of the specification is 1.
  static constexpr auto version{ std::uint8_t{ 1 } };
  // Length in bytes of the message body, starting from the end of the header. The header ends after its alignment padding to
  // an 8-boundary.
  std::uint32_t body_length{ 0 };
  // The serial of this message, used as a cookie by the sender to identify the reply corresponding to this request. This
  // must not be zero.
  std::uint32_t serial{ 0 };
  // An array of zero or more header fields where the byte is the field code, and the variant is the field value. The message
  // type determines which fields are required.
//  std::vector<std::byte
};

}  // namespace adbus::message
