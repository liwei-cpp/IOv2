#pragma once
// Requires POSIX.1-2008 (_POSIX_C_SOURCE >= 200809L or _GNU_SOURCE).
// The *_l character-classification and conversion functions used below
// (isupper_l, toupper_l, wctype_l, iswctype_l, towupper_l, etc.) are
// POSIX extensions, not standard C++. The build system must define the
// appropriate feature-test macro before any system header is included.
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <facet/facet_common.h>

#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

namespace IOv2
{
// The ctype lookup tables in this header and in ctype.h are sized as
// std::numeric_limits<unsigned char>::max() + 1, on the assumption that a
// byte is 8 bits and that the table covers all values an unsigned char can
// hold. Exotic platforms with CHAR_BIT != 8 (some DSPs, historical mainframes)
// would silently grow every per-byte table by 2^(CHAR_BIT-8)x. Reject them at
// compile time rather than ship a non-obviously-broken binary.
static_assert(CHAR_BIT == 8,
    "facet/ctype tables assume an 8-bit byte; see ctype.h::s_len");

template <typename CharT> class ctype;

template <>
class base_ft<ctype> : public abs_ft
{
public:
    using abs_ft::abs_ft;

public:
    using mask = unsigned short;

    // Portable fixed mask values (implementation-independent)
    //
    // When adding a new primitive mask bit, update ALL of:
    //   1) the constants below
    //   2) ctype_conf<CharT>::m_wmask_<name>  (private member)
    //   3) ctype_conf<CharT> constructor      (wctype_wrapper assignment)
    //   4) ctype_conf<CharT>::is()            (iswctype_l check)
    // Missing #2 or #3 leaves the wctype_t uninitialized — UB at iswctype_l.
    // Missing #4 silently drops the new bit from is() results.
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
        {
            clocale_user guard(m_inter_locale);
            for (size_t j = 0; j < std::size(m_widen); ++j)
                m_widen[j] = btowc(j);
        }

        auto wctype_wrapper = [&](const char* category)
        {
            wctype_t res = wctype_l(category, m_inter_locale.c_locale);
            if (res == 0)
                throw cvt_error(std::string("ctype_conf constructor failed: wctype_l returned 0 for category ") + category);
            return res;
        };
        m_wmask_upper  = wctype_wrapper("upper");
        m_wmask_lower  = wctype_wrapper("lower");
        m_wmask_alpha  = wctype_wrapper("alpha");
        m_wmask_digit  = wctype_wrapper("digit");
        m_wmask_xdigit = wctype_wrapper("xdigit");
        m_wmask_space  = wctype_wrapper("space");
        m_wmask_print  = wctype_wrapper("print");
        m_wmask_cntrl  = wctype_wrapper("cntrl");
        m_wmask_punct  = wctype_wrapper("punct");
    }

    virtual base_ft<ctype>::mask is(CharT _c) const
    {
        if (out_of_wchar_range(_c)) return 0;

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
        if (out_of_wchar_range(c)) return c;
        return towupper_l(static_cast<wchar_t>(c), m_inter_locale.c_locale);
    }

    virtual CharT tolower(CharT c) const
    {
        if (out_of_wchar_range(c)) return c;
        return towlower_l(static_cast<wchar_t>(c), m_inter_locale.c_locale);
    }

    // Matches std::ctype<wchar_t>::widen() semantics: only required to be
    // correct for the basic source character set. For multi-byte locales
    // (e.g. UTF-8), high bytes are not standalone characters and btowc()
    // returns WEOF for them at table-build time; the corresponding entries
    // in m_widen hold WEOF cast to CharT (a sentinel-looking but
    // unspecified value). Callers must not widen() such bytes.
    virtual CharT widen(char c) const
    {
        return static_cast<CharT>(m_widen[static_cast<unsigned char>(c)]);
    }

    // Implementation note: unlike is/toupper/tolower (which call *_l
    // functions with an explicit, immutable locale_t), wctob has no _l
    // variant -- it reads whatever locale is currently active on the
    // calling thread. We therefore install m_inter_locale via clocale_user
    // for the duration of the wctob call, and restore on scope exit.
    //
    // Visible side effect for the window of this call: the calling
    // thread's locale is m_inter_locale, not whatever it was on entry.
    // Anything reached during this call -- in particular, a callback
    // dispatched through a user-derived override of this function --
    // that performs locale-sensitive I/O (std::ostream numeric
    // formatting, std::format, third-party locale-aware code) will
    // observe m_inter_locale instead of the thread's outer locale.
    // Derivations should keep this function pure-computational and
    // avoid locale-sensitive side effects inside the override; the same
    // guidance the std::ctype facet conventions imply.
    virtual std::optional<char> narrow(CharT wc) const
    {
        if (out_of_wchar_range(wc)) return std::nullopt;
        clocale_user guard(m_inter_locale);
        const int c = wctob(wc);
        if (c == EOF) return std::nullopt;
        else return static_cast<char>(c);
    }

private:
    // The out-of-wchar-range guards used by is/toupper/tolower/narrow
    // above are provided by IOv2::out_of_wchar_range in facet_common.h
    // (reusable across facets); resolved here via unqualified lookup
    // since this class lives in the same namespace.

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

// Specialisation for UTF-8 code units (char8_t). The design rests on two
// structural guarantees of the UTF-8 encoding: (1) no byte in [0x00, 0x7F]
// ever appears as a leading or continuation byte of a multi-byte sequence,
// so code units in that range are ASCII-compatible and can be classified
// exactly like the corresponding char value in the locale; (2) every byte
// in [0x80, 0xFF] is either a leading byte (0xC0–0xFF) or a continuation
// byte (0x80–0xBF) of a multi-byte sequence, and therefore does not
// represent a standalone Unicode code point.
//
// For [0x00, 0x7F]: all operations delegate to an internal ctype_conf<char>,
// which builds locale-aware lookup tables for all 256 char values. Only the
// lower half of that table is meaningful here; the high half is never used.
//
// For [0x80, 0xFF]: each API applies the conservative treatment documented
// on its individual method below.
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
            return static_cast<mask>(0);  // not a standalone code point; no classification applies
        return m_internal.is(static_cast<char>(c));
    }

    virtual char8_t toupper(char8_t c) const
    {
        if (c & 0x80) return c;  // not a standalone code point; no case information
        return static_cast<char8_t>(m_internal.toupper(static_cast<char>(c)));
    }

    virtual char8_t tolower(char8_t c) const
    {
        if (c & 0x80) return c;  // not a standalone code point; no case information
        return static_cast<char8_t>(m_internal.tolower(static_cast<char>(c)));
    }

    // char8_t has the same range as unsigned char, so every char value maps
    // to exactly one char8_t code unit with no loss and no WEOF sentinel (cf.
    // the wchar_t/char32_t specialisation, where btowc() may return WEOF for
    // high bytes).
    virtual char8_t widen(char c) const { return static_cast<char8_t>(c); }

    virtual std::optional<char> narrow(char8_t c) const
    {
        if (c & 0x80) return std::nullopt;  // not a standalone code point; no char equivalent
        return static_cast<char>(c);
    }

private:
    ctype_conf<char> m_internal;
};
}
