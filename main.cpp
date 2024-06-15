#include <regex>
#include <type_traits>

#include <fmt/format.h>
#include <boost/asio.hpp>

#include <adbus/protocol/message_header.hpp>
#include <adbus/protocol/methods.hpp>
#include <adbus/protocol/read.hpp>
#include <adbus/protocol/signature.hpp>
#include <adbus/protocol/write.hpp>

namespace adbus {

namespace asio = boost::asio;

template <typename Executor = asio::any_io_executor>
class basic_dbus_socket {
public:
  using executor_type = Executor;

  template <typename executor_in_t>
    requires std::same_as<std::remove_cvref_t<executor_in_t>, Executor>
  explicit basic_dbus_socket(executor_in_t&& executor) : socket_{ std::forward<executor_in_t>(executor) } {}

  auto async_connect(auto&& endpoint, asio::completion_token_for<void(std::error_code)> auto&& token) {
    // https://dbus.freedesktop.org/doc/dbus-specification.html#auth-nul-byte
    enum struct state_e : std::uint8_t { connect, auth_nul_byte, complete };
    using std::string_literals::operator""s;
    return asio::async_compose<decltype(token), void(std::error_code)>(
        [this, state = state_e::connect, endp = std::forward<decltype(endpoint)>(endpoint), auth_buffer = "\0"s](
            auto& self, std::error_code err = {}, std::size_t size = 0) mutable -> void {
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
              return asio::async_write(socket_, asio::buffer(auth_buffer), std::move(self));
            }
            case state_e::complete: {
              fmt::println("wrote {} bytes: {}", auth_buffer, size);
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

  auto external_authenticate(asio::completion_token_for<void(std::error_code, std::string_view)> auto&& token) {
    return external_authenticate(getuid(), std::forward<decltype(token)>(token));
  }

  /// \note https://dbus.freedesktop.org/doc/dbus-specification.html#auth-protocol
  template <asio::completion_token_for<void(std::error_code, std::string_view)> completion_token_t>
  auto external_authenticate(decltype(getuid()) user_id, completion_token_t&& token)
      -> asio::async_result<std::decay_t<completion_token_t>, void(std::error_code, std::string_view)>::return_type {
    //  31303030 is ASCII decimal "1000" represented in hex, so
    //  the client is authenticating as Unix uid 1000 in this example.
    //
    //  C: AUTH EXTERNAL 31303030
    //  S: OK 1234deadbeef
    //  C: BEGIN
    using std::string_view_literals::operator""sv;

    // The protocol is a line-based protocol, where each line ends with \r\n.
    static constexpr std::string_view line_ending{ "\r\n" };
    static constexpr std::string_view auth_command{ "AUTH" };
    static constexpr std::string_view begin_command{ "BEGIN" };
    static constexpr std::string_view ok_command{ "OK" };
    static constexpr std::string_view auth_mechanism{ "EXTERNAL" };

    enum struct state_e : std::uint8_t { send_auth, recv_ack, send_begin, complete };
    auto const user_id_hex{ ascii_to_hex(std::to_string(user_id)) };
    return asio::async_compose<completion_token_t, void(std::error_code, std::string_view)>(
        [this, state = state_e::send_auth,
         auth = std::make_shared<std::string>(fmt::format("{} {} {}{}", auth_command, auth_mechanism, user_id_hex, line_ending)),
         recv_buffer{ std::make_shared<std::array<char, 1024>>() },
         recv_view{ ""sv },
         begin = std::make_shared<std::string>(fmt::format("{}{}", begin_command, line_ending))
    ](auto& self, std::error_code err = {}, std::size_t size = 0) mutable -> void {
          if (err) {
            return self.complete(err, {});
          }
          switch (state) {
            case state_e::send_auth: {
              state = state_e::recv_ack;
              return asio::async_write(socket_, asio::buffer(*auth), std::move(self));
            }
            case state_e::recv_ack: {
              state = state_e::send_begin;
              return socket_.async_read_some(asio::buffer(*recv_buffer), std::move(self));
            }
            case state_e::send_begin: {
              // The server replies with DATA, OK or REJECTED.
              state = state_e::complete;
              recv_view = { recv_buffer->data(), size };
              if (recv_view.starts_with(ok_command) && recv_view.ends_with(line_ending)) {
                return asio::async_write(socket_, asio::buffer(*begin), std::move(self));
              }
              return self.complete(std::make_error_code(std::errc::bad_message), recv_view);
            }
            case state_e::complete: {
              return self.complete(err, recv_view);
            }
          }
        },
        token, socket_);
  }

  auto say_hello(asio::completion_token_for<void(std::error_code, std::string_view)> auto&& token) {
    auto buffer = std::make_shared<std::string>();
    auto const header{ protocol::methods::hello() };
    auto serialize_error{ protocol::write_dbus_binary(header, *buffer) };
    if (!!serialize_error) {
      fmt::println(stderr, "error: {}\n", serialize_error);
    }
    enum struct state_e : std::uint8_t { send_hello, recv_my_name, complete };
    return asio::async_compose<decltype(token), void(std::error_code, std::string_view)>(
        [this, serialize_error, buffer{ std::move(buffer) }, state{ state_e::send_hello }, recv_buffer{ std::make_shared<std::array<char, 1024>>() }](auto& self, std::error_code err = {}, std::size_t size = 0) mutable -> void {
          if (serialize_error) {
            // Todo make error as std::error_code
            return self.complete(std::make_error_code(std::errc::no_message_available), {});
          }
          if (err) {
            return self.complete(err, {});
          }
          switch (state) {
            case state_e::send_hello: {
              state = state_e::recv_my_name;
//              static constexpr std::string_view foo{ "l\001\000\001\000\000\000\000\001\000\000\000n\000\000\000\001\001o\000\025\000\000\000/org/freedesktop/DBus\000\000\000\006\001s\000\024\000\000\000org.freedesktop.DBus\000\000\000\000\002\001s\000\024\000\000\000org.freedesktop.DBus\000\000\000\000\003\001s\000\005\000\000\000Hello\000\000" };
              return asio::async_write(socket_, asio::buffer(*buffer), std::move(self));
            }
            case state_e::recv_my_name: {
              state = state_e::complete;
              return socket_.async_read_some(asio::buffer(*recv_buffer), std::move(self));
            }
            case state_e::complete: {
              protocol::header::header recv_header{};
              auto deserialize_error{ protocol::read_dbus_binary(recv_header, std::string_view{ recv_buffer->data(), size }) };
              for (std::size_t i{}; i < size; ++i) {
                fmt::println("{},", static_cast<std::uint8_t>(recv_buffer->at(i)));
              }
              return self.complete(err, { recv_buffer->data(), size });
            }
          }
        },
        token, socket_);

  }

 private:
  // todo windows using generic::stream_protocol::socket
  asio::local::stream_protocol::socket socket_;
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

}  // namespace adbus

namespace asio = boost::asio;
using std::string_view_literals::operator""sv;
using std::string_literals::operator""s;
using std::chrono_literals::operator""s;

int main() {
  asio::io_context ctx;

  adbus::basic_dbus_socket socket{ ctx };

  std::string path{ getenv(adbus::env::session.data()) };
  path = std::regex_replace(path, std::regex(std::string{ adbus::detail::unix_path_prefix }), "");

  asio::local::stream_protocol::endpoint ep{ "/tmp/relay.sock" };

  fmt::println("Connecting to {}\n", ep.path());

  asio::steady_timer timer{ ctx, 1s };

  socket.async_connect(ep, [&](auto&& ec) {
     socket.external_authenticate([&](std::error_code err, std::string_view msg) {
       // todo why does this not print in debug mode not running in debugger?
       fmt::println("auth err {}: msg {}\n", err.message(), msg);
       timer.expires_from_now(1s);
         timer.async_wait([&](auto&& ec) {
         if (ec) {
           fmt::println("timer err: {}\n", ec.message());
           return;
         }
         fmt::println("timer expired\n");
         socket.say_hello(
           [](std::error_code ec, std::string_view id) { fmt::println("say_hello err {}: msg {}\n", ec.message(), id); });
       });
     });
  });

  ctx.run();

  fmt::println("done\n");
  return 0;
}
