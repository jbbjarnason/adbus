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

  "integer types"_test = [](auto&& test) {
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
    number_test<double>{ .value = 1337.42, .expected = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x40, 0x40 } },
  };

}
