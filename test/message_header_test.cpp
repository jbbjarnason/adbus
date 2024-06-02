#include <string_view>

#include <adbus/util/string_literal.hpp>
#include <adbus/protocol/message_header.hpp>
#include <adbus/protocol/signature.hpp>

namespace adbus::protocol {
using std::string_view_literals::operator""sv;

#if __cpp_static_assert >= 202306L
static_assert(type::signature_v<header::header> == "yyyyuu"sv, adbus::util::join_v<adbus::util::chars<"got: \"">, type::signature_v<header::header>, adbus::util::chars<"\" expected: \"yyyyuu\"">>);
#else
static_assert(type::signature_v<header::header> == "yyyyuu"sv);
#endif
}

int main() {
  return 0;
}