#pragma once
#include <algorithm>
#include <bit>
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

private:
    template <unsigned bit>
    struct ISBit
    {
        constexpr static mask value = (std::endian::native == std::endian::big) ?
                                      (1 << (bit)) : 
                                      (((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8)));
    };

public:
    constexpr static mask upper = ISBit<0>::value;  /* UPPERCASE.   */
    constexpr static mask lower = ISBit<1>::value;  /* lowercase.   */
    constexpr static mask alpha = ISBit<2>::value;  /* Alphabetic.  */
    constexpr static mask digit = ISBit<3>::value;  /* Numeric.     */
    constexpr static mask xdigit = ISBit<4>::value; /* Hexadecimal numeric.  */
    constexpr static mask space = ISBit<5>::value;  /* Whitespace.  */
    constexpr static mask print = ISBit<6>::value;  /* Printing.    */
    constexpr static mask cntrl = ISBit<9>::value;  /* Control character.  */
    constexpr static mask punct = ISBit<10>::value; /* Punctuation. */
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
        , m_toupper(m_inter_locale.c_locale->__ctype_toupper)
        , m_tolower(m_inter_locale.c_locale->__ctype_tolower)
    {
        for (unsigned c = 0; c <= std::numeric_limits<unsigned char>::max(); ++c)
        {
            m_table[c] = (m_inter_locale.c_locale->__ctype_b[c] & base_ft<ctype>::all);
        }
    }

public:
    virtual mask is(char c) const
    {
        return m_table[static_cast<unsigned char>(c)];
    }
    
    virtual char toupper(char c) const
    {
        return m_toupper[static_cast<unsigned char>(c)];
    }
    
    virtual char tolower(char c) const
    {
        return m_tolower[static_cast<unsigned char>(c)];
    }
    
    virtual char widen(char c) const { return c; }
    
    virtual std::optional<char> narrow(char c) const { return c; }
    
private:
    clocale_wrapper   m_inter_locale;
    const int* const  m_toupper;
    const int* const  m_tolower;
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