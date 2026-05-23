#pragma once
#include <array>
#include <cstring>
#include <string>
#include <vector>
#include <common/clocale_wrapper.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>

namespace IOv2
{
template <typename CharT> class monetary;

template <>
class base_ft<monetary> : public abs_ft
{
public:
    using abs_ft::abs_ft;
    
    enum part { none, space, symbol, sign, value };
    using pattern = std::array<part, 4>;
    
protected:
    inline const static pattern s_default_pattern = {symbol, sign, none, value};
    static pattern s_construct_pattern(int8_t precedes, int8_t sp, int8_t posn)
    {
        pattern ret;

        // This insanely complicated routine attempts to construct a valid
        // pattern for use with monyepunct. A couple of invariants:

        // if (precedes) symbol -> value
        // else value -> symbol

        // if (sp) space
        // else none

        // none == never first
        // space never first or last
        switch (posn)
        {
        case 0:
        case 1:
            // 1 The sign precedes the value and symbol.
            ret[0] = sign;
            if (sp)
            {
                // Pattern starts with sign.
                if (precedes)
                {
                    ret[1] = symbol;
                    ret[3] = value;
                }
                else
                {
                    ret[1] = value;
                    ret[3] = symbol;
                }
                ret[2] = space;
            }
            else
            {
                // Pattern starts with sign and ends with none.
                if (precedes)
                {
                    ret[1] = symbol;
                    ret[2] = value;
                }
                else
                {
                    ret[1] = value;
                    ret[2] = symbol;
                }
                ret[3] = none;
            }
            break;
        case 2:
            // 2 The sign follows the value and symbol.
            if (sp)
            {
                // Pattern either ends with sign.
                if (precedes)
                {
                    ret[0] = symbol;
                    ret[2] = value;
                }
                else
                {
                    ret[0] = value;
                    ret[2] = symbol;
                }
                ret[1] = space;
                ret[3] = sign;
            }
            else
            {
                // Pattern ends with sign then none.
                if (precedes)
                {
                    ret[0] = symbol;
                    ret[1] = value;
                }
                else
                {
                    ret[0] = value;
                    ret[1] = symbol;
                }
                ret[2] = sign;
                ret[3] = none;
            }
            break;
        case 3:
            // 3 The sign immediately precedes the symbol.
            if (precedes)
            {
                ret[0] = sign;
                ret[1] = symbol;
                if (sp)
                {
                    ret[2] = space;
                    ret[3] = value;
                }
                else
                {
                    ret[2] = value;
                    ret[3] = none;
                }
            }
            else
            {
                ret[0] = value;
                if (sp)
                {
                    ret[1] = space;
                    ret[2] = sign;
                    ret[3] = symbol;
                }
                else
                {
                    ret[1] = sign;
                    ret[2] = symbol;
                    ret[3] = none;
                }
            }
            break;
        case 4:
            // 4 The sign immediately follows the symbol.
            if (precedes)
            {
                ret[0] = symbol;
                ret[1] = sign;
                if (sp)
                {
                    ret[2] = space;
                    ret[3] = value;
                }
                else
                {
                    ret[2] = value;
                    ret[3] = none;
                }
            }
            else
            {
                ret[0] = value;
                if (sp)
                {
                    ret[1] = space;
                    ret[2] = symbol;
                    ret[3] = sign;
                }
                else
                {
                    ret[1] = symbol;
                    ret[2] = sign;
                    ret[3] = none;
                }
            }
            break;
        default:
            ret = s_default_pattern;
        }
        return ret;
    }
};

template <typename CharT> class monetary_conf;

template <>
class monetary_conf<char> : public ft_basic<monetary<char>>
{
public:
    using char_type = char;

public:
    monetary_conf(const std::string& name)
        : ft_basic<monetary<char>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = '.';
            m_thousands_sep = ',';
            m_frac_digits_int = 0;
            m_frac_digits_nat = 0;
            m_negative_sign_int = "-";
            m_negative_sign_nat = "-";
            m_pos_format_nat = s_default_pattern;
            m_neg_format_nat = s_default_pattern;
            m_pos_format_int = s_default_pattern;
            m_neg_format_int = s_default_pattern;
        }
        else
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();

            m_decimal_point = FacetHelper::string_to_char_convert(lc->mon_decimal_point, name);
            m_thousands_sep = FacetHelper::string_to_char_convert(lc->mon_thousands_sep, name);
            if (m_decimal_point == '\0')
            {
                m_frac_digits_int = 0;
                m_frac_digits_nat = 0;
                m_decimal_point = '.';
            }
            else
            {
                m_frac_digits_nat = lc->frac_digits;
                m_frac_digits_int = lc->int_frac_digits;
            }

            if (m_thousands_sep == '\0') m_thousands_sep = ',';
            else
            {
                const char* cgroup = lc->mon_grouping;
                size_t len = strlen(cgroup);
                if (len != 0)
                {
                    // Copy raw POSIX grouping bytes, then normalise into the
                    // internal convention here at the POSIX boundary.
                    m_grouping.resize(len);
                    for (size_t i = 0; i < len; ++i)
                        m_grouping[i] = static_cast<uint8_t>(cgroup[i]);
                    FacetHelper::adjust_grouping(m_grouping);
                }
            }

            m_positive_sign_nat = lc->positive_sign;
            m_positive_sign_int = m_positive_sign_nat;

            if (!lc->n_sign_posn)
                m_negative_sign_nat = "()";
            else
                m_negative_sign_nat = lc->negative_sign;

            if (!lc->int_n_sign_posn)
                m_negative_sign_int = "()";
            else
                m_negative_sign_int = lc->negative_sign;

            m_curr_symbol_nat = lc->currency_symbol;
            m_curr_symbol_int = lc->int_curr_symbol;

            m_pos_format_nat = s_construct_pattern(lc->p_cs_precedes, lc->p_sep_by_space, lc->p_sign_posn);
            m_neg_format_nat = s_construct_pattern(lc->n_cs_precedes, lc->n_sep_by_space, lc->n_sign_posn);
            m_pos_format_int = s_construct_pattern(lc->int_p_cs_precedes, lc->int_p_sep_by_space, lc->int_p_sign_posn);
            m_neg_format_int = s_construct_pattern(lc->int_n_cs_precedes, lc->int_n_sep_by_space, lc->int_n_sign_posn);
        }
    }
    
public:
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }
    virtual const std::string& curr_symbol_int() const { return m_curr_symbol_int; }
    virtual const std::string& curr_symbol_nat() const { return m_curr_symbol_nat; }
    virtual const std::string& positive_sign_int() const { return m_positive_sign_int; }
    virtual const std::string& positive_sign_nat() const { return m_positive_sign_nat; }
    virtual const std::string& negative_sign_int() const { return m_negative_sign_int; }
    virtual const std::string& negative_sign_nat() const { return m_negative_sign_nat; }
    virtual const pattern& pos_format_int() const { return m_pos_format_int; }
    virtual const pattern& pos_format_nat() const { return m_pos_format_nat; }
    virtual const pattern& neg_format_int() const { return m_neg_format_int; }
    virtual const pattern& neg_format_nat() const { return m_neg_format_nat; }
    virtual int frac_digits_int() const { return m_frac_digits_int; }
    virtual int frac_digits_nat() const { return m_frac_digits_nat; }
    virtual char decimal_point() const { return m_decimal_point; }
    virtual char thousands_sep() const { return m_thousands_sep; }
   
private:
    std::vector<uint8_t>    m_grouping;
    std::string             m_curr_symbol_int;
    std::string             m_curr_symbol_nat;
    std::string             m_positive_sign_int;
    std::string             m_positive_sign_nat;
    std::string             m_negative_sign_int;
    std::string             m_negative_sign_nat;
    pattern                 m_pos_format_int;
    pattern                 m_pos_format_nat;
    pattern                 m_neg_format_int;
    pattern                 m_neg_format_nat;
    int                     m_frac_digits_int;
    int                     m_frac_digits_nat;
    char                    m_decimal_point;
    char                    m_thousands_sep;
};

template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> || 
                (std::is_same_v<CharT, char32_t> && 
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class monetary_conf<CharT> : public ft_basic<monetary<CharT>>
{
public:
    using char_type = CharT;

public:
    monetary_conf(const std::string& name)
        : ft_basic<monetary<CharT>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            if constexpr (std::is_same_v<CharT, wchar_t>)
            {
                m_decimal_point = L'.';
                m_thousands_sep = L',';
                m_negative_sign_int = L"-";
                m_negative_sign_nat = L"-";
            }
            else
            {
                m_decimal_point = U'.';
                m_thousands_sep = U',';
                m_negative_sign_int = U"-";
                m_negative_sign_nat = U"-";
            }
            m_frac_digits_int = 0;
            m_frac_digits_nat = 0;
            m_pos_format_nat = base_ft<monetary>::s_default_pattern;
            m_neg_format_nat = base_ft<monetary>::s_default_pattern;
            m_pos_format_int = base_ft<monetary>::s_default_pattern;
            m_neg_format_int = base_ft<monetary>::s_default_pattern;
        }
        else
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();

            auto to_charT = [&](const char* narrow) -> CharT {
                if (!narrow || !narrow[0]) return static_cast<CharT>('\0');
                if constexpr (std::is_same_v<CharT, wchar_t>)
                {
                    auto wide = detail::to_wstring(narrow, name);
                    return wide.empty() ? static_cast<CharT>('\0') : wide[0];
                }
                else
                {
                    auto wide = detail::to_u32string(narrow, name);
                    return wide.empty() ? static_cast<CharT>('\0') : wide[0];
                }
            };

            m_decimal_point = to_charT(lc->mon_decimal_point);
            m_thousands_sep = to_charT(lc->mon_thousands_sep);
            
            if (static_cast<int>(m_decimal_point) == 0)
            {
                m_frac_digits_int = 0;
                m_frac_digits_nat = 0;
                if constexpr (std::is_same_v<CharT, wchar_t>) m_decimal_point = L'.';
                else m_decimal_point = U'.';
            }
            else
            {
                m_frac_digits_nat = lc->frac_digits;
                m_frac_digits_int = lc->int_frac_digits;
            }

            if (static_cast<int>(m_thousands_sep) == 0)
            {
                if constexpr (std::is_same_v<CharT, wchar_t>) m_thousands_sep = L',';
                else m_thousands_sep = U',';
            }
            else
            {
                const char* cgroup = lc->mon_grouping;
                size_t len = strlen(cgroup);
                if (len != 0)
                {
                    // Copy raw POSIX grouping bytes, then normalise into the
                    // internal convention here at the POSIX boundary.
                    m_grouping.resize(len);
                    for (size_t i = 0; i < len; ++i)
                        m_grouping[i] = static_cast<uint8_t>(cgroup[i]);
                    FacetHelper::adjust_grouping(m_grouping);
                }
            }

            {
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    m_positive_sign_nat = detail::to_wstring(lc->positive_sign, name);
                else
                    m_positive_sign_nat = detail::to_u32string(lc->positive_sign, name);
            }

            if (!lc->n_sign_posn)
            {
                if constexpr (std::is_same_v<CharT, wchar_t>) m_negative_sign_nat = L"()";
                else m_negative_sign_nat = U"()";
            }
            else
            {
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    m_negative_sign_nat = detail::to_wstring(lc->negative_sign, name);
                else
                    m_negative_sign_nat = detail::to_u32string(lc->negative_sign, name);
            }

            if (!lc->int_n_sign_posn)
            {
                if constexpr (std::is_same_v<CharT, wchar_t>) m_negative_sign_int = L"()";
                else m_negative_sign_int = U"()";
            }
            else
            {
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    m_negative_sign_int = detail::to_wstring(lc->negative_sign, name);
                else
                    m_negative_sign_int = detail::to_u32string(lc->negative_sign, name);
            }

            {
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    m_curr_symbol_nat = detail::to_wstring(lc->currency_symbol, name);
                else
                    m_curr_symbol_nat = detail::to_u32string(lc->currency_symbol, name);
            }
            {
                if constexpr (std::is_same_v<CharT, wchar_t>)
                    m_curr_symbol_int = detail::to_wstring(lc->int_curr_symbol, name);
                else
                    m_curr_symbol_int = detail::to_u32string(lc->int_curr_symbol, name);
            }

            m_pos_format_nat = base_ft<monetary>::s_construct_pattern(lc->p_cs_precedes, lc->p_sep_by_space, lc->p_sign_posn);
            m_neg_format_nat = base_ft<monetary>::s_construct_pattern(lc->n_cs_precedes, lc->n_sep_by_space, lc->n_sign_posn);
            m_pos_format_int = base_ft<monetary>::s_construct_pattern(lc->int_p_cs_precedes, lc->int_p_sep_by_space, lc->int_p_sign_posn);
            m_neg_format_int = base_ft<monetary>::s_construct_pattern(lc->int_n_cs_precedes, lc->int_n_sep_by_space, lc->int_n_sign_posn);
        }
    }
    
public:
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }
    virtual const std::basic_string<CharT>& curr_symbol_int() const { return m_curr_symbol_int; }
    virtual const std::basic_string<CharT>& curr_symbol_nat() const { return m_curr_symbol_nat; }
    virtual const std::basic_string<CharT>& positive_sign_int() const { return m_positive_sign_int; }
    virtual const std::basic_string<CharT>& positive_sign_nat() const { return m_positive_sign_nat; }
    virtual const std::basic_string<CharT>& negative_sign_int() const { return m_negative_sign_int; }
    virtual const std::basic_string<CharT>& negative_sign_nat() const { return m_negative_sign_nat; }
    virtual const base_ft<monetary>::pattern& pos_format_int() const { return m_pos_format_int; }
    virtual const base_ft<monetary>::pattern& pos_format_nat() const { return m_pos_format_nat; }
    virtual const base_ft<monetary>::pattern& neg_format_int() const { return m_neg_format_int; }
    virtual const base_ft<monetary>::pattern& neg_format_nat() const { return m_neg_format_nat; }
    virtual int frac_digits_int() const { return m_frac_digits_int; }
    virtual int frac_digits_nat() const { return m_frac_digits_nat; }
    virtual CharT decimal_point() const { return m_decimal_point; }
    virtual CharT thousands_sep() const { return m_thousands_sep; }

private:
    std::vector<uint8_t>        m_grouping;
    std::basic_string<CharT>    m_curr_symbol_int;
    std::basic_string<CharT>    m_curr_symbol_nat;
    std::basic_string<CharT>    m_positive_sign_int;
    std::basic_string<CharT>    m_positive_sign_nat;
    std::basic_string<CharT>    m_negative_sign_int;
    std::basic_string<CharT>    m_negative_sign_nat;
    base_ft<monetary>::pattern      m_pos_format_int;
    base_ft<monetary>::pattern      m_pos_format_nat;
    base_ft<monetary>::pattern      m_neg_format_int;
    base_ft<monetary>::pattern      m_neg_format_nat;
    int                         m_frac_digits_int;
    int                         m_frac_digits_nat;
    CharT                       m_decimal_point;
    CharT                       m_thousands_sep;
};

template <>
class monetary_conf<char8_t> : public ft_basic<monetary<char8_t>>
{
public:
    using char_type = char8_t;

public:
    monetary_conf(const std::string& name)
        : ft_basic<monetary<char8_t>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = u8'.';
            m_thousands_sep = u8',';
            m_negative_sign_int = u8"-";
            m_negative_sign_nat = u8"-";
            m_frac_digits_int = 0;
            m_frac_digits_nat = 0;
            m_pos_format_nat = base_ft<monetary>::s_default_pattern;
            m_neg_format_nat = base_ft<monetary>::s_default_pattern;
            m_pos_format_int = base_ft<monetary>::s_default_pattern;
            m_neg_format_int = base_ft<monetary>::s_default_pattern;
        }
        else
        {
            monetary_conf<char32_t> monetary_tmp(name);
            
            {
                const auto& input = monetary_tmp.decimal_point();
                auto byte_str = detail::to_u8string(input);
                if (byte_str.size() == 1) m_decimal_point = byte_str[0];
                else m_decimal_point = u8'.';
            }
            {
                const auto& input = monetary_tmp.thousands_sep();
                auto byte_str = detail::to_u8string(input);
                if (byte_str.size() == 1) m_thousands_sep = byte_str[0];
                else m_thousands_sep = u8'\0';
            }
            m_frac_digits_int = monetary_tmp.frac_digits_int();
            m_frac_digits_nat = monetary_tmp.frac_digits_nat();
            m_grouping = monetary_tmp.grouping();
            
            {
                const auto& input = monetary_tmp.positive_sign_int();
                m_positive_sign_int = detail::to_u8string(input);
            }
            {
                const auto& input = monetary_tmp.positive_sign_nat();
                m_positive_sign_nat = detail::to_u8string(input);
            }
            
            {
                const auto& input = monetary_tmp.negative_sign_int();
                m_negative_sign_int = detail::to_u8string(input);
            }
            {
                const auto& input = monetary_tmp.negative_sign_nat();
                m_negative_sign_nat = detail::to_u8string(input);
            }
            
            {
                const auto& input = monetary_tmp.curr_symbol_nat();
                m_curr_symbol_nat = detail::to_u8string(input);
            }
            
            {
                const auto& input = monetary_tmp.curr_symbol_int();
                m_curr_symbol_int = detail::to_u8string(input);
            }
            
            m_pos_format_nat = monetary_tmp.pos_format_nat();
            m_pos_format_int = monetary_tmp.pos_format_int();
            m_neg_format_nat = monetary_tmp.neg_format_nat();
            m_neg_format_int = monetary_tmp.neg_format_int();
        }
    }
    
public:
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }
    virtual const std::basic_string<char8_t>& curr_symbol_int() const { return m_curr_symbol_int; }
    virtual const std::basic_string<char8_t>& curr_symbol_nat() const { return m_curr_symbol_nat; }
    virtual const std::basic_string<char8_t>& positive_sign_int() const { return m_positive_sign_int; }
    virtual const std::basic_string<char8_t>& positive_sign_nat() const { return m_positive_sign_nat; }
    virtual const std::basic_string<char8_t>& negative_sign_int() const { return m_negative_sign_int; }
    virtual const std::basic_string<char8_t>& negative_sign_nat() const { return m_negative_sign_nat; }
    virtual const base_ft<monetary>::pattern& pos_format_int() const { return m_pos_format_int; }
    virtual const base_ft<monetary>::pattern& pos_format_nat() const { return m_pos_format_nat; }
    virtual const base_ft<monetary>::pattern& neg_format_int() const { return m_neg_format_int; }
    virtual const base_ft<monetary>::pattern& neg_format_nat() const { return m_neg_format_nat; }
    virtual int frac_digits_int() const { return m_frac_digits_int; }
    virtual int frac_digits_nat() const { return m_frac_digits_nat; }
    virtual char8_t decimal_point() const { return m_decimal_point; }
    virtual char8_t thousands_sep() const { return m_thousands_sep; }

private:
    std::vector<uint8_t>        m_grouping;
    std::basic_string<char8_t>  m_curr_symbol_int;
    std::basic_string<char8_t>  m_curr_symbol_nat;
    std::basic_string<char8_t>  m_positive_sign_int;
    std::basic_string<char8_t>  m_positive_sign_nat;
    std::basic_string<char8_t>  m_negative_sign_int;
    std::basic_string<char8_t>  m_negative_sign_nat;
    base_ft<monetary>::pattern      m_pos_format_int;
    base_ft<monetary>::pattern      m_pos_format_nat;
    base_ft<monetary>::pattern      m_neg_format_int;
    base_ft<monetary>::pattern      m_neg_format_nat;
    int                         m_frac_digits_int;
    int                         m_frac_digits_nat;
    char8_t                     m_decimal_point;
    char8_t                     m_thousands_sep;
};
}