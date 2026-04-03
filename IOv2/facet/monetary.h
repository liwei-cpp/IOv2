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
    TIter put(TIter __s, bool __intl, ios_base<char_type>& __io, const std::basic_string<char_type>& __digits) const
    {
        return __intl ? insert<true>(__s, __io, __digits)
                      : insert<false>(__s, __io, __digits);
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
    TIter get(TIter __beg, TSent __end, bool __intl, ios_base<char_type>& __io, std::basic_string<char_type>& __digits) const
    {
        bool succ = true;
        
        std::string __str;
        std::tie(succ, __beg) = __intl ? extract<true>(__beg, __end, __io, __str)
                                       : extract<false>(__beg, __end, __io, __str);
        const auto __len = __str.size();
        if (__len)
        {
            __digits.clear();
            __digits.reserve(__len);
            for (auto ch : __str)
            {
                if (ch == '-')
                    __digits.push_back(s_atoms[0]);
                else if ((ch >= '0') && (ch <= '9'))
                    __digits.push_back(s_atoms[ch - '0' + 1]);
            }
        }
        if (!succ)
            throw stream_error("monetary parse fail");
        return __beg;
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
    TIter insert(TIter __s, ios_base<char_type>& __io, const std::basic_string<char_type>& __digits) const
    {
        const split_info& info = isIntl ? m_int : m_nat;
        using part = base_ft<monetary>::part;
        
        // Determine if negative or positive formats are to be used, and
        // discard leading negative_sign if it is present.
        const char_type* __beg = __digits.data();

        base_ft<monetary>::pattern __p;
        const std::basic_string<char_type>* __sign;
        if (!(*__beg == s_atoms[s_minus]))
        {
            __p = info.m_pos_format;
            __sign = &(info.m_positive_sign);
        }
        else
        {
            __p = info.m_neg_format;
            __sign = &(info.m_negative_sign);
            if (__digits.size())
                ++__beg;
        }
       
        // Look for valid numbers in input digits.
        int __len = 0;
        for (auto i = __beg; i != __beg + __digits.size(); ++i)
        {
            char_type ch = *i;
            int j = 1;
            for (; j < 11; ++j)
                if (ch == s_atoms[j]) break;
                
            if (j == 11) break;
            ++__len;
        }
        
        if (__len)
        {
            // Assume valid input, and attempt to format.
            // Break down input numbers into base components, as follows:
            //   final_value = grouped units + (decimal point) + (digits)
            std::basic_string<char_type> __value;
            __value.reserve(2 * __len);

            // Add thousands separators to non-decimal digits, per
            // grouping rules.
            long __paddec = __len - info.m_frac_digits;
            if (__paddec > 0)
            {
                if (info.m_frac_digits < 0)
                    __paddec = __len;
                if (!m_grouping.empty())
                {
                    __value.assign(2 * __paddec, char_type());
                    char_type* __vend = FacetHelper::add_grouping(&__value[0], m_thousands_sep, m_grouping, __beg, __beg + __paddec);
                    __value.erase(__vend - &__value[0]);
                }
                else
                    __value.assign(__beg, __paddec);
            }

            // Deal with decimal point, decimal digits.
            if (info.m_frac_digits > 0)
            {
                __value += m_decimal_point;
                if (__paddec >= 0)
                    __value.append(__beg + __paddec, info.m_frac_digits);
                else
                {
                    // Have to pad zeros in the decimal position.
                    __value.append(-__paddec, s_atoms[s_zero]);
                    __value.append(__beg, __len);
                }
            }
  
            // Calculate length of resulting string.
            const ios_defs::fmtflags __f = __io.flags() & ios_defs::adjustfield;
            __len = __value.size() + __sign->size();
            __len += ((__io.flags() & ios_defs::showbase) ? info.m_curr_symbol.size() : 0);

            std::basic_string<char_type> __res;
            __res.reserve(2 * __len);

            const int __width = static_cast<int>(__io.width());  
            const bool __testipad = (__f == ios_defs::internal && __len < __width);
            // Fit formatted digits into the required pattern.
            for (int __i = 0; __i < 4; ++__i)
            {
                const part __which = static_cast<part>(__p[__i]);
                switch (__which)
                {
                case base_ft<monetary>::symbol:
                    if (__io.flags() & ios_defs::showbase)
                        __res += info.m_curr_symbol;
                    break;
                case base_ft<monetary>::sign:
                    // Sign might not exist, or be more than one
                    // character long. In that case, add in the rest
                    // below.
                    if (!__sign->empty())
                        __res += (*__sign)[0];
                    break;
                case base_ft<monetary>::value:
                    __res += __value;
                    break;
                case base_ft<monetary>::space:
                    // At least one space is required, but if internal
                    // formatting is required, an arbitrary number of
                    // fill spaces will be necessary.
                    if (__testipad)
                        __res.append(__width - __len, __io.fill());
                    else
                        __res += __io.fill();
                    break;
                case base_ft<monetary>::none:
                    if (__testipad)
                        __res.append(__width - __len, __io.fill());
                    break;
                }
            }

            // Special case of multi-part sign parts.
            if (__sign->size() > 1)
                __res.append(__sign->c_str() + 1, __sign->size() - 1);

            // Pad, if still necessary.
            __len = __res.size();
            if (__width > __len)
            {
                if (__f == ios_defs::left) // After.
                    __res.append(__width - __len, __io.fill());
                else // Before.
                    __res.insert(0, __width - __len, __io.fill());
                __len = __width;
            }

            // Write resulting, fully-formatted string to output iterator.
            __s = std::copy(__res.data(), __res.data() + __len, __s);
        }
        __io.width(0);
        return __s;
    }
    
    template <bool isIntl, typename TIter, std::sentinel_for<TIter> TSent>
    std::pair<bool, TIter> extract(TIter __beg, TSent __end, ios_base<char_type>& __io, std::string& __units) const
    {
        const split_info& info = isIntl ? m_int : m_nat;
        using part = base_ft<monetary>::part;
        
        // Deduced sign.
        bool __negative = false;
        // Sign size.
        int __sign_size = 0;
        // True if sign is mandatory.
        const bool __mandatory_sign = (!info.m_positive_sign.empty() && !info.m_negative_sign.empty());
        // String of grouping info from thousands_sep plucked from __units.
        std::string __grouping_tmp;
        if (!m_grouping.empty())
            __grouping_tmp.reserve(32);
        // Last position before the decimal point.
        int __last_pos = 0;
        // Separator positions, then, possibly, fractional digits.
        int __n = 0;
        // If input iterator is in a valid state.
        bool __testvalid = true;
        // Flag marking when a decimal point is found.
        bool __testdecfound = false;

        // The tentative returned string is stored here.
        std::string __res; __res.reserve(32);

        const char_type* __lit_zero = s_atoms + s_zero;
        const base_ft<monetary>::pattern __p = info.m_neg_format;

        for (int __i = 0; __i < 4 && __testvalid; ++__i)
        {
            const part __which = static_cast<part>(__p[__i]);
            switch (__which)
            {
            case base_ft<monetary>::symbol:
                // According to 22.2.6.1.2, p2, symbol is required
                // if (__io.flags() & ios_base::showbase), otherwise
                // is optional and consumed only if other characters
                // are needed to complete the format.
                if (__io.flags() & ios_defs::showbase || __sign_size > 1
                    || __i == 0
                    || (__i == 1 && (__mandatory_sign
                        || (static_cast<part>(__p[0])
                        == base_ft<monetary>::sign)
                        || (static_cast<part>(__p[2])
                        == base_ft<monetary>::space)))
                    || (__i == 2 && ((static_cast<part>(__p[3])
                        == base_ft<monetary>::value)
                        || (__mandatory_sign
                        && (static_cast<part>(__p[3])
                            == base_ft<monetary>::sign)))))
                {
                    const int __len = info.m_curr_symbol.size();
                    int __j = 0;
                    for (; __beg != __end && __j < __len && *__beg == info.m_curr_symbol[__j]; ++__beg, (void)++__j);
                    if (__j != __len && (__j || __io.flags() & ios_defs::showbase))
                        __testvalid = false;
                }
                break;
            case base_ft<monetary>::sign:
                // Sign might not exist, or be more than one character long.
                if (!info.m_positive_sign.empty() && __beg != __end && *__beg == info.m_positive_sign[0])
                {
                    __sign_size = info.m_positive_sign.size();
                    ++__beg;
                }
                else if (!info.m_negative_sign.empty() && __beg != __end && *__beg == info.m_negative_sign[0])
                {
                    __negative = true;
                    __sign_size = info.m_negative_sign.size();
                    ++__beg;
                }
                else if (!info.m_positive_sign.empty() && info.m_negative_sign.empty())
                // "... if no sign is detected, the result is given the sign
                // that corresponds to the source of the empty string"
                    __negative = true;
                else if (__mandatory_sign)
                    __testvalid = false;
                break;
            case base_ft<monetary>::value:
                // Extract digits, remove and stash away the
                // grouping of found thousands separators.
                for (; __beg != __end; ++__beg)
                {
                    const char_type __c = *__beg;
                    const char_type* __q = std::find(__lit_zero, __lit_zero + 10, __c);
                    if (__q != __lit_zero + 10)
                    {
                        __res += '0' + (__q - __lit_zero);
                        ++__n;
                    }
                    else if (__c == m_decimal_point && !__testdecfound)
                    {
                        if (info.m_frac_digits <= 0)
                            break;

                        __last_pos = __n;
                        __n = 0;
                        __testdecfound = true;
                    }
                    else if (!m_grouping.empty() && __c == m_thousands_sep && !__testdecfound)
                    {
                        if (__n)
                        {
                            // Mark position for later analysis.
                            __grouping_tmp += static_cast<char>(__n);
                            __n = 0;
                        }
                        else
                        {
                            __testvalid = false;
                            break;
                        }
                    }
                    else
                        break;
                }
                if (__res.empty())
                    __testvalid = false;
                break;
            case base_ft<monetary>::space:
                // At least one space is required.
                if (__beg != __end && (*__beg == __io.fill()))
                    ++__beg;
                else
                    __testvalid = false;
            // fallthrough
            case base_ft<monetary>::none:
                // Only if not at the end of the pattern.
                if (__i != 3)
                for (; __beg != __end && (*__beg == __io.fill()); ++__beg);
                break;
            }
        }

        // Need to get the rest of the sign characters, if they exist.
        if (__sign_size > 1 && __testvalid)
        {
            const auto& __sign = __negative ? info.m_negative_sign
                                            : info.m_positive_sign;
            int __i = 1;
            for (; __beg != __end && __i < __sign_size && *__beg == __sign[__i]; ++__beg, (void)++__i);
            if (__i != __sign_size)
                __testvalid = false;
        }

        bool succ = true;
        if (__testvalid)
        {
            // Strip leading zeros.
            if (__res.size() > 1)
            {
                const auto __first = __res.find_first_not_of('0');
                const bool __only_zeros = __first == std::string::npos;
                if (__first)
                    __res.erase(0, __only_zeros ? __res.size() - 1 : __first);
            }

            // 22.2.6.1.2, p4
            if (__negative && __res[0] != '0')
                __res.insert(__res.begin(), '-');
	    
            // Test for grouping fidelity.
            if (__grouping_tmp.size())
            {
                // Add the ending grouping.
                __grouping_tmp += static_cast<char>(__testdecfound ? __last_pos : __n);
                succ = FacetHelper::verify_grouping(m_grouping, __grouping_tmp);
            }

            // Iff not enough digits were supplied after the decimal-point.
            if (__testdecfound && __n != info.m_frac_digits)
                __testvalid = false;
        }

        // Iff valid sequence is not recognized.
        if (!__testvalid)
            succ = false;
        else
            __units.swap(__res);
        
        return std::pair(succ, __beg);
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