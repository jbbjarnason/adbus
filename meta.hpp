#pragma once

#include <concepts>
#include <string_view>

namespace adbus::protocol {

template <typename value_t>
struct meta;

template <typename value_t>
concept meta_t = requires {
  typename meta<value_t>;
  requires std::same_as<decltype(meta<value_t>::name), std::string_view>;
  requires meta<value_t>::value;
};

//template <typename ... member_t>
//static constexpr auto object()


}
