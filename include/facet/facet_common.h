#pragma once
#include <concepts>
#include <cstddef>
#include <cwchar>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace IOv2
{
struct abs_ft
{
    explicit abs_ft(size_t id)
        : m_id(id) {}

    abs_ft(const abs_ft&) = delete;
    abs_ft& operator=(const abs_ft&) = delete;

    virtual ~abs_ft() = default;

    size_t id() const noexcept { return m_id; }

private:
    const size_t m_id;
};

template <template<typename> class TFacet>
class base_ft : public abs_ft
{
public:
    using abs_ft::abs_ft;
};

template <typename TFacet> class ft_basic;

template <template<typename> class TFacet, typename CharT>
class ft_basic<TFacet<CharT>> : public base_ft<TFacet>
{
    static_assert(sizeof(void*) <= sizeof(size_t));
public:
    using char_type = CharT;
public:
    template <typename... T>
        requires std::constructible_from<base_ft<TFacet>, size_t, T...>
    ft_basic(T&&... args)
        : base_ft<TFacet>(id(), std::forward<T>(args)...) {}

    static size_t id() noexcept { return reinterpret_cast<size_t>(&s_id); }
private:
    inline static const void* s_id = nullptr;
};

template <typename...>
struct facet_create_rule;

template <typename...>
struct facet_create_pack;

template <typename T>
constexpr static bool is_nonempty_facet_create_rule = false;

template <typename... T>
constexpr static bool is_nonempty_facet_create_rule<facet_create_rule<T...>> = (sizeof...(T) != 0);

template <typename T>
constexpr static bool is_nonempty_facet_create_pack = false;

template <typename... T>
constexpr static bool is_nonempty_facet_create_pack<facet_create_pack<T...>> = (sizeof...(T) != 0);

template <typename T>
constexpr static size_t facet_create_pack_size = 0;

template <typename... T>
constexpr static size_t facet_create_pack_size<facet_create_pack<T...>> = sizeof...(T);

template <typename T>
    requires (facet_create_pack_size<T> > 0)
struct facet_create_pack_head;

template <typename H, typename... T>
struct facet_create_pack_head<facet_create_pack<H, T...>>
{
    using type = H;
};

template <typename T>
    requires (facet_create_pack_size<T> > 0)
struct facet_create_pack_tail;

template <typename H, typename... T>
struct facet_create_pack_tail<facet_create_pack<H, T...>>
{
    using type = facet_create_pack<T...>;
};

// Returns true if `c` has no wchar_t representation. Used to guard the
// C `*_l` wide-character functions (iswctype_l, towupper_l, towlower_l,
// wctob, ...) at any facet's API boundary, since the C standard says
// their behaviour is implementation-defined for inputs that are not
// representable as a wchar_t.
//
// Only non-trivial for CharT == char32_t on platforms where char32_t
// covers a strictly wider range than wchar_t (e.g. Linux: signed
// 32-bit wchar_t with WCHAR_MAX = 0x7FFFFFFF, vs unsigned 32-bit
// char32_t). For every other character type (wchar_t, char, char8_t,
// char16_t) the input is always representable as wchar_t on every
// supported platform, and the function is statically a no-op.
//
// CharT is constrained to the five standard C++ character types so
// that misuse with non-character integral or floating types (where
// the wchar_t-range question is meaningless) fails at compile time
// instead of silently returning false.
template <typename CharT>
    requires std::is_same_v<CharT, char>     ||
             std::is_same_v<CharT, wchar_t>  ||
             std::is_same_v<CharT, char8_t>  ||
             std::is_same_v<CharT, char16_t> ||
             std::is_same_v<CharT, char32_t>
constexpr bool out_of_wchar_range(CharT c) noexcept
{
    if constexpr (std::is_same_v<CharT, char32_t>)
        return c > static_cast<char32_t>(WCHAR_MAX);
    else
        return false;
}

template <typename T>
std::shared_ptr<T> avail_ptr(std::shared_ptr<T> obj)
{
    if (!obj) throw std::runtime_error("shared_ptr is empty");
    return obj;
}
}
