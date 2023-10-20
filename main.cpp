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

  auto async_connect(auto&& endpoint, auto&& token) {
    // https://dbus.freedesktop.org/doc/dbus-specification.html#auth-nul-byte
    enum struct state_e : std::uint8_t { connect, auth_nul_byte, rcv_ack, complete };
    using std::string_literals::operator""s;
    return asio::async_compose<decltype(token), void(std::error_code)>(
        [this, state = state_e::connect, endp = std::forward<decltype(endpoint)>(endpoint), auth_buffer = "\0"s,
         recv_buffer = std::array<char, 1024>{}](auto& self, std::error_code err = {}, std::size_t = 0) mutable -> void {
          if (err) {
            return self.complete(err);
          }
          switch (state) {
            case state_e::connect: {
              state = state_e::auth_nul_byte;
              socket_.async_connect(endp, std::move(self));
              return;
            }
            case state_e::auth_nul_byte: {
              state = state_e::rcv_ack;
              socket_.async_write_some(asio::buffer(auth_buffer), std::move(self));
              return;
            }
            case state_e::rcv_ack: {
              state = state_e::complete;
              return self.complete(err);
//              socket_.async_read_some(asio::buffer(recv_buffer), std::move(self));
              return;
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

  /// \brief write and take ownership of the buffer
  auto async_write_some(auto&& buffer, auto&& token)
      -> asio::async_result<std::decay_t<decltype(token)>, void(std::error_code, std::size_t)>::return_type {
    return asio::async_compose<decltype(token), void(std::error_code, std::size_t)>(
        [this, first_call = true,
         buffer_ptr = std::make_shared<std::remove_cvref_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer))](
            auto& self, std::error_code err = {}, std::size_t size = 0) mutable {
          if (err) {
            self.complete(err, size);
            return;
          }
          if (first_call) {
            first_call = false;
            socket_.async_write_some(asio::buffer(*buffer_ptr), std::move(self));
            return;
          }
          self.complete(err, size);
        },
        token);
  }

  auto async_read_some(auto&& buffer, auto&& token) {
    return asio::async_compose<decltype(token),
                               void(std::error_code, std::shared_ptr<std::remove_cvref_t<decltype(buffer)>>)>(
        [this, first_call = true,
         buffer_ptr = std::make_shared<std::remove_cvref_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer))](
            auto& self, std::error_code err = {}, std::size_t size = 0) mutable {
          if (err) {
            self.complete(err, nullptr);
            return;
          }
          if (first_call) {
            first_call = false;
            socket_.async_read_some(asio::buffer(*buffer_ptr), std::move(self));
            return;
          }
          self.complete(err, std::move(buffer_ptr));
        },
        token);
  }

  auto async_receive(auto&& buffer, auto&& token) {
    return asio::async_compose<decltype(token),
                               void(std::error_code, std::shared_ptr<std::remove_cvref_t<decltype(buffer)>>)>(
        [this, first_call = true,
         buffer_ptr = std::make_shared<std::remove_cvref_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer))](
            auto& self, std::error_code err = {}, std::size_t size = 0) mutable {
          if (err) {
            self.complete(err, nullptr);
            return;
          }
          if (first_call) {
            first_call = false;
            buffer_ptr->resize(1024);
            socket_.async_receive(asio::buffer(*buffer_ptr), std::move(self));
            return;
          }
          self.complete(err, std::move(buffer_ptr));
        },
        token);
  }

  auto external_authenticate(auto&& token) { return external_authenticate(getuid(), std::forward<decltype(token)>(token)); }

  /// \note https://dbus.freedesktop.org/doc/dbus-specification.html#auth-protocol
  template <typename completion_token_t>
  auto external_authenticate(decltype(getuid()) user_id, completion_token_t&& token)
      -> asio::async_result<std::decay_t<completion_token_t>, void(std::error_code, std::size_t)>::return_type {
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

    auto hex = ascii_to_hex(std::to_string(user_id));

    auto auth = fmt::format("{} {} {}{}", auth_command, auth_mechanism, hex, line_ending);

    return async_write_some(std::move(auth), std::forward<decltype(token)>(token));
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
    socket.external_authenticate([&socket](std::error_code err, std::size_t size) {
      fmt::print("auth err {} size {}\n", err.message(), size);


      socket.async_receive(std::string{}, [](std::error_code err, std::shared_ptr<std::string> buffer) {
        fmt::print("read buffer{} err {}\n", buffer->size(), err.message());
      });
    });
  });

  ctx.run();

  return 0;
}
