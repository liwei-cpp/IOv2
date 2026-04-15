#pragma once
#include <memory>
#include <type_traits>


namespace IOv2
{
    template <typename... T>
    constexpr bool DependencyFalse = false;
    template <auto... T>
    constexpr bool DependencyFalseV = false;

    template <typename T>
    inline constexpr bool is_small_type_v = sizeof(T) <= 2 * sizeof(void*);

    template<typename, typename>
    struct shared_ptr_to_impl : std::false_type {};

    template<typename T, typename U>
    struct shared_ptr_to_impl<std::shared_ptr<U>, T> : std::bool_constant<std::is_convertible_v<U*, const T*>> {};

    template<typename P, typename T>
    concept shared_ptr_to = shared_ptr_to_impl<std::decay_t<P>, T>::value;
}