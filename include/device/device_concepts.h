#pragma once
#include <cstddef>
#include <concepts>
#include <utility>

namespace IOv2
{
    namespace dev_cpt
    {
        template <typename T>
        concept support_positioning = requires(T a, const T& c)
            {
                { c.dtell() } -> std::same_as<size_t>;
                { a.dseek(std::declval<size_t>()) } -> std::same_as<void>;
                { a.drseek(std::declval<size_t>()) } -> std::same_as<void>;
            };
        
        template <typename T>
        concept support_put = requires(T a)
            {
                { a.dput(std::declval<const typename T::char_type*>(), std::declval<size_t>()) } -> std::same_as<void>;
                { a.dflush() } -> std::same_as<void>;
            };
        
        template <typename T>
        concept support_get = requires(T a)
            {
                {a.dget(std::declval<typename T::char_type*>(), std::declval<size_t>())} -> std::same_as<size_t>;
            };
    }

    template <typename T>
    concept io_device =
        requires{ typename T::char_type; } &&
        (dev_cpt::support_put<T> || dev_cpt::support_get<T>) &&
        requires(T a)
        {
            { a.deos() } -> std::same_as<bool>;
        };
}