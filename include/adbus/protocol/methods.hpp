
#include <adbus/protocol/message_header.hpp>

namespace adbus::protocol::methods {

auto hello() -> protocol::header::header {
  using namespace adbus::protocol::header;
  return {
    .type = message_type_e::method_call,
    .body_length = 0,
    .serial = 1,
    .fields = {
      {
        .value = field_destination{"org.freedesktop.DBus"}
      },
      {
        .value = field_path{"/org/freedesktop/DBus"}
      },
      {
        .value = field_interface{"org.freedesktop.DBus"}
      },
      {
        .value = field_member{"Hello"}
      },
      {
        .value = field_signature{}
      }
    }
  };
}

auto request_name(std::string_view name) -> protocol::header::header {
  using namespace adbus::protocol::header;
  return {
        .type = message_type_e::method_call,
        .body_length = 0,
        .serial = 1,
        .fields = {
          {
                .value = field_destination{"org.freedesktop.DBus"}
          },
          {
                .value = field_path{"/org/freedesktop/DBus"}
          },
          {
                .value = field_interface{"org.freedesktop.DBus"}
          },
          {
                .value = field_member{"RequestName"}
          },
          {
                .value = field_signature{"s"}
          }
        },
  };
}

}

