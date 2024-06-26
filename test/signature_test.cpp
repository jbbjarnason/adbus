#include <array>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
#include <vector>

#include <glaze/tuplet/tuple.hpp>

#include <adbus/util/concepts.hpp>
#include <adbus/protocol/signature.hpp>

#include "common.hpp"

using std::string_view_literals::operator ""sv;
using adbus::util::join_v;
using adbus::util::chars;

template <typename T>
static constexpr auto signature_v = adbus::protocol::type::signature_v<T>;

namespace adbus::protocol::type {

static_assert(has_signature<std::uint8_t>);
static_assert(has_signature<bool>);
static_assert(has_signature<std::int16_t>);
static_assert(has_signature<std::uint16_t>);
static_assert(has_signature<std::int32_t>);
static_assert(has_signature<std::uint32_t>);
static_assert(has_signature<std::int64_t>);
static_assert(has_signature<std::uint64_t>);
static_assert(has_signature<double>);
static_assert(has_signature<std::string_view>);
static_assert(has_signature<std::string>);
static_assert(has_signature<path>);

static_assert(not has_signature<char*>);
static_assert(not has_signature<const char*>);
static_assert(not has_signature<std::int8_t>);

static_assert(signature_v<std::uint8_t> == "y"sv);
static_assert(signature_v<bool> == "b"sv);
static_assert(signature_v<std::int16_t> == "n"sv);
static_assert(signature_v<std::uint16_t> == "q"sv);
static_assert(signature_v<std::int32_t> == "i"sv);
static_assert(signature_v<std::uint32_t> == "u"sv);
static_assert(signature_v<std::int64_t> == "x"sv);
static_assert(signature_v<std::uint64_t> == "t"sv);
static_assert(signature_v<double> == "d"sv);
static_assert(signature_v<std::string_view> == "s"sv);
static_assert(signature_v<std::string> == "s"sv);
static_assert(signature_v<path> == "o"sv);


static_assert(signature_v<std::variant<std::uint8_t>> == "v"sv);
static_assert(signature_v<std::variant<std::uint8_t, std::string>> == "v"sv);
static_assert(has_signature<std::variant<std::uint8_t, std::string>>);
static_assert(!has_signature<std::variant<std::int8_t>>);

static_assert(signature_v<std::vector<int>> == "ai"sv);
static_assert(signature_v<std::array<int, 10>> == "ai"sv);
static_assert(signature_v<std::set<int>> == "ai"sv);


static_assert(signature_v<std::tuple<int, std::string>> == "(is)"sv);
static_assert(signature_v<std::tuple<int, std::string, std::uint8_t>> == "(isy)"sv);
static_assert(signature_v<std::tuple<int, std::array<std::uint8_t, 10>>> == "(iay)"sv);
static_assert(signature_v<std::tuple<int, std::tuple<std::uint8_t, std::string>>> == "(i(ys))"sv);
static_assert(signature_v<std::tuple<int, std::tuple<std::uint8_t, std::tuple<std::string, std::uint8_t>>>> ==
              "(i(y(sy)))"sv);

static_assert(signature_v<std::map<int, std::string>> == "a{is}"sv);
static_assert(signature_v<std::unordered_map<int, std::string>> == "a{is}"sv);
static_assert(signature_v<std::map<std::string, std::tuple<int, std::string>>> == "a{s(is)}"sv);
// nested dicts
static_assert(signature_v<std::map<std::string, std::map<std::string, std::string>>> == "a{sa{ss}}"sv);

struct my_struct {
  std::int32_t a{};
  std::string b{};
};
static_assert(signature_v<my_struct> == "(is)"sv, join_v<chars<"got: \"">, signature_v<my_struct>, chars<"\" expected: \"(is)\"">>);

struct my_struct2 {
  std::int32_t a{};
  std::string b{};
  std::uint8_t c{};
  my_struct d{};
};
static_assert(signature_v<my_struct2> == "(isy(is))"sv, join_v<chars<"got: \"">, signature_v<my_struct2>, chars<"\" expected: \"(isy(is))\"">>);

}  // namespace adbus::protocol::type

struct my_struct3 {
  explicit my_struct3(std::int32_t aa) : a{ aa } {}
  std::int32_t a{};
  std::string b{};
  std::uint8_t c{};
  adbus::protocol::type::my_struct d{};
};

template <>
struct glz::meta<my_struct3> {
  using self = my_struct3;
  static constexpr auto value{ glz::object("a", &self::a, "b", &self::b, "c", &self::c, "d", &self::d) };
};

using foo_t = glz::detail::Object<glz::tuplet::tuple<glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, int my_struct3::*>, glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > my_struct3::*>, glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, unsigned char my_struct3::*>, glz::tuplet::tuple<std::basic_string_view<char, std::char_traits<char> >, adbus::protocol::type::my_struct my_struct3::*> > >;
static_assert(glz::detail::glaze_object_t<my_struct3>);
static_assert(signature_v<my_struct3> == "(isy(is))"sv, join_v<chars<"got: \"">, signature_v<my_struct3>, chars<"\" expected: \"(isy(is))\"">>);

// from common.hpp
static_assert(signature_v<enum_as_number> == "y"sv);
static_assert(signature_v<enum_as_string> == "s"sv);

int main() {
  return 0;
}
