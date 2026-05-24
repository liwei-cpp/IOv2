#pragma once
#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
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
constexpr static bool is_facet_create_rule = false;

template <typename... T>
constexpr static bool is_facet_create_rule<facet_create_rule<T...>> = (sizeof...(T) != 0);

template <typename T>
constexpr static bool is_facet_create_pack = false;

template <typename ...T>
constexpr static bool is_facet_create_pack<facet_create_pack<T...>> = (sizeof...(T) != 0);

template <typename T>
constexpr static size_t facet_create_pack_size = 0;

template <typename... T>
constexpr static size_t facet_create_pack_size<facet_create_pack<T...>> = sizeof...(T);

template <typename T> struct facet_create_pack_head;

template <typename H, typename... T>
struct facet_create_pack_head<facet_create_pack<H, T...>>
{
    using type = H;
};

template <typename T> struct facet_create_pack_tail;

template <typename H, typename... T>
struct facet_create_pack_tail<facet_create_pack<H, T...>>
{
    using type = facet_create_pack<T...>;
};

template <typename T>
std::shared_ptr<T> avail_ptr(std::shared_ptr<T> obj)
{
    if (!obj) throw std::runtime_error("shared_ptr is empty");
    return obj;
}
}
