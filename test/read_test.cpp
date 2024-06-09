#include <array>
#include <tuple>

#include <fmt/format.h>

#include <boost/ut.hpp>
#include <forward_list>
#include <glaze/glaze.hpp>
#include <set>
#include <unordered_set>

#include <adbus/protocol/signature.hpp>
#include <adbus/protocol/read.hpp>

using namespace boost::ut;
using std::string_view_literals::operator""sv;
using std::string_literals::operator""s;


int main() {
  using adbus::protocol::read_dbus_binary;

  "number types"_test = [] {
  };
}
