#pragma once

#include <glaze/concepts/container_concepts.hpp>
#include <glaze/core/reflection_tuple.hpp>
#include <glaze/util/expected.hpp>

#include <adbus/core/context.hpp>
#include <adbus/protocol/padding.hpp>
#include <adbus/protocol/signature.hpp>
#include <adbus/util/concepts.hpp>

namespace adbus::protocol {

namespace detail {

template <typename T>
constexpr void skip_padding(auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
  constexpr auto alignment{ padding<T>::value };
  // idx % alignment: This computes the offset of idx from the nearest previous alignment boundary.
  // alignment - (idx % alignment): This calculates how much padding is needed to reach the next alignment boundary.
  // (alignment - (idx % alignment)) % alignment: This ensures that if idx is already aligned (i.e., idx % alignment == 0),
  // the padding is 0 instead of alignment.
  const auto position = static_cast<std::size_t>(std::distance(begin, it));
  const auto padding = (alignment - (position % alignment)) % alignment;
  if (it + padding > end) [[unlikely]] {
    ctx.err = error{ error_code::out_of_range, static_cast<std::size_t>(std::distance(begin, it)) };
    return;
  }
  std::advance(it, padding);
}

template <typename T>
struct from_dbus_binary;

template <num_t T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    using V = std::decay_t<decltype(value)>;
    skip_padding<V>(ctx, begin, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    if (it + sizeof(V) > end) [[unlikely]] {
      ctx.err = error{ error_code::out_of_range, static_cast<std::size_t>(std::distance(begin, it)) };
      return;
    }
    // todo remove this casts
    std::memcpy(reinterpret_cast<void*>(const_cast<V*>(&value)), &*it, sizeof(V));
    std::advance(it, sizeof(V));
  }
};

template <typename T>
  requires(std::is_enum_v<T> && !glz::detail::glaze_enum_t<T>)
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, auto&&... args) noexcept {
    // todo remove const_cast
    auto& non_const_value = const_cast<std::remove_cvref_t<decltype(value)>&>(value);
    using V = std::underlying_type_t<T>;
    // todo reinterpret_cast is not nice, but it provides less code bloat
    from_dbus_binary<V>::template op<Opts>(reinterpret_cast<V&>(non_const_value), std::forward<decltype(args)>(args)...);
  }
};

template <>
struct from_dbus_binary<bool> {
  template <options Opts>
  static constexpr void op(auto&& value, auto&&... args) noexcept {
    std::uint32_t substitute{};
    from_dbus_binary<std::uint32_t>::template op<Opts>(substitute, std::forward<decltype(args)>(args)...);
    value = substitute != 0;
  }
};

template <string_like T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    std::uint32_t size{};
    from_dbus_binary<std::uint32_t>::template op<Opts>(size, ctx, begin, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    if (it + size > end) [[unlikely]] {
      ctx.err = error{ error_code::out_of_range, static_cast<std::size_t>(std::distance(begin, it)) };
      return;
    }
    using V = std::decay_t<decltype(value)>;
    if constexpr (glz::resizable<V>) {
      value.resize(size);
      std::memcpy(value.data(), &*it, size);
    } else if constexpr (glz::is_specialization_v<V, std::basic_string_view>) {
      using char_type = typename V::value_type;
      value = std::basic_string_view<char_type>{ reinterpret_cast<const char_type*>(&*it), size };
    } else {
      static_assert(glz::false_v<V>, "unsupported type");
    }
    std::advance(it, size + 1);  // the +1 is for the null terminator
  }
};

template <adbus::type::is_signature T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    std::uint8_t size{};
    from_dbus_binary<decltype(size)>::template op<Opts>(size, ctx, begin, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    if (it + size > end) [[unlikely]] {
      ctx.err = error{ error_code::out_of_range, static_cast<std::size_t>(std::distance(begin, it)) };
      return;
    }
    std::memcpy(value.data(), &*it, size);
    value.size_ = size;          // todo could we do this differently?
    std::advance(it, size + 1);  // the +1 is for the null terminator
  }
};

template <typename T>
  requires(std::is_enum_v<T> && glz::detail::glaze_enum_t<T>)
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept {
    std::string_view substitute{};
    from_dbus_binary<std::string_view>::template op<Opts>(substitute, ctx, std::forward<decltype(args)>(args)...);
    if (ctx.err) [[unlikely]] {
      return;
    }
    static constexpr auto frozen_map = glz::detail::make_string_to_enum_map<T>();
    const auto& member_it = frozen_map.find(substitute);
    if (member_it != frozen_map.end()) {
      value = member_it->second;
    } else [[unlikely]] {
      ctx.err = error{ error_code::unexpected_enum };
    }
  }
};

template <container T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    std::uint32_t n{};
    from_dbus_binary<std::uint32_t>::template op<Opts>(n, ctx, begin, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    skip_padding<typename std::decay_t<decltype(value)>::value_type>(
        ctx, begin, it, end);  // n does not include the padding after the length
    if (ctx.err) [[unlikely]] {
      return;
    }
    if (it + n > end) [[unlikely]] {
      ctx.err = error{ error_code::out_of_range, static_cast<std::size_t>(std::distance(begin, it)) };
      return;
    }
    using V = std::decay_t<decltype(value)>;
    if constexpr (has_clear<V>) {
      value.clear();
    }
    std::size_t idx{};
    std::int64_t n_signed{ n };
    while (n_signed > 0) {
      typename V::value_type element{};
      auto const beginning_of_element = it;
      from_dbus_binary<typename V::value_type>::template op<Opts>(element, ctx, begin, it, end);
      if (ctx.err) [[unlikely]] {
        return;
      }
      n_signed -= std::distance(beginning_of_element, it);
      if constexpr (glz::detail::emplace_backable<V>) {
        value.emplace_back(std::move(element));
      } else if constexpr (array_like<V>) {
        // array like, todo support tuple
        if (idx < std::tuple_size_v<V>) {
          value[idx] = std::move(element);
        } else [[unlikely]] {
          ctx.err = error{ error_code::out_of_range, static_cast<std::size_t>(std::distance(begin, it)) };
          return;
        }
      } else if constexpr (glz::detail::emplaceable<V>) {
        value.emplace(std::move(element));
      } else {
        static_assert(glz::false_v<V>, "unsupported type");
      }
      idx++;
    }
    if (n_signed != 0) [[unlikely]] {
      ctx.err = error{ error_code::out_of_range, static_cast<std::size_t>(std::distance(begin, it)) };
    }
  }
};

template <glz::detail::pair_t T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& pair, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    // A struct must start on an 8-byte boundary regardless of the type of the struct fields.
    skip_padding<std::uint64_t>(ctx, begin, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    // I think this is safe in this context, only used currently for maps
    from_dbus_binary<typename T::first_type>::template op<Opts>(
        const_cast<std::remove_const_t<typename T::first_type>&>(pair.first), ctx, begin, it, end);
    from_dbus_binary<typename T::second_type>::template op<Opts>(pair.second, ctx, begin, it, end);
  }
};

template <glz::is_variant T>
struct from_dbus_binary<T> {
  static constexpr auto N = std::variant_size_v<T>;

  template <options Opts>
  static constexpr void op(auto&& variant, is_context auto&& ctx, auto&&... args) noexcept {
    type::signature read_signature{};
    from_dbus_binary<type::signature>::template op<Opts>(read_signature, ctx, args...);
    if (ctx.err) [[unlikely]] {
      return;
    }
    bool found_it{};
    glz::for_each<N>([&](auto I) {
      using V = std::decay_t<std::variant_alternative_t<I, T>>;
      // todo how to exit early? probably not possible during run time
      if (!found_it && read_signature == type::signature_v<V>) {
        variant.template emplace<I>();
        from_dbus_binary<V>::template op<Opts>(std::get<I>(variant), ctx, args...);
        found_it = true;
      }
    });
    if (!found_it) [[unlikely]] {
      ctx.err = error{ error_code::unexpected_variant };
    }
  }
};

template <typename T>
  requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
struct from_dbus_binary<T> {
  static constexpr auto N = glz::reflection_count<T>;

  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    // A struct must start on an 8-byte boundary regardless of the type of the struct fields.
    skip_padding<std::uint64_t>(ctx, begin, it, end);
    if (ctx.err) [[unlikely]] {
      return;
    }
    decltype(auto) t = glz::detail::reflection_tuple<T>(value);
    glz::for_each<N>([&](auto I) {
      using Element = glz::detail::glaze_tuple_element<I, N, T>;
      static constexpr size_t member_index = Element::member_index;
      using val_t = std::remove_cvref_t<typename Element::type>;
      if constexpr (std::same_as<val_t, glz::hidden> || std::same_as<val_t, glz::skip>) {
        return;
      } else {
        decltype(auto) member = [&]() -> decltype(auto) {
          if constexpr (glz::detail::reflectable<T>) {
            return std::get<I>(t);
          } else {
            return glz::get<member_index>(glz::get<I>(glz::meta_v<std::decay_t<T>>));
          }
        }();
        auto& member_ref = glz::detail::get_member(value, member);
        from_dbus_binary<std::decay_t<decltype(member_ref)>>::template op<Opts>(member_ref, ctx, begin, it, end);
      }
    });
    if constexpr (is_header<T>) {
      // todo test
      // The length of the header must be a multiple of 8, allowing the body to begin on an 8-byte boundary when storing the
      // entire message in a single buffer. If the header does not naturally end on an 8-byte boundary up to 7 bytes of
      // nul-initialized alignment padding must be added.
      skip_padding<std::uint64_t>(ctx, begin, it, end);
    }
  }
};


template <glz::detail::glaze_value_t T>
struct from_dbus_binary<T> {
  template <options Opts>
  static constexpr void op(auto&& value, is_context auto&& ctx, auto&& begin, auto&& it, auto&& end) noexcept {
    using V = decltype(glz::detail::get_member(std::declval<T>(), glz::meta_wrapper_v<T>));
    from_dbus_binary<std::decay_t<V>>::template op<Opts>(glz::detail::get_member(value, glz::meta_wrapper_v<T>), ctx, begin, it, end);
  }
};


}  // namespace detail

template <typename T, typename Buffer>
  requires std::is_lvalue_reference_v<T>
[[nodiscard]] constexpr auto read_dbus_binary(T&& value, Buffer&& buffer, auto&& it) noexcept -> error {
  context ctx{};
  // todo begin of buffer should be const, cbegin
  detail::from_dbus_binary<std::decay_t<T>>::template op<{}>(value, ctx, std::begin(buffer), it,
                                                             std::cend(buffer));
  return ctx.err;
}

template <typename T, typename Buffer>
  requires std::is_lvalue_reference_v<T>
[[nodiscard]] constexpr auto read_dbus_binary(T&& value, Buffer&& buffer) noexcept -> error {
  auto it{ std::begin(buffer) };
  return read_dbus_binary(std::forward<T>(value), std::forward<Buffer>(buffer), std::move(it));
}

template <typename T, class Buffer>
[[nodiscard]] constexpr auto read_dbus_binary(Buffer&& buffer) noexcept -> glz::expected<T, error> {
  T value{};
  auto err = read_dbus_binary(value, buffer);
  if (err) [[unlikely]] {
    return glz::unexpected(err);
  }
  return value;
}

}  // namespace adbus::protocol
