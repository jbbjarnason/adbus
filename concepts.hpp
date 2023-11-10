#pragma once

namespace adbus::concepts {

// from
// https://stackoverflow.com/questions/16337610/how-to-know-if-a-type-is-a-specialization-of-stdvector
template <class, template <class...> class>
constexpr bool is_specialization_v = false;

template <template <class...> class T, class... Args>
constexpr bool is_specialization_v<T<Args...>, T> = true;

}

