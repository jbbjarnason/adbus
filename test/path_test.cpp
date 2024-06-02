#include <adbus/protocol/path.hpp>

namespace adbus::protocol {

static_assert(path::validate("/foo/bar") == error{});
static_assert(path::validate("/") == error{});
static_assert(path::validate("/a") == error{});
static_assert(path::validate("") == error{ .code = error_code::empty, .index = 0 });
static_assert(path::validate("//") == error{ .code = error_code::trailing_slash, .index = 1 });
static_assert(path::validate("///") == error{ .code = error_code::trailing_slash, .index = 2 });
static_assert(path::validate("/ab/") == error{ .code = error_code::trailing_slash, .index = 3 });
static_assert(path::validate("///a") == error{ .code = error_code::multiple_slashes, .index = 1 });
static_assert(path::validate("/a.b") == error{ .code = error_code::invalid_character, .index = 2 });
static_assert(path::validate("a/b") == error{ .code = error_code::path_not_absolute, .index = 0 });

}

int main() {
  return 0;
}
