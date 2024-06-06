#include <array>
#include <tuple>

#include <fmt/format.h>
#include <glaze/glaze.hpp>
#include <boost/ut.hpp>

#include <adbus/protocol/write.hpp>

using namespace boost::ut;

template <glz::detail::num_t type>
struct number_test {
  type value{};
  std::array<std::uint8_t, sizeof(type)> expected{};
};
constexpr auto to_hex_vector(auto&& iterative) {
  constexpr auto to_string = [](auto value) {
    return fmt::format("0x{:02x}", value);
  };
  std::vector<std::string> output{};
  std::transform(std::begin(iterative), std::end(iterative), std::back_inserter(output), to_string);
  return output;
}
template <typename number_type>
constexpr auto format_as(number_test<number_type> test) -> std::string {
  if constexpr (std::integral<number_type>) {
    return fmt::format("Type: {}, Value: 0x{:04x}, Expected: {}", glz::name_v<number_type>, test.value, fmt::join(to_hex_vector(test.expected), ", "));
  } else {
    return fmt::format("Type: {}, Value: {}, Expected: {}", glz::name_v<number_type>, test.value, fmt::join(to_hex_vector(test.expected), ", "));
  }
}


int main() {
  using adbus::protocol::write_dbus_binary;

  "number types"_test = [](auto&& test) {
    std::string buffer{};
    auto err = write_dbus_binary(test.value, buffer);
    expect(!err);
    expect(buffer.size() == sizeof(test.value));
    expect(std::equal(buffer.begin(), buffer.end(), test.expected.begin(), test.expected.end(), [](auto&& a, auto&& b) {
      auto foo =  static_cast<std::uint8_t>(a) == static_cast<std::uint8_t>(b);
      if (!foo) {
        fmt::print("a: {}, b: {}\n", a, b);
      }
      return foo;
    })) << fmt::format("Got: {}, Expected: {}", fmt::join(to_hex_vector(test.expected), ", "), test);
  } | std::tuple{
    number_test<std::uint8_t>{ .value = 0x12, .expected = { 0x12 } },
    number_test<std::uint16_t>{ .value = 0x1234, .expected = { 0x34, 0x12 } },
    number_test<std::uint32_t>{ .value = 0x12345678, .expected = { 0x78, 0x56, 0x34, 0x12 } },
    number_test<std::uint64_t>{ .value = 0x123456789abcdef0, .expected = { 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12 } },
    number_test<std::int16_t>{ .value = -0x1234, .expected = { 0xcc, 0xed } },
    number_test<std::int32_t>{ .value = -0x12345678, .expected = { 0x88, 0xa9, 0xcb, 0xed } },
    number_test<std::int64_t>{ .value = -0x123456789abcdef0, .expected = { 0x10, 0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0xed } },
    number_test<double>{ .value = 1337.42, .expected = { 0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0x40 } },
    number_test<double>{ .value = -1337.42, .expected = { 0x48, 0xe1, 0x7a, 0x14, 0xae, 0xe5, 0x94, 0xc0 } },
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
    expect(buffer.size() ==
      sizeof(std::uint32_t) // The number in front of string indicating string len
      + value.size() // The actual string length excluding any null terminator
      + 1 // The null terminator
      );
    // The expected buffer is the size of the string length + the string + the null terminator
    auto compare = std::vector<std::uint8_t>{ static_cast<std::uint8_t>(value.size()), 0, 0, 0, 't','h','i','s',' ','i','s',' ','a',' ','m','e','s','s','a','g','e','\0' };
    expect(std::equal(buffer.begin(), buffer.end(), std::begin(compare), std::end(compare), [](auto&& a, auto&& b) {
      auto foo =  static_cast<std::uint8_t>(a) == static_cast<std::uint8_t>(b);
      if (!foo) {
        fmt::print("a: {}, b: {}\n", a, b);
      }
      return foo;
    }));
  } | std::tuple{ std::string{ "this is a message" }, std::string_view{ "this is a message" } };

  "string too long"_test = [] { // todo this is slow
    std::string value{};
    constexpr auto n{ static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 10 };
    value.resize(n);
    std::fill_n(std::begin(value), n, 'a');
    std::string buffer{};
    auto err = write_dbus_binary(value, buffer);
    expect(!!err);
    expect(err.code == adbus::protocol::error_code::string_too_long);
  };
}
