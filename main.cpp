#include <mutex>
#include <regex>
#include <type_traits>

#include <fmt/format.h>
#include <boost/asio.hpp>
#include <glaze/util/expected.hpp>

#include <adbus/protocol/message_header.hpp>
#include <adbus/protocol/methods.hpp>
#include <adbus/protocol/read.hpp>
#include <adbus/protocol/signature.hpp>
#include <adbus/protocol/write.hpp>

// todo properly use this dependency by namespacing it or use package manager
#define BOOST_SAM_HEADER_ONLY  // todo make this optional
#include <adbus/ext/boost/sam/condition_variable.hpp>
namespace adbus {
#ifdef BOOST_SAM_STANDALONE
using condition_variable = sam::basic_condition_variable<asio::any_io_executor>;
#else
using condition_variable = boost::sam::basic_condition_variable<boost::asio::any_io_executor>;
#endif
}  // namespace adbus

namespace adbus {

namespace api {
struct request_name_params {
  std::string_view name{};
  struct flags_t {
    bool allow_replacement : 1 {};
    bool replace_existing : 1 {};
    bool do_not_queue : 1 {};
    std::uint32_t reserved : 29 {};
  };
  std::uint32_t flags{};
  struct glaze {
    static constexpr auto value{
      glz::object("name",
                  &request_name_params::name,
                  "flags",
                  [](auto&& self) -> auto& { return *reinterpret_cast<const std::uint32_t*>(&self.flags); }),
    };
  };
};
enum struct request_name_reply : std::uint32_t {
  unknown = 0,
  primary_owner = 1,
  in_queue = 2,
  exists = 3,
  already_owner = 4
};
}  // namespace api

namespace asio = boost::asio;

namespace detail {

class incoming_message_queue {
public:
  incoming_message_queue() = default;
  [[nodiscard]] auto on_message(is_header auto&& header, std::string&& message) -> std::error_code {
    std::scoped_lock lock{ mutex_ };
    if (message_queue_.empty()) {
      // is this an error? don't think so
      return {};
    }
    switch (header.type) {
      using enum protocol::header::message_type_e;
      case method_return: {
        auto const reply_serial{ header.reply_serial() };
        auto it = std::ranges::find_if(message_queue_,
                                       [reply_serial](auto&& event) { return event->wait_header.serial == reply_serial; });
        if (it == message_queue_.end()) {
          fmt::println(stderr, "error: serial {} not found", reply_serial.value_or(0));
          // todo custom error_code
          return std::make_error_code(std::errc::bad_message);
        }
        (*it)->message = std::move(message);
        (*it)->recv_header = std::forward<decltype(header)>(header);
        (*it)->cv.notify_all();
        message_queue_.erase(it);
        break;
      }
      case error: {
        // todo implement
        break;
      }
      case signal: {
        // todo implement
        break;
      }
      case method_call: {
        // todo implement
        break;
      }
      case invalid: {
        // todo implement
        break;
      }
      default: {
        break;
      }
    }
    return {};
  }

  struct event {
    protocol::header::header wait_header{};
    condition_variable cv;
    protocol::header::header recv_header{};
    std::string message{};
    std::error_code err{};
  };

  auto async_wait(is_header auto&& header,
                  asio::completion_token_for<
                      void(std::error_code, std::size_t, protocol::header::header const&, std::string_view)> auto&& token) {
    auto exe{ asio::get_associated_executor(token) };
    auto new_event{ std::make_shared<event>(header, condition_variable{ exe }) };
    std::scoped_lock lock{ mutex_ };
    message_queue_.emplace_back(new_event);
    return asio::async_compose<decltype(token),
                               void(std::error_code, std::size_t, protocol::header::header const&, std::string_view)>(
        [first_call = true, new_event](auto& self, std::error_code err = {}) mutable {
          if (first_call) {
            first_call = false;
            new_event->cv.async_wait(std::move(self));
            return;
          }
          if (err) {
            return self.complete(err, {}, {}, {});
          }
          self.complete(new_event->err, new_event->message.size(), new_event->recv_header, new_event->message);
        },
        token, exe);
  }

private:
  std::vector<std::shared_ptr<event>> message_queue_;
  std::mutex mutex_;
};

}  // namespace detail

template <typename Executor = asio::any_io_executor>
class basic_dbus_socket {
public:
  using executor_type = Executor;

  template <typename executor_in_t>
    requires std::same_as<std::remove_cvref_t<executor_in_t>, Executor>
  explicit basic_dbus_socket(executor_in_t&& executor) : socket_{ std::forward<executor_in_t>(executor) } {}

  std::uint32_t new_serial() {
    std::scoped_lock lock{ serial_mutex_ };
    return ++serial_;
  }

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
         auth = std::make_shared<std::string>(
             fmt::format("{} {} {}{}", auth_command, auth_mechanism, user_id_hex, line_ending)),
         recv_buffer{ std::make_shared<std::array<char, 1024>>() }, recv_view{ ""sv },
         begin = std::make_shared<std::string>(fmt::format("{}{}", begin_command, line_ending))](
            auto& self, std::error_code err = {}, std::size_t size = 0) mutable -> void {
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

  /// \brief async read from socket returns when a error occurs, otherwise infinite loop
  auto async_read_loop(asio::completion_token_for<void(std::error_code)> auto&& token) {
    enum struct state_e : std::uint8_t { recv_fixed_header, recv_header_fields, recv_payload, complete };
    return asio::async_compose<decltype(token), void(std::error_code)>(
        [this, state{ state_e::recv_fixed_header }, buffer_header{ std::make_shared<std::string>() },
         buffer_header_fields{ std::make_shared<std::string>() }, buffer_payload{ std::make_shared<std::string>() },
         recv_header{ protocol::header::header{} }](auto& self, std::error_code err = {},
                                                    std::size_t size = 0) mutable -> void {
          if (err) {
            return self.complete(err);
          }
          switch (state) {
            case state_e::recv_fixed_header: {
              state = state_e::recv_header_fields;
              buffer_header = std::make_shared<std::string>();
              buffer_header->resize(sizeof(protocol::header::fixed_header));
              return socket_.async_read_some(asio::buffer(*buffer_header), std::move(self));
            }
            case state_e::recv_header_fields: {
              state = state_e::recv_payload;
              protocol::header::fixed_header fixed_header{};
              auto deserialize_error{ protocol::read_dbus_binary(fixed_header, *buffer_header) };
              if (!!deserialize_error) {
                fmt::println(stderr, "error: {}\n", deserialize_error);
                // todo std::error_code convertible
                return self.complete(std::make_error_code(std::errc::bad_message));
              }
              // + padding length
              const std::size_t total_header_length{ sizeof(protocol::header::fixed_header) +
                                                     fixed_header.fields_array_len };
              constexpr auto alignment{ 8 };  // 8 bit alignment after header
              const auto padding = (alignment - (total_header_length % alignment)) % alignment;
              buffer_header_fields = std::make_shared<std::string>();
              buffer_header_fields->resize(fixed_header.fields_array_len + padding);
              return socket_.async_read_some(asio::buffer(*buffer_header_fields), std::move(self));
            }
            case state_e::recv_payload: {
              state = state_e::complete;
              // not the most optimized but shouldn't be too big copy
              buffer_header->append(*buffer_header_fields);
              auto deserialize_error{ protocol::read_dbus_binary(recv_header, *buffer_header) };
              if (!!deserialize_error) {
                fmt::println(stderr, "error: {}\n", deserialize_error);
                // todo std::error_code convertible
                return self.complete(std::make_error_code(std::errc::bad_message));
              }
              buffer_payload = std::make_shared<std::string>();
              buffer_payload->resize(recv_header.body_length);
              return socket_.async_read_some(asio::buffer(*buffer_payload), std::move(self));
            }
            case state_e::complete: {
              state = state_e::recv_fixed_header;  // loop
              if (auto message_queue_err{ incoming_message_queue_.on_message(protocol::header::header{ recv_header },
                                                                             std::move(*buffer_payload)) }) {
                return self.complete(message_queue_err);
              }
              return asio::post(std::move(self));
            }
          }
        },
        token, socket_);
  }

  template <typename return_type>
  auto call_method(is_header auto&& header,
                   auto&& params,
                   asio::completion_token_for<void(glz::expected<return_type, std::error_code>)> auto&& token) {
    using return_t = glz::expected<return_type, std::error_code>;
    auto write_buffer = std::make_shared<std::vector<std::uint8_t>>();
    adbus::protocol::error serialize_error{};
    header.serial = new_serial();
    if constexpr (std::same_as<glz::skip, std::decay_t<decltype(params)>>) {
      serialize_error = protocol::write_dbus_binary(header, *write_buffer);
    } else {
      serialize_error = protocol::write_dbus_binary(params, *write_buffer);
      if (!serialize_error) {
        std::vector<std::uint8_t> header_buffer{};
        header.body_length = write_buffer->size();
        serialize_error = protocol::write_dbus_binary(header, header_buffer);
        if (!serialize_error) {
          // todo benchmark insert vs push_back and rotate or even use std::deque
          write_buffer->insert(write_buffer->begin(), header_buffer.begin(), header_buffer.end());
        }
      }
    }
    enum struct state_e : std::uint8_t { send_write, wait_reply, complete };
    return asio::async_compose<decltype(token), void(return_t)>(
        [this, serialize_error, write_buffer{ std::move(write_buffer) }, state{ state_e::send_write },
         header_mv{ std::move(header) }](auto& self, std::error_code err = {}, std::size_t size = 0,
                                         protocol::header::header const& recv_header = {},
                                         std::string_view reply = {}) mutable -> void {
          if (serialize_error) {
            fmt::println(stderr, "error: {}\n", serialize_error);
            // Todo make error as std::error_code
            return self.complete(glz::unexpected<std::error_code>(std::make_error_code(std::errc::no_message_available)));
          }
          if (err) {
            return self.complete(glz::unexpected<std::error_code>(err));
          }
          switch (state) {
            case state_e::send_write: {
              state = state_e::wait_reply;
              return asio::async_write(socket_, asio::buffer(*write_buffer), std::move(self));
            }
            case state_e::wait_reply: {
              state = state_e::complete;
              // todo now we have written to the socket, I hope that we don't receive the reply before the below is done
              // can we somehow mutex it or are we confident it won't occur?
              return incoming_message_queue_.async_wait(std::move(header_mv), std::move(self));
            }
            case state_e::complete: {
              if (recv_header.signature() != protocol::type::signature_v<return_type>) {
                fmt::println(stderr, "error: expected signature {} got {}\n", protocol::type::signature_v<return_type>,
                             recv_header.signature().value_or("unknown"));
                // todo std::error_code convertible
                return self.complete(glz::unexpected<std::error_code>(std::make_error_code(std::errc::bad_message)));
              }
              return_type return_value{};
              auto parse_error{ protocol::read_dbus_binary(return_value, reply) };
              if (!!parse_error) {
                fmt::println(stderr, "error: {}\n", parse_error);
                // todo std::error_code convertible
                return self.complete(glz::unexpected<std::error_code>(std::make_error_code(std::errc::bad_message)));
              }
              return self.complete(std::move(return_value));
            }
          }
        },
        token, socket_);
  }

  template <typename return_type>
  auto call_method(is_header auto&& header,
                   asio::completion_token_for<void(glz::expected<return_type, std::error_code>)> auto&& token) {
    return call_method<return_type>(std::forward<decltype(header)>(header), glz::skip{},
                                    std::forward<decltype(token)>(token));
  }

  auto say_hello(asio::completion_token_for<void(glz::expected<std::string_view, std::error_code>)> auto&& token) {
    return call_method<std::string_view>(protocol::methods::hello(), std::forward<decltype(token)>(token));
  }

  auto request_name(api::request_name_params const& params,
                    asio::completion_token_for<void(glz::expected<api::request_name_reply, std::error_code>)> auto&& token) {
    return call_method<api::request_name_reply>(protocol::methods::request_name(), params,
                                                std::forward<decltype(token)>(token));
  }

  template <typename method_param_t, typename method_result_t = void>
  auto register_method(is_header auto&& header,
                       std::invocable<method_result_t(glz::expected<method_param_t, std::error_code>)> auto&& token) {
    enum struct state_e : std::uint8_t { wait_call, call };
    auto exe{ asio::get_associated_executor(token) };
    return asio::async_compose<decltype(token), method_result_t(glz::expected<method_param_t, std::error_code>)>(
        [this, state{ state_e::wait_call }, header_mv{ std::forward<decltype(header)>(header) }](
            auto& self, std::error_code err = {}, std::size_t size = 0, protocol::header::header const& recv_header = {},
            std::string_view reply = {}) mutable {
          if (err) {
            return self.complete(glz::unexpected<std::error_code>(err));
          }
          switch (state) {
            case state_e::wait_call: {
              state = state_e::call;
              return incoming_message_queue_.async_wait(std::move(header_mv), std::move(self));
            }
            case state_e::call: {
              if (recv_header.signature() != protocol::type::signature_v<method_param_t>) {
                fmt::println(stderr, "error: expected signature {} got {}\n", protocol::type::signature_v<method_param_t>,
                             recv_header.signature().value_or("unknown"));
                // todo std::error_code convertible
                // todo we should async_write error to header
                return self.complete(glz::unexpected<std::error_code>(std::make_error_code(std::errc::bad_message)));
              }
              method_param_t param{};
              auto parse_error{ protocol::read_dbus_binary(param, reply) };
              if (!!parse_error) {
                fmt::println(stderr, "error: {}\n", parse_error);
                // todo std::error_code convertible
                return self.complete(glz::unexpected<std::error_code>(std::make_error_code(std::errc::bad_message)));
              }
              return self.complete(param);
            }
          }
        }, [executor{ std::move(exe) }, invocable{ token }](glz::expected<method_param_t, std::error_code> const& params) mutable {
          if constexpr (has_co_await<method_result_t>) {
            asio::co_spawn(executor, [invocable_mv{ std::move(invocable) }, params_cp{ params }]() -> asio::awaitable<void> {
              auto result = co_await invocable_mv(params_cp);
              // todo async write result
            }, asio::detached);
          }
          else {
            auto result = invocable(params);
            // todo async write result
          }

        }, socket_);
  }

private:
  // todo windows using generic::stream_protocol::socket
  std::uint32_t serial_{};
  std::mutex serial_mutex_{};
  asio::local::stream_protocol::socket socket_;
  detail::incoming_message_queue incoming_message_queue_{};
};

template <typename Executor>
basic_dbus_socket(Executor e) -> basic_dbus_socket<Executor>;

// class dbus_interface {
// public:
//   dbus_interface() = default;
//   dbus_interface(basic_dbus_socket& socket) : socket_{ &socket } {}
//
// };

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

  // example $ socat -v UNIX-LISTEN:/tmp/relay2.sock,fork UNIX-CONNECT:/run/user/1000/bus
  std::string path{ "/tmp/relay2.sock" };  //{ getenv(adbus::env::session.data()) };
  path = std::regex_replace(path, std::regex(std::string{ adbus::detail::unix_path_prefix }), "");

  asio::local::stream_protocol::endpoint ep{ path };

  fmt::println("Connecting to {}\n", ep.path());

  socket.async_connect(ep, [&](auto&& ec) {
    socket.external_authenticate([&](std::error_code err, std::string_view msg) {
      // todo why does this not print in debug mode not running in debugger?
      fmt::println("auth err {}: msg {}\n", err.message(), msg);
      if (err) {
        return;
      }
      socket.async_read_loop([&](auto&& ec) { fmt::println("read_loop err: {}\n", ec.message()); });
      socket.say_hello([&](glz::expected<std::string_view, std::error_code> id) {
        if (id) {
          fmt::println("say_hello: {}\n", *id);
          socket.request_name({ .name = "com.example.HelloWorld" },
                              [&](glz::expected<adbus::api::request_name_reply, std::error_code> reply) {
                                if (reply) {
                                  fmt::println("request_name: {}\n", std::to_underlying(*reply));
                                } else {
                                  fmt::println("request_name error: {}\n", reply.error().message());
                                }
                              });
        } else {
          fmt::println("say_hello error: {}\n", id.error().message());
        }
      });
    });
  });

  ctx.run();

  fmt::println("done\n");
  return 0;
}
