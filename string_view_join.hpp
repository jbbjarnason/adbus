#pragma once

#include <string_view>
#include <array>

namespace adbus::details {

template <std::array V>
struct make_static {
  static constexpr auto value = V;
};

template <const std::string_view&... Strs>
constexpr std::string_view join() {
  constexpr auto joined_arr = []() {
    constexpr size_t len = (Strs.size() + ... + 0);
    std::array<char, len + 1> arr{};
    auto append = [i = 0, &arr](const auto& s) mutable {
      for (auto c : s)
        arr[i++] = c;
    };
    (append(Strs), ...);
    arr[len] = 0;
    return arr;
  }();
  auto& static_arr = make_static<joined_arr>::value;
  return { static_arr.data(), static_arr.size() - 1 };
}
// Helper to get the value out
template <const std::string_view&... Strs>
constexpr auto join_v = join<Strs...>();

}  // namespace adbus::details
