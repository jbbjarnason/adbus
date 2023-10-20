#include <regex>
#include <type_traits>

#include <fmt/format.h>
#include <boost/asio.hpp>

namespace boost::asio::dbus {

template <typename Executor = any_io_executor>
class basic_dbus_socket {
public:
  using executor_type = Executor;

  template <typename executor_in_t>
    requires std::same_as<std::remove_cvref_t<executor_in_t>, Executor>
  explicit basic_dbus_socket(executor_in_t&& executor) : socket_{ std::forward<executor_in_t>(executor) } {}

  auto async_connect(auto&& endpoint, completion_token_for<void(std::error_code)> auto&& token) {
    // https://dbus.freedesktop.org/doc/dbus-specification.html#auth-nul-byte
    enum struct state_e : std::uint8_t { connect, auth_nul_byte, complete };
    using std::string_literals::operator""s;
    return asio::async_compose<decltype(token), void(std::error_code)>(
        [this, state = state_e::connect, endp = std::forward<decltype(endpoint)>(endpoint), auth_buffer = "\0"s](
            auto& self, std::error_code err = {}, std::size_t = 0) mutable -> void {
          if (err) {
            return self.complete(err);
          }
          switch (state) {
            case state_e::connect: {
              state = state_e::auth_nul_byte;
              return socket_.async_connect(endp, std::move(self));
            }
            case state_e::auth_nul_byte: {
              state = state_e::complete;
              return socket_.async_write_some(asio::buffer(auth_buffer), std::move(self));
            }
            case state_e::complete: {
              return self.complete(err);
            }
          }
        },
        token, socket_);
  }

  auto ascii_to_hex(std::string_view str) -> std::string {
    // todo compile time
    std::string hex;
    hex.reserve(str.size() * 2);
    for (auto&& c : str) {
      hex += fmt::format("{:02x}", c);
    }
    return hex;
  }

  auto external_authenticate(completion_token_for<void(std::error_code, std::string_view)> auto&& token) {
    return external_authenticate(getuid(), std::forward<decltype(token)>(token));
  }

  /// \note https://dbus.freedesktop.org/doc/dbus-specification.html#auth-protocol
  template <completion_token_for<void(std::error_code, std::string_view)> completion_token_t>
  auto external_authenticate(decltype(getuid()) user_id, completion_token_t&& token)
      -> asio::async_result<std::decay_t<completion_token_t>, void(std::error_code, std::string_view)>::return_type {
    //  31303030 is ASCII decimal "1000" represented in hex, so
    //  the client is authenticating as Unix uid 1000 in this example.
    //
    //  C: AUTH EXTERNAL 31303030
    //  S: OK 1234deadbeef
    //  C: BEGIN

    // The protocol is a line-based protocol, where each line ends with \r\n.
    static constexpr std::string_view line_ending{ "\r\n" };
    static constexpr std::string_view auth_command{ "AUTH" };
    static constexpr std::string_view auth_mechanism{ "EXTERNAL" };

    enum struct state_e : std::uint8_t { send_auth, recv_ack, complete };
    auto const user_id_hex{ ascii_to_hex(std::to_string(user_id)) };
    return asio::async_compose<completion_token_t, void(std::error_code, std::string_view)>(
        [this, state = state_e::send_auth,
         auth{ fmt::format("{} {} {}{}", auth_command, auth_mechanism, user_id_hex, line_ending) },
         recv_buffer{ std::make_shared<std::array<char, 1024>>() }](auto& self, std::error_code err = {}, std::size_t size = 0) mutable -> void {
          if (err) {
            return self.complete(err, {});
          }
          switch (state) {
            case state_e::send_auth: {
              state = state_e::recv_ack;
              return socket_.async_write_some(asio::buffer(auth), std::move(self));
            }
            case state_e::recv_ack: {
              state = state_e::complete;
              return socket_.async_read_some(asio::buffer(*recv_buffer), std::move(self));
            }
            case state_e::complete: {
              return self.complete(err, std::string_view{ recv_buffer->data(), size });
            }
          }
        },
        token, socket_);
  }

private:
  // todo windows using generic::stream_protocol::socket
  local::stream_protocol::socket socket_;
};

template <typename Executor>
basic_dbus_socket(Executor e) -> basic_dbus_socket<Executor>;

namespace env {
static constexpr std::string_view session{ "DBUS_SESSION_BUS_ADDRESS" };
static constexpr std::string_view system{ "DBUS_SYSTEM_BUS_ADDRESS" };
}  // namespace env

namespace detail {
static constexpr std::string_view unix_path_prefix{ "unix:path=" };
}

}  // namespace boost::asio::dbus

namespace asio = boost::asio;
using std::string_view_literals::operator""sv;

int main() {
  asio::io_context ctx;

  asio::dbus::basic_dbus_socket socket{ ctx };

  std::string path{ getenv(asio::dbus::env::session.data()) };
  path = std::regex_replace(path, std::regex(std::string{ asio::dbus::detail::unix_path_prefix }), "");

  asio::local::stream_protocol::endpoint ep{ path };

  fmt::print("Connecting to {}\n", ep.path());

  socket.async_connect(ep, [&socket](auto&& ec) {
    if (ec) {
      fmt::print("error: {}\n", ec.message());
      return;
    }
    fmt::print("connected\n");
    socket.external_authenticate([](std::error_code err, std::string_view msg) {
      fmt::print("auth err {}: msg {}\n", err.message(), msg);
    });
  });

  ctx.run();

  return 0;
}
