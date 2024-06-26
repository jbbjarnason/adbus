#include <array>
#include <tuple>

#include <fmt/format.h>

#include <boost/ut.hpp>
#include <forward_list>
#include <glaze/glaze.hpp>
#include <set>
#include <unordered_set>

#include <adbus/protocol/methods.hpp>
#include <adbus/protocol/signature.hpp>
#include <adbus/protocol/write.hpp>

#include "common.hpp"

using namespace boost::ut;
using std::string_view_literals::operator""sv;
using std::string_literals::operator""s;

template <glz::detail::num_t type>
struct number_test {
  type value{};
  std::array<std::uint8_t, sizeof(type)> expected{};
};
constexpr auto to_hex_vector(auto&& iterative) {
  constexpr auto to_string = [](auto value) { return fmt::format("0x{:02x}", value); };
  std::vector<std::string> output{};
  std::transform(std::begin(iterative), std::end(iterative), std::back_inserter(output), to_string);
  return output;
}
template <typename number_type>
constexpr auto format_as(number_test<number_type> test) -> std::string {
  if constexpr (std::integral<number_type>) {
    return fmt::format("Type: {}, Value: 0x{:04x}, Expected: {}", glz::name_v<number_type>, test.value,
                       fmt::join(to_hex_vector(test.expected), ", "));
  } else {
    return fmt::format("Type: {}, Value: {}, Expected: {}", glz::name_v<number_type>, test.value,
                       fmt::join(to_hex_vector(test.expected), ", "));
  }
}

template <typename value_t>
struct padding_test {
  value_t value;
  std::size_t offset;
  std::size_t padding{ sizeof(value) - offset };
};

constexpr auto uint8_cmp{ [](auto&& a, auto&& b) -> bool {
  auto foo = static_cast<std::uint8_t>(a) == static_cast<std::uint8_t>(b);
  if (!foo) {
    fmt::print("a: {}, b: {}\n", static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(b));
  }
  return foo;
} };

int main() {
  using adbus::protocol::write_dbus_binary;

  "number types"_test =
      [](auto&& test) {
        std::string buffer{};
        auto err = write_dbus_binary(test.value, buffer);
        expect(!err);
        expect(buffer.size() == sizeof(test.value));
        expect(std::equal(buffer.begin(), buffer.end(), test.expected.begin(), test.expected.end(), uint8_cmp))
            << fmt::format("Got: {}, Expected: {}", fmt::join(to_hex_vector(test.expected), ", "), test);
      } |
      std::tuple{
        number_test<std::uint8_t>{ .value = 0x12, .expected = { 0x12 } },
        number_test<std::uint16_t>{ .value = 0x1234, .expected = { 0x34, 0x12 } },
        number_test<std::uint32_t>{ .value = 0x12345678, .expected = { 0x78, 0x56, 0x34, 0x12 } },
        number_test<std::uint64_t>{ .value = 0x123456789abcdef0,
                                    .expected = { 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12 } },
        number_test<std::int16_t>{ .value = -0x1234, .expected = { 0xcc, 0xed } },
        number_test<std::int32_t>{ .value = -0x12345678, .expected = { 0x88, 0xa9, 0xcb, 0xed } },
        number_test<std::int64_t>{ .value = -0x123456789abcdef0,
                                   .expected = { 0x10, 0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0xed } },
        number_test<double>{ .value = 1337.42, .expected = { 0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40 } },
        number_test<double>{ .value = -1337.42, .expected = { 0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0xc0 } },
      };

  "enum as number"_test = [] {
    std::string buffer{};
    auto err = write_dbus_binary(enum_as_number::b, buffer);
    expect(!err);
    expect(buffer.size() == sizeof(std::uint8_t));
    expect(buffer[0] == std::to_underlying(enum_as_number::b));
  };

  "enum as string"_test = [] {
    std::string buffer{};
    auto err = write_dbus_binary(enum_as_string::b, buffer);
    expect(!err);
    auto compare = std::vector<std::uint8_t>{ 1, 0, 0, 0, 'b', 0 };
    expect(buffer.size() == compare.size());
    expect(std::equal(buffer.begin(), buffer.end(), compare.begin(), compare.end(), uint8_cmp));
  };

  "bool"_test = [](auto value) {
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!err);
    expect(buffer.size() == sizeof(std::uint32_t));
    auto compare = std::array<std::uint8_t, 4>{ static_cast<std::uint8_t>(value), 0x00, 0x00, 0x00 };
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare)));
  } | std::vector{ true, false };

  "string"_test = [](auto value) {
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!err);
    expect(buffer.size() == sizeof(std::uint32_t)  // The number in front of string indicating string len
                                + value.size()     // The actual string length excluding any null terminator
                                + 1                // The null terminator
    );
    // The expected buffer is the size of the string length + the string + the null terminator
    // clang-format off
    auto compare = std::vector<std::uint8_t>{ static_cast<std::uint8_t>(value.size()), 0, 0, 0, 't','h','i','s',' ','i','s',' ','a',' ','m','e','s','s','a','g','e','\0' };
    // clang-format on
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  } | std::tuple{ std::string{ "this is a message" }, std::string_view{ "this is a message" } };

  "string too long"_test = [] {  // todo this is slow
    std::string value{};
    constexpr auto n{ static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 10 };
    value.resize(n);
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!!err);
    expect(err.code == adbus::protocol::error_code::string_too_long);
  };

  "signature"_test = [] {
    static_assert(adbus::type::is_signature<adbus::protocol::type::signature>);
    adbus::protocol::type::signature signature{ adbus::protocol::type::signature_v<foo> };
    expect(std::string_view{ signature } == "(ta(st)a(st)s)"sv);
    std::string buffer{};
    auto err = write_dbus_binary(signature, buffer);
    expect(!err);
    expect(buffer.size() == 16);
    auto compare = std::vector<std::uint8_t>{
      signature.size(), '(', 't', 'a', '(', 's', 't', ')', 'a', '(', 's', 't', ')', 's', ')', '\0'
    };

    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  // example: std::vector<std::uint64_t>{ 10, 20, 30 };
  // little endian
  // | Length (UINT32) | Padding     | Element 1 (UINT64)        | Element 2 (UINT64)        | Element 3 (UINT64)        |
  // |    4 bytes      | 4 bytes     |      8 bytes              |      8 bytes              |      8 bytes              |
  // |  18 00 00 00    | 00 00 00 00 | 0A 00 00 00 00 00 00 00   | 14 00 00 00 00 00 00 00   | 1E 00 00 00 00 00 00 00   |
  // |      24         |      0      |          10               |          20               |            30             |
  "vector trivial value_type"_test =
      [](auto&& value) {
        std::string buffer{};
        auto err = write_dbus_binary(value, buffer);
        expect(!err);
        const auto padding{ 4 };  // todo alignement of std::uint64_t - alignment of std::uint32_t
        const auto size{ sizeof(std::uint32_t) + padding + value.size() * sizeof(std::uint64_t) };
        expect(buffer.size() == size) << fmt::format("Expected: {}, Got: {}", size, buffer.size());
        auto compare = std::vector<std::uint8_t>{
          24, 0, 0, 0,              // size
          0,  0, 0, 0,              // padding
          10, 0, 0, 0, 0, 0, 0, 0,  // 10
          20, 0, 0, 0, 0, 0, 0, 0,  // 20
          30, 0, 0, 0, 0, 0, 0, 0,  // 30
        };
        expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
      } |
      std::tuple{
        std::vector{ 10UL, 20UL, 30UL }, std::array{ 10UL, 20UL, 30UL }, std::deque{ 10UL, 20UL, 30UL },
        std::list{ 10UL, 20UL, 30UL },
        // std::forward_list{ 10UL, 20UL, 30UL }, // todo does not have size member function
        std::set{ 10UL, 20UL, 30UL },
        std::unordered_set{ 30UL, 20UL, 10UL },  // inversion because orders differently than set
      };

  // Note that the alignment padding for the first element is required even if there is no first element (an empty array,
  // where n is zero).
  "Empty array"_test = [] {
    std::string buffer{};
    auto err = write_dbus_binary(std::vector<std::uint64_t>{}, buffer);
    expect(!err);
    const auto padding{ 4 };  // todo alignement of std::uint64_t - alignment of std::uint32_t
    const auto size{ sizeof(std::uint32_t) + padding };
    expect(buffer.size() == size) << fmt::format("Expected: {}, Got: {}", size, buffer.size());
    auto compare = std::vector<std::uint8_t>{
      0, 0, 0, 0,  // size
      0, 0, 0, 0,  // padding
    };
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "vector of strings"_test =
      [](auto&& value) {
        std::string buffer{};
        auto err = write_dbus_binary(value, buffer);
        expect(!err);
        const auto padding{ 0 };  // alignment of string is same as std::uint32_t
        const auto size{ sizeof(std::uint32_t) + padding + value.size() * 8 };
        expect(buffer.size() == size) << fmt::format("Expected: {}, Got: {}", size, buffer.size());
        auto compare = std::vector<std::uint8_t>{
          24, 0, 0, 0,  // size
          // no padding
          3, 0, 0, 0, 'b', 'a', 'r', '\0',  // bar
          3, 0, 0, 0, 'b', 'a', 'z', '\0',  // baz
          3, 0, 0, 0, 'f', 'o', 'o', '\0',  // foo
        };
        expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
      } |
      std::tuple{
        std::vector{ "bar"s, "baz"s, "foo"s }, std::array{ "bar"s, "baz"s, "foo"s }, std::deque{ "bar"s, "baz"s, "foo"s },
        std::list{ "bar"s, "baz"s, "foo"s },
        // std::forward_list{ "bar"s, "baz"s, "foo"s }, // todo does not have size member function
        std::set{ "bar"s, "baz"s, "foo"s },
        std::unordered_set{
            "foo"s,
            "baz"s,
            "bar"s,
        },  // inversion because orders differently than set
      };

  // Now let's try array of strings with padding inbetween
  // | Array Length (UINT32) | Length 1 (UINT32) | String 1 (std::string)  | Length 2 (UINT32) | String 2 (std::string)  |
  // Length 3 (UINT32) | String 3 (std::string)  | |        4 bytes        |      4 bytes      |       8 bytes (6+2)     | 4
  // bytes      |       8 bytes (5+3)     |      4 bytes      |       6 bytes (6)       | |     22 00 00 00       |    05 00
  // 00 00    | 68 65 6C 6C 6F 00 00 00 |    04 00 00 00    | 64 62 75 73 00 00 00 00 |    05 00 00 00    | 77 6F 72 6C 64 00
  // | |         34            |         5         |  h  e  l  l  o \0 \0 \0 |         4         |  d  b  u  s \0 \0 \0 \0 |
  // 5         |  w  o  r  l  d \0       |
  "vector of strings with padding"_test = [](auto&& value) {
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!err);
    constexpr auto size{ 38 };
    expect(buffer.size() == size) << fmt::format("Expected: {}, Got: {}", size, buffer.size());
    auto compare = std::vector<std::uint8_t>{
      34,  0,   0,   0,                  // size
      5,   0,   0,   0,                  // length 1
      'h', 'e', 'l', 'l', 'o', 0, 0, 0,  // string 1
      4,   0,   0,   0,                  // length 2
      'd', 'b', 'u', 's', 0,   0, 0, 0,  // string 2
      5,   0,   0,   0,                  // length 3
      'w', 'o', 'r', 'l', 'd', 0,        // string 3
    };
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  } | std::tuple{ std::vector{ "hello"s, "dbus"s, "world"s } };

  "Empty vector of vectors"_test = [] {
    std::string buffer{};
    auto err = write_dbus_binary(std::vector<std::vector<std::uint64_t>>{}, buffer);
    expect(!err);
    // TODO: IS THIS CORRECT ????????
    auto compare = std::vector<std::uint8_t>{
      0, 0, 0, 0,  // size outer
      // no padding
      // 0, 0, 0, 0,  // size inner
      // 0, 0, 0, 0,  // padding
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "Non-empty vector of vectors with padding"_test = [] {
    std::string buffer{};
    std::vector<std::vector<std::uint64_t>> vec = { { 1, 2 }, { 3, 4, 5 } };
    auto err = write_dbus_binary(vec, buffer);
    expect(!err);

    // Correct comparison vector with padding
    auto compare = std::vector<std::uint8_t>{
      0x34, 0x00, 0x00, 0x00,                          // Total length of outer array (52 bytes)
      0x10, 0x00, 0x00, 0x00,                          // Length of first inner array (16 bytes)
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // First element of first inner array
      0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Second element of first inner array
      0x18, 0x00, 0x00, 0x00,                          // Length of second inner array (24 bytes)
      0x00, 0x00, 0x00, 0x00,                          // Padding to next multiple of 8 bytes
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // First element of second inner array
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Second element of second inner array
      0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Third element of second inner array
    };

    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "struct"_test = [] {
    std::string buffer{};
    auto err = write_dbus_binary(simple{}, buffer);
    expect(!err);
    auto compare = std::vector<std::uint8_t>{
      42,                                                 // a
      0,    0,    0,                                      // padding
      4,    0,    0,    0,    'd',  'b',  'u',  's',  0,  // b
      0,    0,    0,                                      // padding
      0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40,     // c
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "vector of struct"_test = [] {
    // Create a vector of foo::bar instances
    std::vector<foo::bar> bars{ { "example1", 67890 }, { "example2", 13579 }, { "example3", 24680 } };

    std::string buffer{};
    auto err = write_dbus_binary(bars, buffer);
    expect(!err);

    auto compare = std::vector<std::uint8_t>{
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
    };

    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "more complex struct"_test = [] {
    foo test_foo{
      .a = 12345, .bars = { { "example1", 67890 }, { "example2", 13579 } }, .bars2 = { { "example3", 24680 } }, .b = "end"
    };

    std::string buffer{};
    auto err = write_dbus_binary(test_foo, buffer);
    expect(!err);

    auto compare = std::vector<std::uint8_t>{
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
    };

    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "non reflectable struct"_test = [] {
    std::string buffer{};
    auto err = write_dbus_binary(bar_meta{ 13 }, buffer);
    expect(!err);
    auto compare = std::vector<std::uint8_t>{
      //
      3,   0,   0,   0,             // string length
      'b', 'a', 'r', 0,             // string
      13,  0,   0,   0, 0, 0, 0, 0  // int value
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "A struct must start on an 8-byte boundary regardless of the type of the struct fields"_test = [] {
    std::string buffer{};
    buffer.resize(1);
    auto err = write_dbus_binary(simple{}, buffer);
    expect(!err);
    auto compare = std::vector<std::uint8_t>{
      0,                                                  // previous buffer
      0,    0,    0,    0,    0,    0,    0,              // padding
      42,                                                 // a
      0,    0,    0,                                      // padding
      4,    0,    0,    0,    'd',  'b',  'u',  's',  0,  // b
      0,    0,    0,                                      // padding
      0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40,     // c
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "Empty map"_test = [](auto&& value) {
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!err);

    auto compare = std::vector<std::uint8_t>{
      0x00, 0x00, 0x00, 0x00  // Length of the array (0 bytes)
    };

    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  } | std::tuple{ std::map<std::string, std::uint64_t>{}, std::unordered_map<std::string, std::uint64_t>{} };

  "map"_test =
      [](auto&& value) {
        std::string buffer{};
        auto err = write_dbus_binary(value, buffer);
        expect(!err);

        // Example marshaled data
        auto compare = std::vector<std::uint8_t>{
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
        };

        expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
        expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
      } |
      std::tuple{
        std::map<std::string, std::uint64_t>{ { "key1", 123 }, { "key2", 456 } },
        // todo probably differently ordered
        // std::unordered_map<std::string, std::uint64_t>{ { "key1", 123 }, { "key2", 456 } }
      };
  "Map of maps"_test = [] {
    std::string buffer{};
    std::map<std::string, std::map<std::string, std::uint64_t>> map = { { "outerKey", { { "innerKey", 789 } } } };
    auto err = write_dbus_binary(map, buffer);
    expect(!err);

    // Example marshaled data
    auto compare = std::vector<std::uint8_t>{
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
    };

    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), uint8_cmp));
  };

  "variant string"_test = [] {
    using VariantType = std::variant<std::string, int, double>;

    VariantType value = std::string{ "variant" };
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!err);

    auto compare = std::vector<std::uint8_t>{
      1,   's', 0,                          // length of signature + signature for string
      0,                                    // padding
      7,   0,   0,   0,                     // string length
      'v', 'a', 'r', 'i', 'a', 'n', 't', 0  // string content + null terminator
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), compare.begin(), compare.end(), uint8_cmp));
  };

  "variant int"_test = [] {
    using VariantType = std::variant<std::string, std::int32_t, double>;

    VariantType value = 123456;
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!err);

    auto compare = std::vector<std::uint8_t>{
      1,  'i', 0,    // length of signature + signature for int
      0,             // padding
      64, 226, 1, 0  // int value (123456 in little-endian format)
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), compare.begin(), compare.end(), uint8_cmp));
  };

  "variant double test"_test = [] {
    using VariantType = std::variant<std::string, int, double>;

    VariantType value = 1337.42;
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!err);

    auto compare = std::vector<std::uint8_t>{
      1,    'd',  0,                                  // length of signature + signature for double
      0,    0,    0,    0,    0,                      // padding
      0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40  // double value (6.28 in little-endian format)
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    expect(std::equal(buffer.begin(), buffer.end(), compare.begin(), compare.end(), uint8_cmp));
  };

  "header with path"_test = [] {
    static_assert(adbus::protocol::type::signature_v<adbus::protocol::header::field_path> == "o"sv);
    namespace header = adbus::protocol::header;
    std::string buffer{};
    auto hello = header::header{
      .type = header::message_type_e::method_call,
      .flags = {},
      .body_length = 0,
      .serial = 1,
      .fields = { { header::field_path{ adbus::protocol::path::make("/org/freedesktop/DBus").value() } } }
    };
    auto err = write_dbus_binary(hello, buffer);
    expect(!err);
    std::vector<std::uint8_t> compare{ 'l',               // endian
                                       1,                 // message type method call
                                       0,                 // flags none
                                       1,                 // version 1
                                       0,   0,   0,   0,  // body length
                                       1,   0,   0,   0,  // serial
                                       30,  0,   0,   0,  // field array byte length todo
                                       1,                 // field code of PATH
                                       1,                 // signature length
                                       'o',               // signature
                                       0,                 // null terminator
                                       21,  0,   0,   0,  // size of string
                                       '/', 'o', 'r', 'g', '/', 'f', 'r', 'e', 'e', 'd', 'e',
                                       's', 'k', 't', 'o', 'p', '/', 'D', 'B', 'u', 's', 0,
                                       0, 0  // data always start at 8 bit alignment, therefore padding is added
    };
    expect(buffer.size() == compare.size()) << fmt::format("Expected: {}, Got: {}", compare.size(), buffer.size());
    for (std::size_t i{}; i < buffer.size(); ++i) {
      expect(buffer[i] == compare[i]) << fmt::format("Expected: {}, Got: {} at index {}", compare[i],
                                                     static_cast<std::uint8_t>(buffer[i]), i);
      if (buffer[i] != compare[i]) {
        return;
      }
    }
    expect(std::equal(buffer.begin(), buffer.end(), compare.begin(), compare.end(), uint8_cmp));
  };

  "alignment or padding"_test =
      [](auto test) {
        std::string buffer{};
        buffer.resize(test.offset);
        auto err = write_dbus_binary(test.value, buffer);
        expect(!err);
        std::size_t size{};
        if constexpr (glz::detail::string_like<std::decay_t<decltype(test.value)>>) {
          size = test.offset + test.padding + sizeof(std::uint32_t) + test.value.size() + 1;
        } else {
          size = test.offset + test.padding + sizeof(test.value);
        }
        expect(buffer.size() == size) << fmt::format("Expected: {}, Got: {} for offset: {}", size, buffer.size(),
                                                     test.offset);
      } |
      std::tuple{
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 1 },
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 2 },
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 3 },
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 4 },
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 5 },
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 6 },
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 7 },
        padding_test<std::uint64_t>{ .value = 0x1234, .offset = 8 },
        padding_test<std::uint32_t>{ .value = 0x1234, .offset = 1 },
        padding_test<std::uint32_t>{ .value = 0x1234, .offset = 2 },
        padding_test<std::uint32_t>{ .value = 0x1234, .offset = 3 },
        padding_test<std::uint32_t>{ .value = 0x1234, .offset = 4 },
        padding_test<std::uint16_t>{ .value = 0x1234, .offset = 1 },
        padding_test<std::uint16_t>{ .value = 0x1234, .offset = 2 },
        padding_test<std::uint8_t>{ .value = 0x12, .offset = 1 },
        padding_test<std::string>{ .value = "foo", .offset = 1, .padding = 3 },
        padding_test<std::string>{ .value = "foo", .offset = 2, .padding = 2 },
        padding_test<std::string>{ .value = "foo", .offset = 3, .padding = 1 },
        padding_test<std::string>{ .value = "foo", .offset = 4, .padding = 0 },
      };
}
