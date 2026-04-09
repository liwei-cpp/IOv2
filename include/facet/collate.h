#pragma once
#include <memory>

#include <common/metafunctions.h>
#include <facet/collate_details.h>

namespace IOv2
{
template <typename CharT>
class collate
{
public:
    using create_rules = facet_create_rule<collate_conf<CharT>>;

    using char_type = CharT;

    template <shared_ptr_to<collate_conf<CharT>> TConfPtr>
    collate(TConfPtr p_obj)
        : m_obj(avail_ptr(p_obj)) {}

public:
    std::strong_ordering compare(const CharT* low1, const CharT* high1, const CharT* low2, const CharT* high2) const
    {
        return m_obj->compare(low1, high1, low2, high2);
    }
    
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    std::strong_ordering compare(const CharT* low1, const CharT* high1, TIter low2, TIter high2) const
    {
        std::vector<CharT> buf1;
        std::vector<CharT> buf2; buf2.reserve(64);
        
        while ((low1 != high1) && (low2 != high2))
        {
            std::strong_ordering c_res = std::strong_ordering::equal;
            low2 = data_to_vec(low2, high2, buf2);

            const CharT* cl1 = low1;
            auto ch1 = std::find(low1, high1, static_cast<CharT>(0));
            if (ch1 == high1)
            {
                c_res = m_obj->compare(cl1, ch1, buf2.data(), buf2.data() + buf2.size());
                low1 = high1;
            }
            else
            {
                low1 = ch1 + 1;
                c_res = m_obj->compare(cl1, low1, buf2.data(), buf2.data() + buf2.size());
            }

            if (c_res != std::strong_ordering::equal)
                return c_res;
        }
        
        if (low1 != high1) return std::strong_ordering::greater;
        if (low2 != high2) return std::strong_ordering::less;
        return std::strong_ordering::equal;
    }
    
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    std::strong_ordering compare(TIter low1, TIter high1, const CharT* low2, const CharT* high2) const
    {
        auto res = this->compare(low2, high2, low1, high1);
        if (res == std::strong_ordering::greater) return std::strong_ordering::less;
        if (res == std::strong_ordering::less) return std::strong_ordering::greater;
        return res;
    }
    
    template <typename TIter1, typename TIter2>
        requires (!(std::is_convertible_v<TIter1, const CharT*> || std::is_convertible_v<TIter2, const CharT*>))
    std::strong_ordering compare(TIter1 low1, TIter1 high1, TIter2 low2, TIter2 high2) const
    {
        std::vector<CharT> buf1; buf1.reserve(64);
        std::vector<CharT> buf2; buf2.reserve(64);
        
        while ((low1 != high1) && (low2 != high2))
        {
            low1 = data_to_vec(low1, high1, buf1);
            low2 = data_to_vec(low2, high2, buf2);
            
            auto c_res = m_obj->compare(buf1.data(), buf1.data() + buf1.size(), buf2.data(), buf2.data() + buf2.size());
            
            if (c_res != std::strong_ordering::equal)
                return c_res;
        }
        
        if (low1 != high1) return std::strong_ordering::greater;
        if (low2 != high2) return std::strong_ordering::less;
        return std::strong_ordering::equal;
    }
    
    size_t transform_length(const CharT* low, const CharT* high) const
    {
        return m_obj->transform_length(low, high);
    }
    
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    size_t transform_length(TIter low, TIter high) const
    {
        size_t res = 0;
        std::vector<CharT> buf; buf.reserve(64);
        
        while (low != high)
        {
            low = data_to_vec(low, high, buf);
            res += m_obj->transform_length(buf.data(), buf.data() + buf.size());
        }
        return res;
    }
    
    std::pair<CharT*, size_t> transform(const CharT* low, const CharT* high, CharT* dest, size_t mx_len = 0) const
    {
        auto s = m_obj->transform(low, high, dest, mx_len);
        return std::pair{dest + s, s};
    }
    
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, const CharT*>)
    std::pair <CharT*, size_t> transform(TIter low, TIter high, CharT* dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        std::vector<CharT> buf; buf.reserve(64);

        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            low = data_to_vec(low, high, buf);
            size_t cur_trans = 0;
            
            if (mx_len == 0)
                cur_trans = m_obj->transform(buf.data(), buf.data() + buf.size(), dest);
            else
                cur_trans = m_obj->transform(buf.data(), buf.data() + buf.size(), dest, mx_len - trans_count);
            trans_count += cur_trans;
            dest += cur_trans;
        }
        return std::pair(dest, trans_count);
    }
    
    template <typename TIter>
        requires (!std::is_convertible_v<TIter, CharT*>)
    std::pair<TIter, size_t> transform(const CharT* low, const CharT* high, TIter dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        std::vector<CharT> buf;
        std::vector<CharT> buf2;

        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            const CharT* cur = low;
            if (const CharT* next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                buf.resize(high - low);
                std::copy(low, high, buf.data());
                size_t cur_trans = transform_length(buf.data(), buf.data() + buf.size());
                buf2.resize(cur_trans);
                buf2.resize(m_obj->transform(buf.data(), buf.data() + buf.size(), buf2.data(), buf2.size()));
                low = high;
            }
            else
            {
                low = next + 1;
                size_t cur_trans = transform_length(cur, low);
                buf2.resize(cur_trans);
                buf2.resize(m_obj->transform(cur, low, buf2.data(), buf2.size()));
            }

            if (mx_len == 0)
            {
                dest = std::copy(buf2.begin(), buf2.end(), dest);
                trans_count += buf2.size();
            }
            else
            {
                auto dest_size = std::min(buf2.size(), mx_len - trans_count);
                dest = std::copy(buf2.begin(), buf2.begin() + dest_size, dest);
                trans_count += dest_size;
            }
        }
        return std::pair(dest, trans_count);
    }
    
    template <typename TIterIn, typename TIterOut>
        requires (!(std::is_convertible_v<TIterIn, const CharT*> || std::is_convertible_v<TIterOut, CharT*>))
    std::pair <TIterOut, size_t> transform(TIterIn low, TIterIn high, TIterOut dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        std::vector<CharT> buf; buf.reserve(64);
        std::vector<CharT> buf2;
        
        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            low = data_to_vec(low, high, buf);
            
            size_t cur_trans = m_obj->transform_length(buf.data(), buf.data() + buf.size());
            buf2.resize(cur_trans);
            buf2.resize(m_obj->transform(buf.data(), buf.data() + buf.size(), buf2.data(), buf2.size()));
            
            if (mx_len == 0)
            {
                dest = std::copy(buf2.begin(), buf2.end(), dest);
                trans_count += buf2.size();
            }
            else
            {
                auto dest_size = std::min(buf2.size(), mx_len - trans_count);
                dest = std::copy(buf2.begin(), buf2.begin() + dest_size, dest);
                trans_count += dest_size;
            }
        }

        return std::pair(dest, trans_count);
    }

private:
    template <typename TIter>
    static TIter data_to_vec(TIter low, TIter high, std::vector<CharT>& buf)
    {
        buf.clear();
        while (low != high)
        {
            buf.push_back(*low++);
            if (buf.back() == static_cast<CharT>(0))
                break;
        }
        return low;
    }

private:
    std::shared_ptr<const collate_conf<CharT>> m_obj;
};

template<typename TConfPtr>
collate(TConfPtr) -> collate<typename TConfPtr::element_type::char_type>;
}