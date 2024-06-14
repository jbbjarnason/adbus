#include <array>
#include <tuple>

#include <fmt/format.h>

#include <boost/ut.hpp>
#include <forward_list>
#include <glaze/glaze.hpp>
#include <set>
#include <unordered_set>

#include <adbus/protocol/read.hpp>
#include <adbus/protocol/signature.hpp>

#include "common.hpp"

using namespace boost::ut;
using std::string_view_literals::operator""sv;
using std::string_literals::operator""s;

namespace std {
template <typename... Args>
constexpr auto format_as(variant<Args...> const& var) noexcept -> std::string {
  return std::visit([](auto&& v) { return fmt::format("{}", v); }, var);
}
template <typename first_t, typename second_t>
constexpr auto format_as(pair<const first_t, second_t> const& v) noexcept -> std::string {
  return fmt::format("({}, {})", v.first, v.second);
}
template <adbus::container T>
constexpr auto format_as(T&& v) noexcept -> std::string {
  std::string str;
  str += '[';
  for (auto&& i : v) {
    str += fmt::format("{}, ", i);
  }
  str += ']';
  return str;
}
}  // namespace std

template <typename T>
struct generic_test {
  T expected{};
  std::vector<std::uint8_t> buffer{};
};

constexpr auto generic_test_case = [](auto&& test) {
  using value_t = std::decay_t<decltype(test.expected)>;
  value_t value{};
  auto err = adbus::protocol::read_dbus_binary(value, test.buffer);
  expect(!err) << fmt::format("error: {}", err);
  expect(value == test.expected) << fmt::format("expected: {}, got: {}", test.expected, value);
};

int main() {
  using adbus::protocol::read_dbus_binary;

  "number types"_test = generic_test_case | std::tuple{
    generic_test<std::uint8_t>{ .expected = 0x12, .buffer = { 0x12 } },
    generic_test<std::uint16_t>{ .expected = 0x1234, .buffer = { 0x34, 0x12 } },
    generic_test<std::uint32_t>{ .expected = 0x12345678, .buffer = { 0x78, 0x56, 0x34, 0x12 } },
    generic_test<std::uint64_t>{ .expected = 0x123456789abcdef0,
                                 .buffer = { 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12 } },
    generic_test<std::int16_t>{ .expected = -0x1234, .buffer = { 0xcc, 0xed } },
    generic_test<std::int32_t>{ .expected = -0x12345678, .buffer = { 0x88, 0xa9, 0xcb, 0xed } },
    generic_test<std::int64_t>{ .expected = -0x123456789abcdef0,
                                .buffer = { 0x10, 0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0xed } },
    generic_test<double>{ .expected = 1337.42, .buffer = { 0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40 } },
    generic_test<double>{ .expected = -1337.42, .buffer = { 0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0xc0 } },
  };

  "enum as number"_test = generic_test_case | std::tuple{
    generic_test<enum_as_number>{ .expected = enum_as_number::a, .buffer = { 0x01 } },
    generic_test<enum_as_number>{ .expected = enum_as_number::b, .buffer = { 0x02 } },
    generic_test<enum_as_number>{ .expected = enum_as_number::c, .buffer = { 0x03 } },
  };

  "bool"_test = generic_test_case | std::tuple{
    generic_test<bool>{ .expected = true, .buffer = { 0x01, 0x00, 0x00, 0x00 } },
    generic_test<bool>{ .expected = false, .buffer = { 0x00, 0x00, 0x00, 0x00 } },
  };

  "string"_test = generic_test_case | std::tuple{
    generic_test<std::string>{ .expected = "this is a message",
                               .buffer = { 17,  0,   0,   0,   't', 'h', 'i', 's', ' ', 'i', 's',
                                           ' ', 'a', ' ', 'm', 'e', 's', 's', 'a', 'g', 'e', '\0' } },
  };

  "string_view"_test = generic_test_case | std::tuple{
    generic_test<std::string_view>{ .expected = "this is a message",
                                    .buffer = { 17,  0,   0,   0,   't', 'h', 'i', 's', ' ', 'i', 's',
                                                ' ', 'a', ' ', 'm', 'e', 's', 's', 'a', 'g', 'e', '\0' } },
    generic_test<std::string_view>{ .expected = "það", .buffer = { 5, 0, 0, 0, 195, 190, 'a', 195, 176, '\0' } },
  };

  "enum as string"_test = generic_test_case | std::tuple{
    generic_test<enum_as_string>{
        .expected = enum_as_string::a,
        .buffer = { 1, 0, 0, 0, 'a', '\0' },
    },
    generic_test<enum_as_string>{
        .expected = enum_as_string::b,
        .buffer = { 1, 0, 0, 0, 'b', '\0' },
    },
    generic_test<enum_as_string>{
        .expected = enum_as_string::c,
        .buffer = { 1, 0, 0, 0, 'c', '\0' },
    },
  };

  using adbus::protocol::type::signature;
  "signature"_test = generic_test_case | std::tuple{
    generic_test<signature>{ .expected = signature{ adbus::protocol::type::signature_v<foo> },
                             .buffer = { signature{ adbus::protocol::type::signature_v<foo> }.size(), '(', 't', 'a', '(',
                                         's', 't', ')', 'a', '(', 's', 't', ')', 's', ')', '\0' } }
  };

  "vector trivial value_type"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = std::vector{ 10UL, 20UL, 30UL },
        .buffer = {
            24, 0, 0, 0,              // size
            0,  0, 0, 0,              // padding
            10, 0, 0, 0, 0, 0, 0, 0,  // 10
            20, 0, 0, 0, 0, 0, 0, 0,  // 20
            30, 0, 0, 0, 0, 0, 0, 0,  // 30
        },
    },
  };

  "empty vector"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = std::vector<std::uint64_t>{},
        .buffer = {
          0, 0, 0, 0,  // size
          0, 0, 0, 0,  // padding
        },
    },
  };

  "vector of strings"_test = generic_test_case | std::tuple{
    generic_test{
      .expected = std::vector{ "bar"s, "baz"s, "foo"s },
      .buffer = {
        24, 0, 0, 0,  // size
        3,  0, 0, 0,  'b', 'a', 'r', '\0',
        3,  0, 0, 0,  'b', 'a', 'z', '\0',
        3,  0, 0, 0,  'f', 'o', 'o', '\0',
      },
    },
    generic_test{
      .expected = std::array{ "bar"s, "baz"s, "foo"s },
      .buffer = {
        24, 0, 0, 0,  // size
        3,  0, 0, 0,  'b', 'a', 'r', '\0',
        3,  0, 0, 0,  'b', 'a', 'z', '\0',
        3,  0, 0, 0,  'f', 'o', 'o', '\0',
      },
    },
    generic_test{
      .expected = std::list{ "bar"s, "baz"s, "foo"s },
      .buffer = {
        24, 0, 0, 0,  // size
        3,  0, 0, 0,  'b', 'a', 'r', '\0',
        3,  0, 0, 0,  'b', 'a', 'z', '\0',
        3,  0, 0, 0,  'f', 'o', 'o', '\0',
      },
    },
    generic_test{
      .expected = std::set{ "bar"s, "baz"s, "foo"s },
      .buffer = {
        24, 0, 0, 0,  // size
        3,  0, 0, 0,  'b', 'a', 'r', '\0',
        3,  0, 0, 0,  'b', 'a', 'z', '\0',
        3,  0, 0, 0,  'f', 'o', 'o', '\0',
      },
    },
    generic_test{
      .expected = std::unordered_set{"foo"s, "baz"s, "bar"s, },
      .buffer = {
        24, 0, 0, 0,  // size
        3,  0, 0, 0,  'b', 'a', 'r', '\0',
        3,  0, 0, 0,  'b', 'a', 'z', '\0',
        3,  0, 0, 0,  'f', 'o', 'o', '\0',
      },
    },
  };
  "vector of strings with padding"_test = generic_test_case | std::tuple{
    generic_test{
      .expected = std::vector{ "hello"s, "dbus"s, "world"s },
      .buffer = {
        34,  0,   0,   0,                  // size
        5,   0,   0,   0,                  // length 1
        'h', 'e', 'l', 'l', 'o', 0, 0, 0,  // string 1
        4,   0,   0,   0,                  // length 2
        'd', 'b', 'u', 's', 0,   0, 0, 0,  // string 2
        5,   0,   0,   0,                  // length 3
        'w', 'o', 'r', 'l', 'd', 0,        // string 3
      },
    },
  };
  "empty vector of vectors"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = std::vector<std::vector<std::uint64_t>>{},
        .buffer = {
            0, 0, 0, 0,  // size
        },
    },
  };
  "Non-empty vector of vectors with padding"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = std::vector{ std::vector{ 1UL, 2UL }, std::vector{ 3UL, 4UL, 5UL } },
        .buffer = {
            0x34, 0x00, 0x00, 0x00,                          // Total length of outer array (52 bytes)
            0x10, 0x00, 0x00, 0x00,                          // Length of first inner array (16 bytes)
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // First element of first inner array
            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Second element of first inner array
            0x18, 0x00, 0x00, 0x00,                          // Length of second inner array (24 bytes)
            0x00, 0x00, 0x00, 0x00,                          // Padding to next multiple of 8 bytes
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // First element of second inner array
            0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Second element of second inner array
            0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Third element of second inner array
        }
    },
  };
  "struct"_test = generic_test_case | std::tuple{ generic_test{ .expected = simple{},
                                                                .buffer = {
                                                                    42,             // a
                                                                    0,    0,    0,  // padding
                                                                    4,    0,    0,    0,    'd',  'b',  'u',  's',  0,  // b
                                                                    0,    0,    0,  // padding
                                                                    0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40,  // c
                                                                } } };
  "vector of struct"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = std::vector{ foo::bar{ "example1", 67890 }, { "example2", 13579 }, { "example3", 24680 } },
        .buffer = {
          // Vector size
          76, 0, 0, 0,  // number of elements in vector (little-endian)
          0, 0, 0, 0,   // padding

          // bars[0] - {"example1", 67890}
          8, 0, 0, 0,                                      // string length
          'e', 'x', 'a', 'm', 'p', 'l', 'e', '1', 0,       // string content + null terminator
          0, 0, 0,                                         // + padding
          0x32, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,  // 67890 (little-endian)

          // bars[1] - {"example2", 13579}
          8, 0, 0, 0,                                          // string length
          'e', 'x', 'a', 'm', 'p', 'l', 'e', '2', 0, 0, 0, 0,  // string content + null terminator + padding
          0x0B, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      // 13579 (little-endian)

          // bars[2] - {"example3", 24680}
          8, 0, 0, 0,                                          // string length
          'e', 'x', 'a', 'm', 'p', 'l', 'e', '3', 0, 0, 0, 0,  // string content + null terminator + padding
          0x68, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      // 24680 (little-endian)
        }
    },
  };
  "more complex struct"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = foo{ .a = 12345, .bars = { { "example1", 67890 }, { "example2", 13579 } }, .bars2 = { { "example3", 24680 } }, .b = "end" },
        .buffer = {
          // a
          0x39, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 12345 (little-endian)

          // bars - vector size
          52, 0, 0, 0,  // number of elements in vector

          0, 0, 0, 0,  // padding

          // bars[0] - {"example1", 67890}
          8, 0, 0, 0,                                      // string length
          'e', 'x', 'a', 'm', 'p', 'l', 'e', '1', 0,       // string content + null terminator
          0, 0, 0,                                         // + padding
          0x32, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,  // 67890 (little-endian)

          // bars[1] - {"example2", 13579}
          8, 0, 0, 0,                                      // string length
          'e', 'x', 'a', 'm', 'p', 'l', 'e', '2', 0,       // string content + null terminator
          0, 0, 0,                                         // + padding
          0x0B, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 13579 (little-endian)

          // bars2 - vector size
          28, 0, 0, 0,  // number of elements in vector

          0, 0, 0, 0,  // padding

          // bars2[0] - {"example3", 24680}
          8, 0, 0, 0,                                      // string length
          'e', 'x', 'a', 'm', 'p', 'l', 'e', '3', 0,       // string content + null terminator
          0, 0, 0,                                         // + padding
          0x68, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 24680 (little-endian)

          // b - "end"
          3, 0, 0, 0,       // string length
          'e', 'n', 'd', 0  // string content + null terminator
        }
    },
  };
  "non reflectable struct"_test = [] {
    using value_t = bar_meta;
    value_t value{ 13 };
    std::vector<std::uint8_t> buffer{
      3,   0,   0,   0,             // string length
      'b', 'a', 'r', 0,             // string
      13,  0,   0,   0, 0, 0, 0, 0  // int value
    };
    auto err = adbus::protocol::read_dbus_binary(value, buffer);
    expect(!err) << fmt::format("error: {}", err);
    value_t expected{ 13 };
    expect(value == expected) << fmt::format("expected: {}, got: {}", expected, value);
  };
  "empty map"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = std::map<std::string, std::uint64_t>{},
        .buffer = {
            0, 0, 0, 0,  // size
        },
    },
    generic_test{
        .expected = std::unordered_map<std::string, std::uint64_t>{},
        .buffer = {
            0, 0, 0, 0,  // size
        },
    },
  };
  "map"_test = generic_test_case | std::tuple{
    generic_test{ .expected = std::map<std::string, std::uint64_t>{ { "key1", 123 }, { "key2", 456 } },
                  .buffer = {
      52, 0x00, 0x00, 0x00,  // Length of the array (52 bytes)
      0, 0, 0, 0,            // padding
      // First entry
      0x04, 0x00, 0x00, 0x00,                          // Length of the string key1 (4 bytes)
      'k', 'e', 'y', '1', 0x00,                        // key1
      0, 0, 0, 0, 0, 0, 0,                             // padding
      0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Value 123 (64-bit)
      // Second entry
      0x04, 0x00, 0x00, 0x00,                         // Length of the string key2 (4 bytes)
      'k', 'e', 'y', '2', 0x00,                       // key2
      0, 0, 0, 0, 0, 0, 0,                            // padding
      0xC8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Value 456 (64-bit)
                  } },
  };
  "Map of maps"_test = generic_test_case | std::tuple{
    generic_test{
      .expected = std::map<std::string, std::map<std::string, std::uint64_t>>{ { "outerKey", { { "innerKey", 789 } } } },
      .buffer = {
        52,   0x00, 0x00, 0x00,                               // Length of the outer array (52 bytes)
        0,    0,    0,    0,                                  // padding
        0x08, 0x00, 0x00, 0x00,                               // Length of the string outerKey
        'o',  'u',  't',  'e',  'r',  'K',  'e',  'y', 0x00,  // outerKey
        0,    0,    0,                                        // padding
        28,   0x00, 0x00, 0x00,                               // Length of the inner array (24 bytes)
        0,    0,    0,    0,                                  // padding
        0x08, 0x00, 0x00, 0x00,                               // Length of the string innerKey
        'i',  'n',  'n',  'e',  'r',  'K',  'e',  'y', 0x00,  // innerKey
        0,    0,    0,                                        // padding
        0x15, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00        // Value 789 (64-bit)
        }
    },
  };
  using VariantType = std::variant<std::string, int, double>;
  "variant"_test = generic_test_case | std::tuple{
    generic_test{
        .expected = VariantType { std::string{ "variant" } },
        .buffer = {
            1,   's', 0,                          // length of signature + signature for string
            0,                                    // padding
            7,   0,   0,   0,                     // string length
            'v', 'a', 'r', 'i', 'a', 'n', 't', 0  // string content + null terminator
        },
    },
    generic_test{
      .expected = VariantType { 123456 },
      .buffer = {
        1,  'i', 0,    // length of signature + signature for int
        0,             // padding
        64, 226, 1, 0  // int value (123456 in little-endian format)
      },
    },
    generic_test{
      .expected = VariantType { 1337.42 },
      .buffer = {
        1,    'd',  0,                                  // length of signature + signature for double
        0,    0,    0,    0,    0,                      // padding
        0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40  // double value (6.28 in little-endian format)
      },
    },
  };
}
