#pragma once

#include <memory>
#include <mutex>

#include <common/metafunctions.h>
#include <common/lru_cache.h>
#include <facet/ctype_details.h>

namespace IOv2
{
template <typename CharT>
class ctype;

template <typename CharT>
    requires (sizeof(CharT) == 1)
class ctype<CharT>
{
    // Note: we have to use unsigned char here to avoid negative value
    constexpr static unsigned s_len = std::numeric_limits<unsigned char>::max() + 1;

public:
    using create_rules = facet_create_rule<ctype_conf<CharT>>;

    using char_type = CharT;
    using mask = typename ctype_conf<CharT>::mask;

    template <shared_ptr_to<ctype_conf<CharT>> TConfPtr>
    ctype(TConfPtr p_obj)
    {
        avail_ptr(p_obj);
        for (unsigned i = 0; i < s_len; ++i)
        {
            m_table[i] = p_obj->is((CharT)i);
            m_toupper[i] = p_obj->toupper((CharT)i);
            m_tolower[i] = p_obj->tolower((CharT)i);
            m_widen[i] = p_obj->widen((CharT)i);
            m_narrow[i] = p_obj->narrow((CharT)i);
        }
    }

    ctype(const ctype&) = delete;
    ctype& operator=(const ctype&) = delete;

public:
    mask is(CharT c) const
    {
        return m_table[static_cast<unsigned char>(c)];
    }
    
    bool is_any(mask m, CharT c) const
    {
        return is(c) & m;
    }

    template <typename InIt, typename OutIt>
    OutIt is_seq(InIt low, InIt high, OutIt vec) const
    {
        while (low < high)
            *vec++ = is(*low++);
        return vec;
    }

    template <typename InIt>
    InIt scan_is_any(mask m, InIt beg, InIt end) const
    {
        while ((beg < end) && (!(is(*beg) & m)))
            ++beg;
        return beg;
    }

    template <typename InIt>
    InIt scan_not_any(mask m, InIt beg, InIt end) const
    {
        while ((beg < end) && (is(*beg) & m))
            ++beg;
        return beg;
    }

    CharT toupper(CharT c) const
    {
        return m_toupper[static_cast<unsigned char>(c)];
    }
    
    template <typename InIt, typename OutIt>
    OutIt toupper_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg < end)
            *dst++ = toupper(*beg++);
        return dst;
    }

    CharT tolower(CharT c) const
    {
        return m_tolower[static_cast<unsigned char>(c)];
    }

    template <typename InIt, typename OutIt>
    OutIt tolower_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg < end)
            *dst++ = tolower(*beg++);
        return dst;
    }

    CharT widen(char c) const
    {
        return m_widen[static_cast<unsigned char>(c)];
    }
    
    template <typename InIt, typename OutIt>
    OutIt widen_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg < end)
            *dst++ = widen(*beg++);
        return dst;
    }

    std::optional<char> narrow(CharT c) const
    {
        return m_narrow[static_cast<unsigned char>(c)];
    }

    char narrow(CharT c, char def) const
    {
        auto res = narrow(c);
        return res ? *res : def;
    }

    template <typename InIt, typename OutIt>
    OutIt narrow_seq(InIt beg, InIt end, char dflt, OutIt dst) const
    {
        while (beg < end)
            *dst++ = narrow(*beg++, dflt);
        return dst;
    }

private:
    mask  m_table[s_len];
    CharT m_toupper[s_len];
    CharT m_tolower[s_len];
    CharT m_widen[s_len];
    std::optional<char> m_narrow[s_len];
};

template <typename CharT>
    requires (sizeof(CharT) > 1)
class ctype<CharT>
{
    constexpr static unsigned s_len = std::numeric_limits<unsigned char>::max() + 1;

public:
    using create_rules = facet_create_rule<ctype_conf<CharT>>;

    using char_type = CharT;
    using mask = typename ctype_conf<CharT>::mask;

    ctype(std::shared_ptr<const ctype_conf<CharT>> p_obj)
        : m_obj(avail_ptr(p_obj))
    {
        for (unsigned i = 0; i < s_len; ++i)
        {
            m_toupper[i] = m_obj->toupper((CharT)i);
            m_tolower[i] = m_obj->tolower((CharT)i);
            m_widen[i] = m_obj->widen((CharT)i);
            m_narrow[i] = m_obj->narrow((CharT)i);
            m_table[i] = m_obj->is((CharT)i);
        }
    }

    ctype(const ctype&) = delete;
    ctype& operator=(const ctype&) = delete;

public:
    mask is(CharT c) const
    {
        return do_is(c);
    }

    bool is_any(mask m, CharT c) const
    {
        return is(c) & m;
    }

    template <typename InIt, typename OutIt>
    OutIt is_seq(InIt low, InIt high, OutIt vec) const
    {
        while (low < high)
            *vec++ = do_is(*low++);
        return vec;
    }

    template <typename InIt>
    InIt scan_is_any(mask m, InIt beg, InIt end) const
    {
        while ((beg < end) && (!(do_is(*beg) & m)))
            ++beg;
        return beg;
    }

    template <typename InIt>
    InIt scan_not_any(mask m, InIt beg, InIt end) const
    {
        while ((beg < end) && (do_is(*beg) & m))
            ++beg;
        return beg;
    }

    CharT toupper(CharT c) const
    {
        return do_toupper(c);
    }

    template <typename InIt, typename OutIt>
    OutIt toupper_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg < end)
            *dst++ = do_toupper(*beg++);
        return dst;
    }

    CharT tolower(CharT c) const
    {
        return do_tolower(c);
    }

    template <typename InIt, typename OutIt>
    OutIt tolower_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg < end)
            *dst++ = do_tolower(*beg++);
        return dst;
    }

    CharT widen(char c) const
    {
        return m_widen[static_cast<unsigned char>(c)];
    }

    template <typename InIt, typename OutIt>
    OutIt widen_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg < end)
            *dst++ = widen(*beg++);
        return dst;
    }

    std::optional<char> narrow(CharT c) const
    {
        return do_narrow(c);
    }

    char narrow(CharT c, char def) const
    {
        auto res = narrow(c);
        return res ? *res : def;
    }

    template <typename InIt, typename OutIt>
    OutIt narrow_seq(InIt beg, InIt end, char dflt, OutIt dst) const
    {
        while (beg < end)
        {
            auto res = do_narrow(*beg++);
            *dst++ = res ? *res : dflt;
        }
        return dst;
    }
    
private:
    mask do_is(CharT c) const
    {
        if (static_cast<unsigned>(c) < s_len)
            return m_table[static_cast<unsigned>(c)];

        std::lock_guard g(m_lru_mutex);
        auto res = m_table_cache.get(c);
        if (!res)
        {
            auto v = m_obj->is(c);
            m_table_cache.try_put(c, v);
            return v;
        }
        return *res;
    }
    
    CharT do_toupper(CharT c) const
    {
        if (static_cast<unsigned>(c) < s_len)
            return m_toupper[static_cast<unsigned>(c)];

        std::lock_guard g(m_lru_mutex);
        auto res = m_upper_cache.get(c);
        if (!res)
        {
            auto v = m_obj->toupper(c);
            m_upper_cache.try_put(c, v);
            return v;
        }
        return *res;
    }
    
    CharT do_tolower(CharT c) const
    {
        if (static_cast<unsigned>(c) < s_len)
            return m_tolower[static_cast<unsigned>(c)];

        std::lock_guard g(m_lru_mutex);
        auto res = m_lower_cache.get(c);
        if (!res)
        {
            auto v = m_obj->tolower(c);
            m_lower_cache.try_put(c, v);
            return v;
        }
        return *res;
    }
    
    std::optional<char> do_narrow(CharT c) const
    {
        if (static_cast<unsigned>(c) < s_len)
            return m_narrow[static_cast<unsigned>(c)];

        std::lock_guard g(m_lru_mutex);
        auto res = m_narrow_cache.get(c);
        if (!res)
        {
            auto v = m_obj->narrow(c);
            m_narrow_cache.try_put(c, v);
            return v;
        }
        return *res;
    }
private:
    std::shared_ptr<const ctype_conf<CharT>> m_obj;

    CharT m_toupper[s_len];
    CharT m_tolower[s_len];
    CharT m_widen[s_len];
    std::optional<char> m_narrow[s_len];
    mask  m_table[s_len];

    mutable std::mutex m_lru_mutex;
    mutable lru_cache<CharT, mask, 1024>  m_table_cache;
    mutable lru_cache<CharT, CharT, 1024> m_upper_cache;
    mutable lru_cache<CharT, CharT, 1024> m_lower_cache;
    mutable lru_cache<CharT, std::optional<char>, 1024> m_narrow_cache;
};

template<typename TConfPtr>
ctype(TConfPtr) -> ctype<typename TConfPtr::element_type::char_type>;
}