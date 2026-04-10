#pragma once
#include <langinfo.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>

namespace IOv2
{
template <typename CharT> class collate;
template <typename CharT> class collate_conf;

template <typename CharT>
class collate_conf : public ft_basic<collate<CharT>>
{
public:
    collate_conf(const std::string& name)
        : ft_basic<collate<CharT>>()
        , m_inter_locale(name.c_str()) {}

public:
    virtual std::strong_ordering compare(const CharT* low1, const CharT* high1,
                                         const CharT* low2, const CharT* high2) const
    {
        std::vector<CharT> buf1; bool extra_eos1 = false;
        std::vector<CharT> buf2; bool extra_eos2 = false;
        
        clocale_user guard(m_inter_locale.c_locale);
        
        while ((low1 != high1) && (low2 != high2))
        {
            const CharT* cl1 = low1;
            auto ch1 = std::find(low1, high1, static_cast<CharT>(0));
            if (ch1 == high1)
            {
                auto data_len = high1 - low1;
                buf1.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low1, high1, buf1.data());
                buf1[data_len] = static_cast<CharT>(0);
                extra_eos1 = true;
                cl1 = buf1.data();
                ch1 = buf1.data() + data_len + 1;
                low1 = high1;
            }
            else low1 = ch1 + 1;
            
            const CharT* cl2 = low2;
            auto ch2 = std::find(low2, high2, static_cast<CharT>(0));
            if (ch2 == high2)
            {
                auto data_len = high2 - low2;
                buf2.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low2, high2, buf2.data());
                buf2[data_len] = static_cast<CharT>(0);
                extra_eos2 = true;
                cl2 = buf2.data();
                ch2 = buf2.data() + data_len + 1;
                low2 = high2;
            }
            else low2 = ch2 + 1;
            
            int c_res = 0;
            if constexpr (std::is_same_v<CharT, char>)
                c_res = std::strcoll(cl1, cl2);
            else if constexpr (std::is_same_v<CharT, wchar_t>)
                c_res = std::wcscoll(cl1, cl2);
            else if constexpr ((sizeof(char32_t) == sizeof(wchar_t)) && 
                               (static_cast<wchar_t>(U'李') == L'李') &&
                               (static_cast<char32_t>(L'伟') == U'伟'))
            {
                if constexpr (std::is_same_v<CharT, char32_t>)
                    c_res = std::wcscoll(reinterpret_cast<const wchar_t*>(cl1),
                                         reinterpret_cast<const wchar_t*>(cl2));
                else if constexpr (std::is_same_v<CharT, char8_t>)
                {
                    auto ws1 = to_u32string(cl1);
                    auto ws2 = to_u32string(cl2);
                    c_res = std::wcscoll(reinterpret_cast<const wchar_t*>(ws1.c_str()),
                                         reinterpret_cast<const wchar_t*>(ws2.c_str()));
                }
            }
            else
                static_assert(DependencyFalse<CharT>, "collate_conf::compare is not implemented.");

            if (c_res < 0) return std::strong_ordering::less;
            if (c_res > 0) return std::strong_ordering::greater;
        }
        
        if (low1 != high1) return std::strong_ordering::greater;
        if (low2 != high2) return std::strong_ordering::less;
        if (extra_eos1 && !extra_eos2) return std::strong_ordering::less;
        if (!extra_eos1 && extra_eos2) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    virtual size_t transform_length(const CharT* low, const CharT* high) const
    {
        size_t res = 0;
        std::vector<CharT> buf;
        
        clocale_user guard(m_inter_locale.c_locale);
        while (low != high)
        {
            const CharT* cur = low;
            if (auto next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                auto data_len = high - low;
                buf.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low, high, buf.data());
                buf[data_len] = static_cast<CharT>(0);
                cur = buf.data();
                low = high;
            }
            else
            {
                low = next + 1;
                ++res;  // for the terminal character
            }

            if constexpr (std::is_same_v<CharT, char>)
                res += strxfrm(nullptr, cur, 0);
            else if constexpr (std::is_same_v<CharT, wchar_t>)
                res += wcsxfrm(nullptr, cur, 0);
            else if constexpr ((sizeof(char32_t) == sizeof(wchar_t)) && 
                               (static_cast<wchar_t>(U'李') == L'李') &&
                               (static_cast<char32_t>(L'伟') == U'伟'))
            {
                if constexpr (std::is_same_v<CharT, char32_t>)
                    res += wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(cur), 0);
                else if constexpr (std::is_same_v<CharT, char8_t>)
                {
                    auto ws = to_u32string(cur);
                    res += wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(ws.c_str()), 0) * 6;
                }
            }
            else
                static_assert(DependencyFalse<CharT>, "collate_sale::transform_length is not implemented.");
        }

        return res;
    };
    
    virtual size_t transform(const CharT* low, const CharT* high, CharT* dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        std::vector<CharT> buf;
        bool extra_eos = false;
        
        clocale_user guard(m_inter_locale.c_locale);
        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            const CharT* cur = low;
            if (auto next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                auto data_len = high - low;
                buf.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low, high, buf.data());
                buf[data_len] = static_cast<CharT>(0);
                cur = buf.data();
                low = high;
                extra_eos = true;
            }
            else
                low = next + 1;

            if constexpr (std::is_same_v<CharT, char8_t> && 
                          (sizeof(char32_t) == sizeof(wchar_t)) && 
                          (static_cast<wchar_t>(U'李') == L'李') &&
                          (static_cast<char32_t>(L'伟') == U'伟'))
            {
                auto ws = to_u32string(cur);
                auto trans_len = wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(ws.c_str()), 0);
                std::vector<char32_t> buf2;
                buf2.resize(trans_len + 1);
                auto cur_trans = wcsxfrm(reinterpret_cast<wchar_t*>(buf2.data()), 
                                         reinterpret_cast<const wchar_t*>(ws.c_str()),
                                         static_cast<unsigned>(-1));
                buf2[cur_trans] = 0;
                
                auto char8s = to_u8string(buf2.data());
                if (mx_len == 0)
                {
                    dest = std::copy(char8s.data(), char8s.data() + char8s.size(), dest);
                    trans_count += char8s.size();
                }
                else
                {
                    cur_trans = std::min(char8s.size(), mx_len - trans_count);
                    dest = std::copy(char8s.data(), char8s.data() + cur_trans, dest);
                    trans_count += cur_trans;
                }
            }
            else if (mx_len == 0)
            {
                size_t cur_trans = 0;
                if constexpr (std::is_same_v<CharT, char>)
                    cur_trans = strxfrm(dest, cur, static_cast<unsigned>(-1));
                else if constexpr (std::is_same_v<CharT, wchar_t>)
                    cur_trans = wcsxfrm(dest, cur, static_cast<unsigned>(-1));
                else if constexpr ((std::is_same_v<CharT, char32_t> && 
                                   (sizeof(char32_t) == sizeof(wchar_t)) && 
                                   (static_cast<wchar_t>(U'李') == L'李') &&
                                   (static_cast<char32_t>(L'伟') == U'伟')))
                    cur_trans = wcsxfrm(reinterpret_cast<wchar_t*>(dest), reinterpret_cast<const wchar_t*>(cur), static_cast<unsigned>(-1));
                else
                    static_assert(DependencyFalse<CharT>, "collate_sale::transform is not implemented.");
                    
                dest += cur_trans;
                trans_count += cur_trans;
            }
            else
            {
                size_t trans_len = 0;
                if constexpr (std::is_same_v<CharT, char>)
                    trans_len = strxfrm(nullptr, cur, 0);
                else if constexpr (std::is_same_v<CharT, wchar_t>)
                    trans_len = wcsxfrm(nullptr, cur, 0);
                else if constexpr ((std::is_same_v<CharT, char32_t> && 
                                   (sizeof(char32_t) == sizeof(wchar_t)) && 
                                   (static_cast<wchar_t>(U'李') == L'李') &&
                                   (static_cast<char32_t>(L'伟') == U'伟')))
                    trans_len = wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(cur), 0);
                else
                    static_assert(DependencyFalse<CharT>, "collate_sale::transform is not implemented.");
                    
                std::vector<CharT> buf2;
                buf2.resize(trans_len + 1);

                size_t cur_trans = 0;
                if constexpr (std::is_same_v<CharT, char>)
                    cur_trans = strxfrm(buf2.data(), cur, static_cast<unsigned>(-1));
                else if constexpr (std::is_same_v<CharT, wchar_t>)
                    cur_trans = wcsxfrm(buf2.data(), cur, static_cast<unsigned>(-1));
                else if constexpr ((std::is_same_v<CharT, char32_t> && 
                                   (sizeof(char32_t) == sizeof(wchar_t)) && 
                                   (static_cast<wchar_t>(U'李') == L'李') &&
                                   (static_cast<char32_t>(L'伟') == U'伟')))
                    cur_trans = wcsxfrm(reinterpret_cast<wchar_t*>(buf2.data()), reinterpret_cast<const wchar_t*>(cur), static_cast<unsigned>(-1));
                else
                    static_assert(DependencyFalse<CharT>, "collate_sale::transform is not implemented.");
                    
                cur_trans = std::min(cur_trans, mx_len - trans_count);
                dest = std::copy(buf2.data(), buf2.data() + cur_trans, dest);
                trans_count += cur_trans;
            }

            if ((!extra_eos) && ((mx_len == 0) || (trans_count < mx_len)))
            {
                *dest++ = '\0';
                ++trans_count;
            }
        }
        return trans_count;
    }
private:
    clocale_wrapper   m_inter_locale;
};
}