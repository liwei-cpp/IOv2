#pragma once
#include <concepts>
#include <iterator>
#include <memory>
#include <common/metafunctions.h>
#include <facet/monetary_details.h>

namespace IOv2
{
template <typename CharT>
class monetary
{
    struct split_info
    {
        std::basic_string<CharT>        m_curr_symbol;
        std::basic_string<CharT>        m_positive_sign;
        std::basic_string<CharT>        m_negative_sign;
        base_ft<monetary>::pattern      m_pos_format;
        base_ft<monetary>::pattern      m_neg_format;
        int                             m_frac_digits;
    };

public:
    using create_rules = facet_create_rule<monetary_conf<CharT>>;

    using char_type = CharT;

    template <shared_ptr_to<monetary_conf<CharT>> TConfPtr>
    monetary(TConfPtr p_obj)
        : m_grouping(p_obj->grouping())
        , m_nat{.m_curr_symbol = p_obj->curr_symbol_nat(),
                .m_positive_sign = p_obj->positive_sign_nat(),
                .m_negative_sign = p_obj->negative_sign_nat(),
                .m_pos_format = p_obj->pos_format_nat(),
                .m_neg_format = p_obj->neg_format_nat(),
                .m_frac_digits = p_obj->frac_digits_nat()}
        , m_int{.m_curr_symbol = p_obj->curr_symbol_int(),
                .m_positive_sign = p_obj->positive_sign_int(),
                .m_negative_sign = p_obj->negative_sign_int(),
                .m_pos_format = p_obj->pos_format_int(),
                .m_neg_format = p_obj->neg_format_int(),
                .m_frac_digits = p_obj->frac_digits_int()}
        , m_decimal_point(p_obj->decimal_point())
        , m_thousands_sep(p_obj->thousands_sep())
    {
        adjust_grouping();
    }

    const std::vector<uint8_t>& grouping() const { return m_grouping; }
    const std::basic_string<CharT>& curr_symbol_int() const { return m_int.m_curr_symbol; }
    const std::basic_string<CharT>& curr_symbol_nat() const { return m_nat.m_curr_symbol; }
    const std::basic_string<CharT>& positive_sign_int() const { return m_int.m_positive_sign; }
    const std::basic_string<CharT>& positive_sign_nat() const { return m_nat.m_positive_sign; }
    const std::basic_string<CharT>& negative_sign_int() const { return m_int.m_negative_sign; }
    const std::basic_string<CharT>& negative_sign_nat() const { return m_nat.m_negative_sign; }
    const base_ft<monetary>::pattern& pos_format_int() const { return m_int.m_pos_format; }
    const base_ft<monetary>::pattern& pos_format_nat() const { return m_nat.m_pos_format; }
    const base_ft<monetary>::pattern& neg_format_int() const { return m_int.m_neg_format; }
    const base_ft<monetary>::pattern& neg_format_nat() const { return m_nat.m_neg_format; }
    int frac_digits_int() const { return m_int.m_frac_digits; }
    int frac_digits_nat() const { return m_nat.m_frac_digits; }
    CharT decimal_point() const { return m_decimal_point; }
    CharT thousands_sep() const { return m_thousands_sep; }
    
    template <typename TIter, std::integral TVal>
    TIter put(TIter s, bool intl, ios_base<char_type>& io, TVal v) const
    {
        constexpr size_t buf_size = std::numeric_limits<TVal>::digits10 + 3;
        char_type vec[buf_size];

        char_type* p = vec + buf_size;
        *--p = '\0';

        using TU = std::make_unsigned_t<TVal>;
        bool negative = false;
        TU uv;

        if constexpr(std::is_unsigned_v<TVal>)
            uv = v;
        else
        {
            bool negative = (v < 0);
            uv = negative ? static_cast<TU>(-(v + 1)) + 1
                          : static_cast<TU>(v);
        }

        do {
            *--p = static_cast<char_type>('0' + (uv % 10));
            uv /= 10;
        } while (uv != 0);

        if (negative)
            *--p = '-';

        return intl ? insert<true>(s, io, p)
                    : insert<false>(s, io, p);
    }
    
    template <typename TIter>
    TIter put(TIter s, bool intl, ios_base<char_type>& io, const std::basic_string<char_type>& digits) const
    {
        return intl ? insert<true>(s, io, digits)
                    : insert<false>(s, io, digits);
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent, std::integral TVal>
    TIter get(TIter beg, TSent end, bool intl, ios_base<char_type>& io, TVal& utils) const
    {
        std::string str;
        bool succ = true;
        
        std::tie(succ, beg) = intl ? extract<true>(beg, end, io, str)
                                   : extract<false>(beg, end, io, str);

        succ &= str_to_v(str, utils);
        if (!succ)
            throw stream_error("monetary parse fail");
        return beg;
    }

    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter beg, TSent end, bool intl, ios_base<char_type>& io, std::basic_string<char_type>& digits) const
    {
        bool succ = true;

        std::string str;
        std::tie(succ, beg) = intl ? extract<true>(beg, end, io, str)
                                   : extract<false>(beg, end, io, str);
        const auto len = str.size();
        if (len)
        {
            digits.clear();
            digits.reserve(len);
            for (auto ch : str)
            {
                if (ch == '-')
                    digits.push_back(s_atoms[0]);
                else if ((ch >= '0') && (ch <= '9'))
                    digits.push_back(s_atoms[ch - '0' + 1]);
            }
        }
        if (!succ)
            throw stream_error("monetary parse fail");
        return beg;
    }

private:
    // this trys to solve the case: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=39168
    // Following the code in gcc: https://github.com/gcc-mirror/gcc/blob/19b98410eb43be60f7e2673afeaabd016321c625/libstdc%2B%2B-v3/include/bits/locale_facets.tcc#L91
    // however, I don't know whether this is appropriate or not, since it does not consider the number except grouping[0]
    // what's more: I don't know whether c++ standard has this limitation is resonable or not...
    void adjust_grouping()
    {
        if ((!m_grouping.empty()) && 
            (m_grouping[0] > 0) &&
            (m_grouping[0] != std::numeric_limits<char>::max()))
            return;
        else
            m_grouping.clear();
    }

private:
    template <bool isIntl, typename TIter>
    TIter insert(TIter s, ios_base<char_type>& io, const std::basic_string<char_type>& digits) const
    {
        const split_info& info = isIntl ? m_int : m_nat;
        using part = base_ft<monetary>::part;

        // Determine if negative or positive formats are to be used, and
        // discard leading negative_sign if it is present.
        const char_type* beg = digits.data();

        base_ft<monetary>::pattern p;
        const std::basic_string<char_type>* sign_ptr;
        if (!(*beg == s_atoms[s_minus]))
        {
            p = info.m_pos_format;
            sign_ptr = &(info.m_positive_sign);
        }
        else
        {
            p = info.m_neg_format;
            sign_ptr = &(info.m_negative_sign);
            if (digits.size())
                ++beg;
        }

        // Look for valid numbers in input digits.
        int len = 0;
        for (auto i = beg; i != beg + digits.size(); ++i)
        {
            char_type ch = *i;
            int j = 1;
            for (; j < 11; ++j)
                if (ch == s_atoms[j]) break;

            if (j == 11) break;
            ++len;
        }

        if (len)
        {
            // Assume valid input, and attempt to format.
            // Break down input numbers into base components, as follows:
            //   final_value = grouped units + (decimal point) + (digits)
            std::basic_string<char_type> value;
            value.reserve(2 * len);

            // Add thousands separators to non-decimal digits, per
            // grouping rules.
            long paddec = len - info.m_frac_digits;
            if (paddec > 0)
            {
                if (info.m_frac_digits < 0)
                    paddec = len;
                if (!m_grouping.empty())
                {
                    value.assign(2 * paddec, char_type());
                    char_type* vend = FacetHelper::add_grouping(&value[0], m_thousands_sep, m_grouping, beg, beg + paddec);
                    value.erase(vend - &value[0]);
                }
                else
                    value.assign(beg, paddec);
            }

            // Deal with decimal point, decimal digits.
            if (info.m_frac_digits > 0)
            {
                value += m_decimal_point;
                if (paddec >= 0)
                    value.append(beg + paddec, info.m_frac_digits);
                else
                {
                    // Have to pad zeros in the decimal position.
                    value.append(-paddec, s_atoms[s_zero]);
                    value.append(beg, len);
                }
            }

            // Calculate length of resulting string.
            const ios_defs::fmtflags f = io.flags() & ios_defs::adjustfield;
            len = value.size() + sign_ptr->size();
            len += ((io.flags() & ios_defs::showbase) ? info.m_curr_symbol.size() : 0);

            std::basic_string<char_type> res;
            res.reserve(2 * len);

            const int width = static_cast<int>(io.width());
            const bool testipad = (f == ios_defs::internal && len < width);
            // Fit formatted digits into the required pattern.
            for (int i = 0; i < 4; ++i)
            {
                const part which = static_cast<part>(p[i]);
                switch (which)
                {
                case base_ft<monetary>::symbol:
                    if (io.flags() & ios_defs::showbase)
                        res += info.m_curr_symbol;
                    break;
                case base_ft<monetary>::sign:
                    // Sign might not exist, or be more than one
                    // character long. In that case, add in the rest
                    // below.
                    if (!sign_ptr->empty())
                        res += (*sign_ptr)[0];
                    break;
                case base_ft<monetary>::value:
                    res += value;
                    break;
                case base_ft<monetary>::space:
                    // At least one space is required, but if internal
                    // formatting is required, an arbitrary number of
                    // fill spaces will be necessary.
                    if (testipad)
                        res.append(width - len, io.fill());
                    else
                        res += io.fill();
                    break;
                case base_ft<monetary>::none:
                    if (testipad)
                        res.append(width - len, io.fill());
                    break;
                }
            }

            // Special case of multi-part sign parts.
            if (sign_ptr->size() > 1)
                res.append(sign_ptr->c_str() + 1, sign_ptr->size() - 1);

            // Pad, if still necessary.
            len = res.size();
            if (width > len)
            {
                if (f == ios_defs::left) // After.
                    res.append(width - len, io.fill());
                else // Before.
                    res.insert(0, width - len, io.fill());
                len = width;
            }

            // Write resulting, fully-formatted string to output iterator.
            s = std::copy(res.data(), res.data() + len, s);
        }
        io.width(0);
        return s;
    }
    
    template <bool isIntl, typename TIter, std::sentinel_for<TIter> TSent>
    std::pair<bool, TIter> extract(TIter beg, TSent end, ios_base<char_type>& io, std::string& units) const
    {
        const split_info& info = isIntl ? m_int : m_nat;
        using part = base_ft<monetary>::part;

        // Deduced sign.
        bool negative = false;
        // Sign size.
        int sign_size = 0;
        // True if sign is mandatory.
        const bool mandatory_sign = (!info.m_positive_sign.empty() && !info.m_negative_sign.empty());
        // String of grouping info from thousands_sep plucked from units.
        std::string grouping_tmp;
        if (!m_grouping.empty())
            grouping_tmp.reserve(32);
        // Last position before the decimal point.
        int last_pos = 0;
        // Separator positions, then, possibly, fractional digits.
        int n = 0;
        // If input iterator is in a valid state.
        bool testvalid = true;
        // Flag marking when a decimal point is found.
        bool testdecfound = false;

        // The tentative returned string is stored here.
        std::string res; res.reserve(32);

        const char_type* lit_zero = s_atoms + s_zero;
        const base_ft<monetary>::pattern p = info.m_neg_format;

        for (int i = 0; i < 4 && testvalid; ++i)
        {
            const part which = static_cast<part>(p[i]);
            switch (which)
            {
            case base_ft<monetary>::symbol:
                // According to 22.2.6.1.2, p2, symbol is required
                // if (io.flags() & ios_base::showbase), otherwise
                // is optional and consumed only if other characters
                // are needed to complete the format.
                if (io.flags() & ios_defs::showbase || sign_size > 1
                    || i == 0
                    || (i == 1 && (mandatory_sign
                        || (static_cast<part>(p[0])
                        == base_ft<monetary>::sign)
                        || (static_cast<part>(p[2])
                        == base_ft<monetary>::space)))
                    || (i == 2 && ((static_cast<part>(p[3])
                        == base_ft<monetary>::value)
                        || (mandatory_sign
                        && (static_cast<part>(p[3])
                            == base_ft<monetary>::sign)))))
                {
                    const int len = info.m_curr_symbol.size();
                    int j = 0;
                    for (; beg != end && j < len && *beg == info.m_curr_symbol[j]; ++beg, (void)++j);
                    if (j != len && (j || io.flags() & ios_defs::showbase))
                        testvalid = false;
                }
                break;
            case base_ft<monetary>::sign:
                // Sign might not exist, or be more than one character long.
                if (!info.m_positive_sign.empty() && beg != end && *beg == info.m_positive_sign[0])
                {
                    sign_size = info.m_positive_sign.size();
                    ++beg;
                }
                else if (!info.m_negative_sign.empty() && beg != end && *beg == info.m_negative_sign[0])
                {
                    negative = true;
                    sign_size = info.m_negative_sign.size();
                    ++beg;
                }
                else if (!info.m_positive_sign.empty() && info.m_negative_sign.empty())
                // "... if no sign is detected, the result is given the sign
                // that corresponds to the source of the empty string"
                    negative = true;
                else if (mandatory_sign)
                    testvalid = false;
                break;
            case base_ft<monetary>::value:
                // Extract digits, remove and stash away the
                // grouping of found thousands separators.
                for (; beg != end; ++beg)
                {
                    const char_type c = *beg;
                    const char_type* q = std::find(lit_zero, lit_zero + 10, c);
                    if (q != lit_zero + 10)
                    {
                        res += '0' + (q - lit_zero);
                        ++n;
                    }
                    else if (c == m_decimal_point && !testdecfound)
                    {
                        if (info.m_frac_digits <= 0)
                            break;

                        last_pos = n;
                        n = 0;
                        testdecfound = true;
                    }
                    else if (!m_grouping.empty() && c == m_thousands_sep && !testdecfound)
                    {
                        if (n)
                        {
                            // Mark position for later analysis.
                            grouping_tmp += static_cast<char>(n);
                            n = 0;
                        }
                        else
                        {
                            testvalid = false;
                            break;
                        }
                    }
                    else
                        break;
                }
                if (res.empty())
                    testvalid = false;
                break;
            case base_ft<monetary>::space:
                // At least one space is required.
                if (beg != end && (*beg == io.fill()))
                    ++beg;
                else
                    testvalid = false;
            // fallthrough
            case base_ft<monetary>::none:
                // Only if not at the end of the pattern.
                if (i != 3)
                for (; beg != end && (*beg == io.fill()); ++beg);
                break;
            }
        }

        // Need to get the rest of the sign characters, if they exist.
        if (sign_size > 1 && testvalid)
        {
            const auto& sign_str = negative ? info.m_negative_sign
                                            : info.m_positive_sign;
            int i = 1;
            for (; beg != end && i < sign_size && *beg == sign_str[i]; ++beg, (void)++i);
            if (i != sign_size)
                testvalid = false;
        }

        bool succ = true;
        if (testvalid)
        {
            // Strip leading zeros.
            if (res.size() > 1)
            {
                const auto first = res.find_first_not_of('0');
                const bool only_zeros = first == std::string::npos;
                if (first)
                    res.erase(0, only_zeros ? res.size() - 1 : first);
            }

            // 22.2.6.1.2, p4
            if (negative && res[0] != '0')
                res.insert(res.begin(), '-');

            // Test for grouping fidelity.
            if (grouping_tmp.size())
            {
                // Add the ending grouping.
                grouping_tmp += static_cast<char>(testdecfound ? last_pos : n);
                succ = FacetHelper::verify_grouping(m_grouping, grouping_tmp);
            }

            // Iff not enough digits were supplied after the decimal-point.
            if (testdecfound && n != info.m_frac_digits)
                testvalid = false;
        }

        // Iff valid sequence is not recognized.
        if (!testvalid)
            succ = false;
        else
            units.swap(res);

        return std::pair(succ, beg);
    }

    template <std::integral TVal>
    bool str_to_v(const std::string& s, TVal& value) const
    {
        if (s.empty()) return false;

        size_t i = 0;
        value = 0;
        constexpr TVal max_value = std::numeric_limits<TVal>::max();

        if constexpr (std::is_signed_v<TVal>)
        {
            bool negative = false;
            constexpr TVal min_value = std::numeric_limits<TVal>::min();

            if (s[i] == '+' || s[i] == '-') {
                negative = (s[i] == '-');
                ++i;
            }

            if (i == s.size()) return false;

            for (; i < s.size(); ++i)
            {
                char c = s[i];
                if (c < '0' || c > '9') return false;

                int digit = c - '0';

                if (!negative)
                {
                    if (value > (max_value - digit) / 10)
                        return false;
                }
                else if (value < (min_value + digit) / 10)
                    return false;

                value = value * 10 + (negative ? -digit : digit);
            }
        }
        else
        {
            if (s[i] == '+')
                ++i;
            else if (s[i] == '-')
                return false;

            if (i == s.size()) return false;

            for (; i < s.size(); ++i)
            {
                char c = s[i];
                if (c < '0' || c > '9') return false;

                TVal digit = static_cast<TVal>(c - '0');
                if (value > (max_value - digit) / 10)
                    return false;

                value = value * 10 + digit;
            }
        }
        return true;
    }

private:
    static constexpr size_t s_minus = 0;
    static constexpr size_t s_zero = 1;

private:
    std::vector<uint8_t>      m_grouping;
    split_info                m_nat;
    split_info                m_int;
    CharT                     m_decimal_point;
    CharT                     m_thousands_sep;
    
    const inline static char_type s_atoms[11] = {
            (char_type)'-', (char_type)'0', (char_type)'1', (char_type)'2',
            (char_type)'3', (char_type)'4', (char_type)'5', (char_type)'6',
            (char_type)'7', (char_type)'8', (char_type)'9'
        };
};

template<typename TConfPtr>
monetary(TConfPtr) -> monetary<typename TConfPtr::element_type::char_type>;
}