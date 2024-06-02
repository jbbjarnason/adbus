#pragma once

#include <concepts>
#include <string_view>

#include <glaze/tuplet/tuple.hpp>

namespace adbus::protocol {

namespace detail {
template <typename type_t>
struct pack_t {
  type_t value;
};
}  // namespace detail

template <typename value_t>
struct meta;

template <typename value_t>
concept meta_t = requires {
  typename meta<value_t>;
  requires std::same_as<decltype(meta<value_t>::name), std::string_view>;
  requires meta<value_t>::value;
};

constexpr auto pack(auto&&... args) {
  if constexpr (sizeof...(args) == 0) {
    return detail::pack_t{ glz::tuplet::tuple{} };
  } else {
    return detail::pack_t{ glz::tuplet::tuple{ std::forward<decltype(args)>(args)... } };
  }
}

}  // namespace adbus::protocol
