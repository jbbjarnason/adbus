#pragma once

#include <bit>

namespace adbus::message {

namespace details {
static constexpr auto get_endian() -> std::endian {
  if constexpr (std::endian::native == std::endian::little) {
    return std::endian::little;
  } else if constexpr (std::endian::native == std::endian::big) {
    return std::endian::big;
  } else {
    []<bool flag = false> {
      static_assert(flag, "Mixed endian unsupported");
    };
  }
}


}  // namespace details

struct header {
  static constexpr auto endian{ details::get_endian() };
};

}  // namespace adbus::message
