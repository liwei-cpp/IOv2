#pragma once
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <optional>

#include <facet/facet_common.h>
#include <common/clocale_wrapper.h>

namespace IOv2
{
template <typename CharT> class ctype;

template <>
class base_ft<ctype> : public abs_ft
{
public:
    using abs_ft::abs_ft;

public:
    using mask = unsigned short;

    // Portable fixed mask values (implementation-independent)
    constexpr static mask upper  = 0x0001;
    constexpr static mask lower  = 0x0002;
    constexpr static mask alpha  = 0x0004;
    constexpr static mask digit  = 0x0008;
    constexpr static mask xdigit = 0x0010;
    constexpr static mask space  = 0x0020;
    constexpr static mask print  = 0x0040;
    constexpr static mask cntrl  = 0x0080;
    constexpr static mask punct  = 0x0100;

    constexpr static mask alnum = alpha | digit;
    constexpr static mask graph = alnum | punct;

    constexpr static mask all = upper | lower | alpha | digit | xdigit |
        space | print | graph | cntrl | punct;
};

template <typename CharT> class ctype_conf;

template <>
class ctype_conf<char> : public ft_basic<ctype<char>>
{
public:
    ctype_conf(const std::string& name)
        : ft_basic<ctype<char>>()
        , m_inter_locale(name.c_str())
    {
        // Build lookup tables using standard POSIX functions
        for (unsigned c = 0; c <= std::numeric_limits<unsigned char>::max(); ++c)
        {
            m_toupper_table[c] = toupper_l(static_cast<int>(c), m_inter_locale.c_locale);
            m_tolower_table[c] = tolower_l(static_cast<int>(c), m_inter_locale.c_locale);

            mask m = 0;
            if (isupper_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::upper;
            if (islower_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::lower;
            if (isalpha_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::alpha;
            if (isdigit_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::digit;
            if (isxdigit_l(static_cast<int>(c), m_inter_locale.c_locale)) m |= base_ft<ctype>::xdigit;
            if (isspace_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::space;
            if (isprint_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::print;
            if (iscntrl_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::cntrl;
            if (ispunct_l(static_cast<int>(c), m_inter_locale.c_locale))  m |= base_ft<ctype>::punct;
            m_table[c] = m;
        }
    }

public:
    virtual mask is(char c) const
    {
        return m_table[static_cast<unsigned char>(c)];
    }

    virtual char toupper(char c) const
    {
        return static_cast<char>(m_toupper_table[static_cast<unsigned char>(c)]);
    }

    virtual char tolower(char c) const
    {
        return static_cast<char>(m_tolower_table[static_cast<unsigned char>(c)]);
    }

    virtual char widen(char c) const { return c; }

    virtual std::optional<char> narrow(char c) const { return c; }

private:
    clocale_wrapper m_inter_locale;
    int m_toupper_table[std::numeric_limits<unsigned char>::max() + 1];
    int m_tolower_table[std::numeric_limits<unsigned char>::max() + 1];
    mask m_table[std::numeric_limits<unsigned char>::max() + 1];
};

template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> || 
                (std::is_same_v<CharT, char32_t> && 
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class ctype_conf<CharT> : public ft_basic<ctype<CharT>>
{
public:
    ctype_conf(const std::string& name)
        : ft_basic<ctype<CharT>>()
        , m_inter_locale(name.c_str())
    {
        clocale_user guard(m_inter_locale);
        for (size_t j = 0; j < sizeof(m_widen) / sizeof(wint_t); ++j)
          m_widen[j] = btowc(j);
          
        m_wmask_upper  = wctype_l("upper",  m_inter_locale.c_locale);
        m_wmask_lower  = wctype_l("lower",  m_inter_locale.c_locale);
        m_wmask_alpha  = wctype_l("alpha",  m_inter_locale.c_locale);
        m_wmask_digit  = wctype_l("digit",  m_inter_locale.c_locale);
        m_wmask_xdigit = wctype_l("xdigit", m_inter_locale.c_locale);
        m_wmask_space  = wctype_l("space",  m_inter_locale.c_locale);
        m_wmask_print  = wctype_l("print",  m_inter_locale.c_locale);
        m_wmask_cntrl  = wctype_l("cntrl",  m_inter_locale.c_locale);
        m_wmask_punct  = wctype_l("punct",  m_inter_locale.c_locale);
    }
    
    virtual base_ft<ctype>::mask is(CharT _c) const
    {
        base_ft<ctype>::mask res = 0;
        const auto c = static_cast<wchar_t>(_c);
        if (iswctype_l(c, m_wmask_upper, m_inter_locale.c_locale))  res |= base_ft<ctype>::upper;
        if (iswctype_l(c, m_wmask_lower, m_inter_locale.c_locale))  res |= base_ft<ctype>::lower;
        if (iswctype_l(c, m_wmask_alpha, m_inter_locale.c_locale))  res |= base_ft<ctype>::alpha;
        if (iswctype_l(c, m_wmask_digit, m_inter_locale.c_locale))  res |= base_ft<ctype>::digit;
        if (iswctype_l(c, m_wmask_xdigit, m_inter_locale.c_locale)) res |= base_ft<ctype>::xdigit;
        if (iswctype_l(c, m_wmask_space, m_inter_locale.c_locale))  res |= base_ft<ctype>::space;
        if (iswctype_l(c, m_wmask_print, m_inter_locale.c_locale))  res |= base_ft<ctype>::print;
        if (iswctype_l(c, m_wmask_cntrl, m_inter_locale.c_locale))  res |= base_ft<ctype>::cntrl;
        if (iswctype_l(c, m_wmask_punct, m_inter_locale.c_locale))  res |= base_ft<ctype>::punct;
        return res;
    }

    virtual CharT toupper(CharT c) const
    {
        return towupper_l(static_cast<wchar_t>(c), m_inter_locale.c_locale);
    }

    virtual CharT tolower(CharT c) const
    {
        return towlower_l(static_cast<wchar_t>(c), m_inter_locale.c_locale);
    }

    virtual CharT widen(char c) const
    {
        return static_cast<CharT>(m_widen[static_cast<unsigned char>(c)]);
    }

    virtual std::optional<char> narrow(CharT wc) const
    {
        clocale_user guard(m_inter_locale);
        const int c = wctob(wc);
        if (c == EOF) return std::nullopt;
        else return static_cast<char>(c);
    }

private:
    clocale_wrapper   m_inter_locale;
    wint_t            m_widen[1 + std::numeric_limits<unsigned char>::max()];

    wctype_t          m_wmask_upper;
    wctype_t          m_wmask_lower;
    wctype_t          m_wmask_alpha;
    wctype_t          m_wmask_digit;
    wctype_t          m_wmask_xdigit;
    wctype_t          m_wmask_space;
    wctype_t          m_wmask_print;
    wctype_t          m_wmask_cntrl;
    wctype_t          m_wmask_punct;
};

template <>
class ctype_conf<char8_t> : public ft_basic<ctype<char8_t>>
{
public:
    ctype_conf(const std::string& name)
        : ft_basic<ctype<char8_t>>()
        , m_internal(name)
    {}

public:
    virtual mask is(char8_t c) const
    {
        if (c & 0x80)
            return static_cast<mask>(0);
        return m_internal.is(static_cast<char>(c));
    }
    
    virtual char8_t toupper(char8_t c) const
    {
        if (c & 0x80) return c;
        return static_cast<char8_t>(m_internal.toupper(static_cast<char>(c)));
    }

    virtual char8_t tolower(char8_t c) const
    {
        if (c & 0x80) return c;
        return static_cast<char8_t>(m_internal.tolower(static_cast<char>(c)));
    }

    virtual char8_t widen(char c) const { return static_cast<char8_t>(c); }
    
    virtual std::optional<char> narrow(char8_t c) const
    {
        if (c & 0x80) return std::nullopt;
        return static_cast<char>(c);
    }
    
private:
    ctype_conf<char> m_internal;
};
}