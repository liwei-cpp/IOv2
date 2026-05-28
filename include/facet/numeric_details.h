#pragma once
#include <common/clocale_wrapper.h>
#include <common/metafunctions.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>
#include <facet/facet_helper.h>

#include <clocale>
#include <cstdint>
#include <string>
#include <vector>

#include <langinfo.h>

namespace IOv2
{
template <typename CharT> class numeric_conf;
template <typename CharT> class numeric;

template <>
class numeric_conf<char> : public ft_basic<numeric<char>>
{
public:
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = '.';
            m_thousands_sep = ',';
            m_true_name = "true";
            m_false_name = "false";
            return;
        }

        // Snapshot all locale-dependent strings while the locale guard is
        // active. The lconv* and nl_langinfo() pointers may be invalidated
        // by any subsequent setlocale()/uselocale() call (e.g. inside
        // string_to_char_convert / detail::to_wstring), so copy first
        // and convert after.
        std::string dp_raw, ts_raw, grp_raw, yes_raw, no_raw;
        bool yes_set = false, no_set = false;
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();
            if (lc->decimal_point) dp_raw  = lc->decimal_point;
            if (lc->thousands_sep) ts_raw  = lc->thousands_sep;
            if (lc->grouping)      grp_raw = lc->grouping;

            // Treat empty YESSTR/NOSTR the same as a missing key: an empty
            // m_true_name / m_false_name would make numeric::get(bool&) fail
            // unconditionally (zero-length prefix never matches) and let
            // numeric::put(bool) write nothing but padding.
            if (auto* p = nl_langinfo(YESSTR); p && *p) { yes_raw = p; yes_set = true; }
            if (auto* p = nl_langinfo(NOSTR);  p && *p) { no_raw  = p; no_set  = true; }
        }

        m_decimal_point = FacetHelper::string_to_char_convert(dp_raw, name);
        m_thousands_sep = FacetHelper::string_to_char_convert(ts_raw, name);

        // string_to_char_convert hard-codes '\0' as its failure sentinel. A
        // decimal point of '\0' would later be mistaken for an in-stream
        // null byte during parsing; fall back to '.' so the contract that
        // m_decimal_point is a real, distinguishable character is preserved.
        if (m_decimal_point == '\0') m_decimal_point = '.';

        if (m_thousands_sep != '\0' && !grp_raw.empty())
        {
            // Copy raw POSIX grouping bytes, then normalise into the
            // internal convention here at the POSIX boundary.
            // Downstream (numeric, user-derived numeric_conf) sees
            // only the internal form.
            m_grouping.resize(grp_raw.size());
            for (size_t i = 0; i < grp_raw.size(); ++i)
                m_grouping[i] = static_cast<uint8_t>(grp_raw[i]);
            FacetHelper::adjust_grouping(m_grouping);
        }

        if (yes_set) m_true_name  = yes_raw;
        else         m_true_name  = "true";
        if (no_set)  m_false_name = no_raw;
        else         m_false_name = "false";
    }

public:
    virtual char decimal_point() const { return m_decimal_point; }
    virtual char thousands_sep() const { return m_thousands_sep; }
    virtual const std::string& truename() const { return m_true_name; }
    virtual const std::string& falsename() const { return m_false_name; }
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

private:
    char m_decimal_point;
    char m_thousands_sep;
    std::string m_true_name;
    std::string m_false_name;
    std::vector<uint8_t> m_grouping;
};

template <typename CharT>
    requires std::is_same_v<CharT, wchar_t> ||
                (std::is_same_v<CharT, char32_t> &&
                 wchar_t_is_utf32)
class numeric_conf<CharT> : public ft_basic<numeric<CharT>>
{
public:
    using char_type = CharT;

public:
    numeric_conf(const std::string& name)
        : ft_basic<numeric<CharT>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            if constexpr (std::is_same_v<CharT, wchar_t>)
            {
                m_decimal_point = L'.';
                m_thousands_sep = L',';
                m_true_name = L"true";
                m_false_name = L"false";
            }
            else
            {
                m_decimal_point = U'.';
                m_thousands_sep = U',';
                m_true_name = U"true";
                m_false_name = U"false";
            }
            return;
        }

        // Snapshot all locale-dependent strings while the locale guard is
        // active. The lconv* and nl_langinfo() pointers may be invalidated
        // by any subsequent setlocale()/uselocale() call (e.g. inside
        // detail::to_wstring / detail::to_u32string), so copy first and
        // convert after.
        std::string dp_raw, ts_raw, grp_raw, yes_raw, no_raw;
        bool yes_set = false, no_set = false;
        {
            clocale_wrapper inter_locale(name.c_str());
            clocale_user guard(inter_locale);
            const lconv* lc = localeconv();
            if (lc->decimal_point) dp_raw  = lc->decimal_point;
            if (lc->thousands_sep) ts_raw  = lc->thousands_sep;
            if (lc->grouping)      grp_raw = lc->grouping;

            // Treat empty YESSTR/NOSTR the same as a missing key: an empty
            // m_true_name / m_false_name would make numeric::get(bool&) fail
            // unconditionally (zero-length prefix never matches) and let
            // numeric::put(bool) write nothing but padding.
            if (auto* p = nl_langinfo(YESSTR); p && *p) { yes_raw = p; yes_set = true; }
            if (auto* p = nl_langinfo(NOSTR);  p && *p) { no_raw  = p; no_set  = true; }
        }

        m_decimal_point = FacetHelper::string_to_widechar_convert<CharT>(
            dp_raw, name, static_cast<CharT>('.'));
        m_thousands_sep = FacetHelper::string_to_widechar_convert<CharT>(
            ts_raw, name, static_cast<CharT>('\0'));

        // string_to_widechar_convert only substitutes default_char on the
        // empty-input / empty-conversion paths; a successful conversion
        // that yields CharT('\0') is passed through. Treat that case the
        // same way and fall back to '.', so m_decimal_point is always a
        // real, distinguishable character.
        if (m_decimal_point == static_cast<CharT>('\0'))
            m_decimal_point = static_cast<CharT>('.');

        if (m_thousands_sep != static_cast<CharT>('\0') && !grp_raw.empty())
        {
            // Copy raw POSIX grouping bytes, then normalise into the
            // internal convention here at the POSIX boundary.
            m_grouping.resize(grp_raw.size());
            for (size_t i = 0; i < grp_raw.size(); ++i)
                m_grouping[i] = static_cast<uint8_t>(grp_raw[i]);
            FacetHelper::adjust_grouping(m_grouping);
        }

        if (!yes_set)
        {
            if constexpr (std::is_same_v<CharT, wchar_t>) m_true_name = L"true";
            else                                          m_true_name = U"true";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_true_name = detail::to_wstring(yes_raw.c_str(), name);
            else
                m_true_name = detail::to_u32string(yes_raw.c_str(), name);
        }

        if (!no_set)
        {
            if constexpr (std::is_same_v<CharT, wchar_t>) m_false_name = L"false";
            else                                          m_false_name = U"false";
        }
        else
        {
            if constexpr(std::is_same_v<CharT, wchar_t>)
                m_false_name = detail::to_wstring(no_raw.c_str(), name);
            else
                m_false_name = detail::to_u32string(no_raw.c_str(), name);
        }
    }

public:
    virtual CharT decimal_point() const { return m_decimal_point; }
    virtual CharT thousands_sep() const { return m_thousands_sep; }
    virtual const std::basic_string<CharT>& truename() const { return m_true_name; }
    virtual const std::basic_string<CharT>& falsename() const { return m_false_name; }
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

private:
    CharT m_decimal_point;
    CharT m_thousands_sep;
    std::basic_string<CharT>    m_true_name;
    std::basic_string<CharT>    m_false_name;
    std::vector<uint8_t>        m_grouping;
};

template <>
class numeric_conf<char8_t> : public ft_basic<numeric<char8_t>>
{
    static_assert(wchar_t_is_utf32,
        "numeric_conf<char8_t> delegates to numeric_conf<char32_t>, which "
        "requires wchar_t to be a 32-bit UTF-32 code unit. This platform "
        "(e.g. wchar_t is 16-bit UTF-16) does not satisfy that assumption.");

public:
    using char_type = char8_t;

public:
    numeric_conf(const std::string& name)
        : ft_basic<numeric<char8_t>>()
    {
        if ((name == "C") || (name == "POSIX"))
        { // "C" locale
            m_decimal_point = u8'.';
            m_thousands_sep = u8',';
            m_true_name = u8"true";
            m_false_name = u8"false";
            return;
        }

        numeric_conf<char32_t> numeric_temp(name);
        {
            const auto input = numeric_temp.decimal_point();
            auto output = detail::to_u8string(input);
            // Fall back to u8'.' when the UTF-8 encoding is not a single
            // byte OR when it is a single u8'\0' byte. The latter would be
            // indistinguishable from an in-stream null byte during parsing.
            m_decimal_point = (output.size() != 1 || output[0] == u8'\0')
                                  ? u8'.' : output[0];
        }

        {
            const auto input = numeric_temp.thousands_sep();
            auto output = detail::to_u8string(input);
            m_thousands_sep = (output.size() != 1) ? u8',' : output[0];
        }

        m_true_name = detail::to_u8string(numeric_temp.truename());
        m_false_name = detail::to_u8string(numeric_temp.falsename());

        m_grouping = numeric_temp.grouping();

        // After the single-byte fallbacks above, m_thousands_sep may have
        // collapsed onto u8',' while m_decimal_point also resolved to u8','
        // (e.g. a locale where decimal_point is ',' and thousands_sep is a
        // multi-byte UTF-8 codepoint such as U+202F NARROW NO-BREAK SPACE).
        // When the two separators coincide, downstream extract_int /
        // extract_float would compare each input byte against both fields
        // with the same value, making grouping detection ambiguous. Drop
        // grouping in that case so parsing falls back to the no-grouping
        // path instead of misclassifying digits.
        if (m_thousands_sep == m_decimal_point)
            m_grouping.clear();
    }

public:
    virtual char8_t decimal_point() const { return m_decimal_point; }
    virtual char8_t thousands_sep() const { return m_thousands_sep; }
    virtual const std::basic_string<char8_t>& truename() const { return m_true_name; }
    virtual const std::basic_string<char8_t>& falsename() const { return m_false_name; }
    virtual const std::vector<uint8_t>& grouping() const { return m_grouping; }

private:
    char8_t m_decimal_point;
    char8_t m_thousands_sep;
    std::basic_string<char8_t>  m_true_name;
    std::basic_string<char8_t>  m_false_name;
    std::vector<uint8_t>        m_grouping;
};
}
