#include <string_view>

#include <adbus/util/string_literal.hpp>
#include <adbus/protocol/message_header.hpp>
#include <adbus/protocol/signature.hpp>

namespace adbus::protocol {
using std::string_view_literals::operator""sv;

static_assert(type::signature_v<header::message_type_e> == "y"sv);
static_assert(type::signature_v<header::flags_t> == "y"sv);

#if __cpp_static_assert >= 202306L
static_assert(glz::detail::count_members<header::header> == 7);
static_assert(type::signature_v<header::header> == "(yyyyuua(yv))"sv, adbus::util::join_v<adbus::util::chars<"got: \"">, type::signature_v<header::header>, adbus::util::chars<"\" expected: \"(yyyyuua(yv))\"">>);
#else
static_assert(type::signature_v<header::header> == "yyyyuu"sv);
#endif
}

int main() {
  return 0;
}
