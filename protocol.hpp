#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

namespace adbus::concepts {

template <typename presumed_integer_t, typename integer_t>
concept explicit_unsigned_integral = requires {
  requires not std::same_as<std::remove_cv_t<integer_t>, bool>;
  requires std::unsigned_integral<integer_t>;
  requires std::same_as<presumed_integer_t, integer_t>;
};

template <typename presumed_integer_t, typename integer_t>
concept explicit_signed_integral = requires {
  requires not std::same_as<std::remove_cv_t<integer_t>, bool>;
  requires std::signed_integral<integer_t>;
  requires std::same_as<presumed_integer_t, integer_t>;
};

}  // namespace adbus::concepts

namespace adbus::details {

template <typename char_t = char, std::size_t N>
[[nodiscard]] consteval std::array<char_t, N + 1> make_static_buffer(std::string const& in) {
  std::array<char_t, N + 1> buffer{};
  std::move(in.begin(), in.end(), buffer.begin());
  return buffer;
}

template <typename char_t = char>
[[nodiscard]] consteval auto join_string_view_to(std::output_iterator<char_t> auto out, std::basic_string_view<char_t> view)
    -> decltype(out) {
  return std::copy(view.begin(), view.end(), out);
}

template <typename char_t = char, typename... view_t>
  requires(std::same_as<std::string_view, std::remove_cvref_t<view_t>> && ...)
[[nodiscard]] consteval std::basic_string_view<char_t> join_string_views(view_t ... views) {
  std::basic_string<char_t> buffer{};
  auto out = std::back_inserter(buffer);
  (..., (out = join_string_view_to(out, views)));
  static constexpr auto size{ buffer.size() };
  static constexpr auto array_buffer{ make_static_buffer<char_t, size>(buffer) };
  return std::basic_string_view<char_t>{ array_buffer.begin(), array_buffer.end() };
}

using std::string_view_literals::operator""sv;
static_assert(join_string_views("foo"sv, "bar"sv) == "foobar"sv);

}  // namespace adbus::details

namespace adbus::protocol::type {

using std::string_view_literals::operator""sv;

template <typename type_t>
struct signature : std::false_type {};

template <typename type_t>
static constexpr auto signature_v{ signature<type_t>::value };

template <>
struct signature<std::uint8_t> {
  static constexpr auto value{ "y"sv };
};

template <std::same_as<bool> value_t>
struct signature<value_t> {
  static constexpr auto value{ "b"sv };
};

template <concepts::explicit_signed_integral<std::int16_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "n"sv };
};

template <concepts::explicit_unsigned_integral<std::uint16_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "q"sv };
};

template <concepts::explicit_signed_integral<std::int32_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "i"sv };
};

template <concepts::explicit_unsigned_integral<std::uint32_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "u"sv };
};

template <concepts::explicit_signed_integral<std::int64_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "x"sv };
};

template <concepts::explicit_unsigned_integral<std::uint64_t> value_t>
struct signature<value_t> {
  static constexpr auto value{ "t"sv };
};

template <std::same_as<double> value_t>
struct signature<value_t> {
  static constexpr auto value{ "d"sv };
};

// unix_fd ?

template <typename type_t>
  requires(std::same_as<std::remove_cvref_t<type_t>, std::string_view> ||
           std::same_as<std::remove_cvref_t<type_t>, std::string>)
struct signature<type_t> {
  static constexpr auto value{ "s"sv };
};

// * The path may be of any length.
// * The path must begin with an ASCII '/' (integer 47) character, and must consist of elements separated by slash
// characters.
// * Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_"
// * No element may be the empty string.
// * Multiple '/' characters cannot occur in sequence.
// * A trailing '/' character is not allowed unless the path is the root path (a single '/' character).
struct path {
  struct error {
    enum struct code_e : std::uint8_t {
      no_error = 0,
      empty,
      not_absolute,  // does not start with slash
      trailing_slash,
      invalid_character,
      multiple_slashes,
    };
    using index_t = std::size_t;

    constexpr bool operator()() const noexcept { return code != code_e::no_error; }
    constexpr bool operator==(error const&) const noexcept = default;

    code_e code{ code_e::no_error };
    index_t index{ 0 };
  };

  static constexpr error validate(std::string_view input) {
    using enum error::code_e;
    if (input.empty()) {
      return error{ .code = empty, .index = 0 };
    }
    if (input.front() != '/') {
      return error{ .code = not_absolute, .index = 0 };
    }
    if (input.size() == 1) {
      return error{};
    }
    if (input.back() == '/') {
      return error{ .code = trailing_slash, .index = input.size() - 1 };
    }
    for (auto it = input.begin() + 1; it != input.end(); ++it) {
      if (*it == '/' && *(it - 1) == '/') {
        return error{ .code = multiple_slashes, .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
      // We have already checked the first character so this should be safe.
      if (!((*it >= 'A' && *it <= 'Z') || (*it >= 'a' && *it <= 'z') || (*it >= '0' && *it <= '9') || *it == '_' ||
            *it == '/')) {
        return error{ .code = invalid_character, .index = static_cast<error::index_t>(std::distance(input.begin(), it)) };
      }
    }
    return error{};
  }

  static constexpr std::expected<path, error> make(std::string_view input) {
    auto err = validate(input);
    if (err()) {
      return std::unexpected{ err };
    }
    return path{ .buffer = std::string{ input } };
  }
  std::string buffer{};
};

static_assert(path::validate("/foo/bar") == path::error{});
static_assert(path::validate("/") == path::error{});
static_assert(path::validate("/a") == path::error{});
static_assert(path::validate("") == path::error{ .code = path::error::code_e::empty, .index = 0 });
static_assert(path::validate("//") == path::error{ .code = path::error::code_e::trailing_slash, .index = 1 });
static_assert(path::validate("///") == path::error{ .code = path::error::code_e::trailing_slash, .index = 2 });
static_assert(path::validate("/ab/") == path::error{ .code = path::error::code_e::trailing_slash, .index = 3 });
static_assert(path::validate("///a") == path::error{ .code = path::error::code_e::multiple_slashes, .index = 1 });
static_assert(path::validate("/a.b") == path::error{ .code = path::error::code_e::invalid_character, .index = 2 });

template <typename type_t>
  requires std::same_as<std::remove_cvref_t<type_t>, path>
struct signature<type_t> {
  static constexpr auto value{ "o"sv };
};

template <typename type_t>
concept has_signature = requires {
  signature<type_t>::value;
  requires std::same_as<std::remove_const_t<decltype(signature<type_t>::value)>, std::string_view>;
};

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

static_assert((not has_signature<char*>));
static_assert((not has_signature<const char*>));
static_assert((not has_signature<std::int8_t>));

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

template <has_signature type_t, has_signature... types_t>
struct signature<std::variant<type_t, types_t...>> {
  static constexpr auto value{ "v"sv };
};

static_assert(signature<std::variant<std::uint8_t>>::value == "v"sv);
static_assert(signature<std::variant<std::uint8_t, std::string>>::value == "v"sv);
static_assert(has_signature<std::variant<std::uint8_t, std::string>>);
static_assert(!has_signature<std::variant<std::int8_t>>);

template <std::ranges::range type_t>
  requires has_signature<std::ranges::range_value_t<type_t>>
struct signature<type_t> {
  static constexpr auto value{ "a"sv };
};

template <has_signature... types_t>
std::string_view constexpr composed_signature(types_t&&... types) {
  return {};  // todo implement
}

}  // namespace adbus::protocol::type
