#include <printf.h>
#include <type_traits>

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
    return socket_.async_connect(std::forward<decltype(endpoint)>(endpoint), std::forward<decltype(token)>(token));
  }

private:
  // todo windows using generic::stream_protocol::socket
  local::stream_protocol::socket socket_;
};

template <typename Executor>
basic_dbus_socket(Executor e) -> basic_dbus_socket<Executor>;

}  // namespace boost::asio::dbus

namespace asio = boost::asio;
using std::string_view_literals::operator""sv;

int main() {
  asio::io_context ctx;

  asio::dbus::basic_dbus_socket socket{ ctx };
  asio::local::stream_protocol::endpoint ep{ "/run/dbus/system_bus_socket"sv };
  socket.async_connect(ep, [](auto&& ec) {
    if (ec) {
      printf("error: %s\n", ec.message().c_str());
      return;
    }
    printf("connected\n");
  });

  ctx.run();

  return 0;
}
