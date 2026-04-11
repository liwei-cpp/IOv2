#pragma once
#include <iterator>
#include <memory>
#include <common/metafunctions.h>
#include <facet/ctype.h>
#include <facet/numeric_details.h>
#include <type_traits>

namespace IOv2
{
template <typename CharT>
class numeric
{
public:
    using create_rules = facet_create_rule<facet_create_pack<numeric_conf<CharT>, ctype<CharT>>>;
    using char_type = CharT;

    template <shared_ptr_to<numeric_conf<CharT>> TConfPtr,
              shared_ptr_to<ctype<CharT>> TCtypePtr>
    numeric(TConfPtr p_obj, TCtypePtr p_ctype) : m_ctype(p_ctype)
    {
        avail_ptr(p_obj);
        m_decimal_point = p_obj->decimal_point();
        m_thousands_sep = p_obj->thousands_sep();
        m_true_name = p_obj->truename();
        m_false_name = p_obj->falsename();
        m_grouping = p_obj->grouping();
        adjust_grouping();

        char in_atoms_c[] = "-+xX0123456789abcdefABCDEF";
        m_ctype->widen_seq(in_atoms_c, in_atoms_c + 26, m_in_atoms);

        char out_atoms_c[] = "-+xX0123456789abcdef0123456789ABCDEF";
        m_ctype->widen_seq(out_atoms_c, out_atoms_c + 36, m_out_atoms);
    }

public:
    CharT decimal_point() const noexcept { return m_decimal_point; }
    CharT thousands_sep() const noexcept { return m_thousands_sep; }
    const std::basic_string<CharT>& truename() const noexcept { return m_true_name; }
    const std::basic_string<CharT>& falsename() const noexcept { return m_false_name; }
    const std::vector<uint8_t>& grouping() const noexcept { return m_grouping; }

public:
    template <typename TIter>
    TIter put(TIter s, ios_base<char_type>& io, bool v) const
    {
        const auto flags = io.flags();
        if ((flags & ios_defs::boolalpha) == 0)
        {
            return insert_int(s, io, static_cast<const long>(v));
        }
        
        const auto& name = v ? m_true_name : m_false_name;
        size_t len = name.size();

        const auto w = io.width();
        if (w > static_cast<decltype(w)>(len))
        {
            const auto plen = w - len;
            std::vector<char_type> ps(plen, io.fill());
            io.width(0);

            if ((flags & ios_defs::adjustfield) == ios_defs::left)
            {
                s = std::copy(name.begin(), name.end(), s);
                s = std::fill_n(s, plen, io.fill());
            }
            else
            {
                s = std::fill_n(s, plen, io.fill());
                s = std::copy(name.begin(), name.end(), s);
            }
            return s;
        }
        
        io.width(0);
        return std::copy(name.begin(), name.end(), s);
    }
    
    template <typename TIter, typename TValue>
        requires (std::is_integral_v<TValue> && (!std::is_same_v<TValue, bool>))
    TIter put(TIter s, ios_base<char_type>& io, TValue v) const { return insert_int(s, io, v); }

    template <typename TIter, typename TValue>
        requires (std::is_floating_point_v<TValue>)
    TIter put(TIter s, ios_base<char_type>& io, TValue v) const
    {
        if constexpr (std::is_same_v<TValue, long double>)
            return insert_float(s, io, v, 'L');
        else
            return insert_float(s, io, v, char());
    }

    template <typename TIter>
    TIter put(TIter s, ios_base<char_type>& io, const void* v) const
    {
        const ios_defs::fmtflags flags = io.flags();
        const ios_defs::fmtflags fmt = ~(ios_defs::basefield | ios_defs::uppercase);
        io.flags((flags & fmt) | (ios_defs::hex | ios_defs::showbase));

        using _UIntPtrType = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                unsigned long,
                                                unsigned long long>;

        s = insert_int(s, io, reinterpret_cast<_UIntPtrType>(v));
        io.flags(flags);
        return s;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, bool& v) const
    {
        bool success = true;

        if (!(io.flags() & ios_defs::boolalpha))
        {
            // Parse bool values as long.
            long l = -1;
            std::tie(success, beg) = extract_int(beg, end, io, l);

            if (l == 0 || l == 1) v = bool(l);
            else
            {
                // _GLIBCXX_RESOLVE_LIB_DEFECTS
                // 23. Num_get overflow result.
                v = true;
                success = false;
            }
        }
        else
        {
            // Parse bool values as alphanumeric.
            bool testf = true;
            bool testt = true;
            bool donef = (m_false_name.empty());
            bool donet = (m_true_name.empty());
            size_t n = 0;
            while (!donef || !donet)
            {
                if (beg == end)
                    break;

                const CharT c = *beg;

                if (!donef)
                    testf = c == m_false_name[n];

                if (!testf && donet) break;

                if (!donet)
                    testt = c == m_true_name[n];

                if (!testt && donef) break;

                if (!testt && !testf) break;

                ++n;
                ++beg;

                donef = !testf || n >= m_false_name.size();
                donet = !testt || n >= m_true_name.size();
            }
            if (testf && n == m_false_name.size() && n)
            {
                v = false;
                if (testt && n == m_true_name.size())
                    success = false;
            }
            else if (testt && n == m_true_name.size() && n)
            {
                v = true;
            }
            else
            {
                // _GLIBCXX_RESOLVE_LIB_DEFECTS
                // 23. Num_get overflow result.
                v = false;
                success = false;
            }
        }

        if (!success) throw stream_error("numeric::get fail: parse boolean fail");
        return beg;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
        requires (std::is_integral_v<TValue> && (!std::is_same_v<TValue, bool>))
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, TValue& v) const
    {
        auto [succ, res] = extract_int(beg, end, io, v);
        if (!succ) throw stream_error("numeric::get fail: parse integral fail");
        return res;
    }

    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, void*& v) const
    {
        // Prepare for hex formatted input.
        const auto fmt = io.flags();
        io.flags((fmt & ~ios_defs::basefield) | ios_defs::hex);

        using _UIntPtrType = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                unsigned long,
                                                unsigned long long>;
        _UIntPtrType ul;
        auto [succ, res] = extract_int(beg, end, io, ul);

        // Reset from hex formatted input.
        io.flags(fmt);

        v = reinterpret_cast<void*>(ul);

        if (!succ) throw stream_error("numeric::get fail: parse address fail");
        return res;
    }

    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
        requires (std::is_floating_point_v<TValue>)
    TIter get(TIter beg, TSent end, ios_base<char_type>& io, TValue& v) const
    {
        std::string xtrc;
        xtrc.reserve(32);
        auto [succ, res] = extract_float(beg, end, io, xtrc);
        succ &= convert_to_v(xtrc.c_str(), v);

        if (!succ) throw stream_error("numeric::get fail: parse float fail");
        return res;
    }

private:
    template <typename TIter, typename TValue>
    TIter insert_float(TIter s, ios_base<char_type>& io, TValue v, char mod) const
    {
        // Use default precision if out of range.
        const std::streamsize prec = io.precision() < 0 ? 6 : io.precision();
        const int max_digits = std::numeric_limits<TValue>::digits10;

        // [22.2.2.2.2] Stage 1, numeric conversion to character.
        int len;
        // Long enough for the max format spec.
        char fbuf[16];
        _S_format_float(io.flags(), fbuf, mod);

        // Consider the possibility of long ios_base::fixed outputs
        const bool fixed = io.flags() & ios_defs::fixed;
        const int max_exp = std::numeric_limits<TValue>::max_exponent10;

        // The size of the output string is computed as follows.
        // ios_base::fixed outputs may need up to max_exp + 1 chars
        // for the integer part + prec chars for the fractional part
        // + 3 chars for sign, decimal point, '\0'. On the other hand,
        // for non-fixed outputs max_digits * 2 + prec chars are
        // largely sufficient.
        const int cs_size = fixed ? max_exp + prec + 4
                                  : max_digits * 2 + prec;
        std::vector<char> vec_cs(cs_size);
        char* cs = vec_cs.data();

        {
            clocale_wrapper inter_locale("C");
            clocale_user guard(inter_locale.c_locale);
            len = sprintf(cs, fbuf, prec, v);
        }

        // [22.2.2.2.2] Stage 2, convert to char_type, using correct
        // numpunct.decimal_point() values for '.' and adding grouping.
        std::vector<CharT> vec_ws(len);
        CharT* ws = vec_ws.data();
        m_ctype->widen_seq(cs, cs + len, ws);

        // Replace decimal point.
        const char* p = std::find(cs, cs + len, '.');
        CharT* wp = 0;
        if (p != cs + len)
        {
            wp = ws + (p - cs);
            *wp = m_decimal_point;
        }

        // Add grouping, if necessary.
        if ((!m_grouping.empty())
            && (wp || len < 3 || (cs[1] <= '9' && cs[2] <= '9' && cs[1] >= '0' && cs[2] >= '0')))
        {
            // Grouping can add (almost) as many separators as the
            // number of digits, but no more.
            std::vector<CharT> vec_ws2(len * 2);
            CharT* ws2 = vec_ws2.data();

            std::streamsize off = 0;
            if (cs[0] == '-' || cs[0] == '+')
            {
                off = 1;
                ws2[0] = ws[0];
                len -= 1;
            }

            group_float(m_grouping, m_thousands_sep, wp,
                        ws2 + off, ws + off, len);
            len += off;
            std::swap(vec_ws, vec_ws2);
            ws = vec_ws.data();
        }

        // Pad.
        const std::streamsize w = io.width();
        if (w > static_cast<std::streamsize>(len))
        {
            std::vector<CharT> vec_ws3(w);
            CharT* ws3 = vec_ws3.data();
            bool startSign = (ws[0] == m_out_atoms[s_ominus]) || (ws[0] == m_out_atoms[s_oplus]);
            bool start0x = (ws[0] == m_out_atoms[s_odigits]) && (len > 1) &&
                           ((ws[1] == m_out_atoms[s_ox]) || (ws[1] == m_out_atoms[s_oX]));
            const ios_defs::fmtflags adjust = io.flags() & ios_defs::adjustfield;
            pad(io.fill(), w, adjust, ws3, ws, len, startSign, start0x);

            std::swap(vec_ws, vec_ws3);
            ws = vec_ws.data();
        }
        io.width(0);

        // [22.2.2.2.2] Stage 4.
        // Write resulting, fully-formatted string to output iterator.
        return std::copy(ws, ws + len, s);
    }
    
    template <typename TIter, typename TValue>
    TIter insert_int(TIter s, ios_base<char_type>& io, TValue v) const
    {
        using unsigned_type = std::make_unsigned_t<TValue>;

        const ios_defs::fmtflags flags = io.flags();

        // Long enough to hold hex, dec, and octal representations.
        const auto ilen = 5 * sizeof(TValue);
        std::vector<char_type> cs_vec(ilen);
        char_type* cs = cs_vec.data();

        // [22.2.2.2.2] Stage 1, numeric conversion to character.
        // Result is returned right-justified in the buffer.
        const ios_defs::fmtflags basefield = flags & ios_defs::basefield;
        const bool dec = (basefield != ios_defs::oct && basefield != ios_defs::hex);
        const unsigned_type u = ((v > 0 || !dec) ? unsigned_type(v)
                                                 : -unsigned_type(v));
        auto len = int_to_char(cs + ilen, u, flags, dec);
        cs += ilen - len;

        // Add grouping, if necessary.
        if (!m_grouping.empty())
        {
            // Grouping can add (almost) as many separators as the number
            // of digits + space is reserved for numeric base or sign.
            std::vector<char_type> cs_vec2((ilen + 1) * 2);
            char_type* cs2 = cs_vec2.data() + 2;
            len = FacetHelper::add_grouping(cs2, m_thousands_sep, m_grouping, cs, cs + len) - cs2;
            std::swap(cs_vec, cs_vec2);
            cs = cs_vec.data() + 2;
        }

        // Complete Stage 1, prepend numeric base or sign.
        if (dec)
        { // Decimal.
            if (v >= 0)
            {
                if (bool(flags & ios_defs::showpos) && std::is_signed_v<TValue>)
                    *--cs = m_out_atoms[s_oplus], ++len;
            }
            else
                *--cs = m_out_atoms[s_ominus], ++len;
        }
        else if (bool(flags & ios_defs::showbase) && v)
        {
            if (basefield == ios_defs::oct)
                *--cs = m_out_atoms[s_odigits], ++len;
            else
            {
                // 'x' or 'X'
                const bool uppercase = flags & ios_defs::uppercase;
                *--cs = m_out_atoms[s_ox + uppercase];
                // '0'
                *--cs = m_out_atoms[s_odigits];
                    len += 2;
            }
        }

        // Pad.
        const std::streamsize w = io.width();
        if (w > static_cast<std::streamsize>(len))
        {
            std::vector<char_type> cs_vec3(w);
            char_type* cs3 = cs_vec3.data();
            bool startSign = (cs[0] == m_out_atoms[s_ominus]) || (cs[0] == m_out_atoms[s_oplus]);
            bool start0x = (cs[0] == m_out_atoms[s_odigits]) && (len > 1) &&
                           ((cs[1] == m_out_atoms[s_ox]) || (cs[1] == m_out_atoms[s_oX]));
            const ios_defs::fmtflags adjust = io.flags() & ios_defs::adjustfield;
            pad(io.fill(), w, adjust, cs3, cs, len, startSign, start0x);
            std::swap(cs_vec, cs_vec3);
            cs = cs_vec.data();
        }
        io.width(0);

        // [22.2.2.2.2] Stage 4.
        // Write resulting, fully-formatted string to output iterator.
        return std::copy(cs, cs + len, s);
    }
    
    template <typename TValue>
    int int_to_char(CharT* bufend, TValue v, ios_defs::fmtflags flags, bool dec) const
    {
        auto buf = bufend;
        if (dec)
        {   // Decimal.
            do
            {
                *--buf = m_out_atoms[(v % 10) + s_odigits];
                v /= 10;
            } while (v != 0);
        }
        else if ((flags & ios_defs::basefield) == ios_defs::oct)
        { // Octal.
            do
            {
                *--buf = m_out_atoms[(v & 0x7) + s_odigits];
                v >>= 3;
            } while (v != 0);
        }
        else
        {   // Hex.
            const bool uppercase = flags & ios_defs::uppercase;
            const int case_offset = uppercase ? s_oudigits : s_odigits;
            do
            {
                *--buf = m_out_atoms[(v & 0xf) + case_offset];
                v >>= 4;
            } while (v != 0);
        }
        return bufend - buf;
    }

    void pad(char_type fill, std::streamsize w, ios_defs::fmtflags adjust,
             char_type* new_buf, const char_type* cs, int& len,
             bool startSign, bool start0x) const
    {
      // [22.2.2.2.2] Stage 3.
      // If necessary, pad.
      _S_pad(adjust, fill, new_buf, cs, w, len, startSign, start0x);
      len = static_cast<int>(w);
    }

    void _S_pad(ios_defs::fmtflags adjust, char_type fill,
                char_type* news, const char_type* olds,
                std::streamsize newlen, std::streamsize oldlen,
                bool startSign, bool start0x) const
    {

        const size_t plen = static_cast<size_t>(newlen - oldlen);

        // Padding last.
        if (adjust == ios_defs::left)
        {
            std::copy(olds, olds + oldlen, news);
            std::fill_n(news + oldlen, plen, fill);
            return;
        }

        size_t mod = 0;
        if (adjust == ios_defs::internal)
        {
            // Pad after the sign, if there is one.
            // Pad after 0[xX], if there is one.
            // Who came up with these rules, anyway? Jeeze.
            if (startSign)
            {
                news[0] = olds[0];
                mod = 1;
                ++news;
            }
            else if (start0x)
            {
                news[0] = olds[0];
                news[1] = olds[1];
                mod = 2;
                news += 2;
            }
            // else Padding first.
        }
        std::fill_n(news, plen, fill);
        std::copy(olds + mod, olds + oldlen, news + plen);
    }
    
    void _S_format_float(ios_defs::fmtflags flags, char* fptr, char mod) const noexcept
    {
        *fptr++ = '%';
        // [22.2.2.2.2] Table 60
        if (flags & ios_defs::showpos)
            *fptr++ = '+';
        if (flags & ios_defs::showpoint)
            *fptr++ = '#';

        ios_defs::fmtflags fltfield = flags & ios_defs::floatfield;
        if (fltfield != (ios_defs::fixed | ios_defs::scientific))
        {
            // As per DR 231: not only when flags & ios_base::fixed || prec > 0
            *fptr++ = '.';
            *fptr++ = '*';
        }

        if (mod)
            *fptr++ = mod;
        // [22.2.2.2.2] Table 58
        if (fltfield == ios_defs::fixed)
            *fptr++ = 'f';
        else if (fltfield == ios_defs::scientific)
            *fptr++ = (flags & ios_defs::uppercase) ? 'E' : 'e';
        else if (fltfield == (ios_defs::fixed | ios_defs::scientific))
            *fptr++ = (flags & ios_defs::uppercase) ? 'A' : 'a';
        else
            *fptr++ = (flags & ios_defs::uppercase) ? 'G' : 'g';
        *fptr = '\0';
    }
    
    void group_float(const std::vector<uint8_t>& grouping, char_type sep, const char_type* p, char_type* new_buf, char_type* cs, int& len) const
    {
        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 282. What types does numpunct grouping refer to?
        // Add grouping, if necessary.
        const int declen = p ? p - cs : len;
        char_type* p2 = FacetHelper::add_grouping(new_buf, sep, grouping, cs, cs + declen);

        // Tack on decimal part.
        int newlen = p2 - new_buf;
        if (p)
        {
            std::copy(p, p + len - declen, p2);
            newlen += len - declen;
        }
        len = newlen;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
    std::pair<bool, TIter> extract_int(TIter beg, TSent end, ios_base<char_type>& io, TValue& v) const
    {
        using unsigned_type = std::make_unsigned_t<TValue>;

        CharT c{};

        // NB: Iff basefield == 0, base can change based on contents.
        const ios_defs::fmtflags basefield = io.flags() & ios_defs::basefield;
        const bool oct = basefield == ios_defs::oct;
        int base = oct ? 8 : (basefield == ios_defs::hex ? 16 : 10);

        // True if beg becomes equal to end.
        bool testeof = beg == end;

        // First check for sign.
        bool negative = false;
        if (!testeof)
        {
            c = *beg;
            negative = c == m_in_atoms[s_ominus];
            if ((negative || c == m_in_atoms[s_oplus])
                && (m_grouping.empty() || c != m_thousands_sep)
                && (c != m_decimal_point))
            {
                if (++beg != end)
                    c = *beg;
                else
                    testeof = true;
            }
        }

        // Next, look for leading zeros and check required digits
        // for base formats.
        bool found_zero = false;
        int sep_pos = 0;
        while (!testeof)
        {
            if ((!m_grouping.empty() && c == m_thousands_sep)
                || c == m_decimal_point)
                break;
            else if (c == m_in_atoms[s_odigits] && (!found_zero || base == 10))
            {
                found_zero = true;
                ++sep_pos;
                if (basefield == 0) base = 8;
                if (base == 8) sep_pos = 0;
            }
            else if (found_zero && (c == m_in_atoms[s_ox] || c == m_in_atoms[s_oX]))
            {
                if (basefield == 0) base = 16;
                if (base == 16)
                {
                    found_zero = false;
                    sep_pos = 0;
                }
                else
                    break;
            }
            else
                break;

            if (++beg != end)
            {
                c = *beg;
                if (!found_zero) break;
            }
            else
                testeof = true;
        }

        // At this point, base is determined. If not hex, only allow
        // base digits as valid input.
        const size_t len = (base == 16 ? s_iend - s_izero : base);

        // Extract.
        std::string found_grouping;
        if (!m_grouping.empty()) found_grouping.reserve(32);
        bool testfail = false;
        bool testoverflow = false;
        const unsigned_type max_val = (negative && std::is_signed_v<TValue>)
                                        ? -static_cast<unsigned_type>(std::numeric_limits<TValue>::min()) : std::numeric_limits<TValue>::max();
        const unsigned_type smax = max_val / base;
        unsigned_type result = 0;
        int digit = 0;
        const char_type* lit_zero = m_in_atoms + s_izero;

        while (!testeof)
        {
            // According to 22.2.2.1.2, p8-9, first look for thousands_sep
            // and decimal_point.
            if (!m_grouping.empty() && c == m_thousands_sep)
            {
                // NB: Thousands separator at the beginning of a string
                // is a no-no, as is two consecutive thousands separators.
                if (sep_pos)
                {
                    found_grouping += static_cast<char>(sep_pos);
                    sep_pos = 0;
                }
                else
                {
                    testfail = true;
                    break;
                }
            }
            else if (c == m_decimal_point) break;
            else
            {
                const char_type* q = std::find(lit_zero, lit_zero + len, c);
                if (q == lit_zero + len) break;

                digit = q - lit_zero;
                if (digit > 15) digit -= 6;
                if (result > smax) testoverflow = true;
                else
                {
                    result *= base;
                    testoverflow |= result > max_val - digit;
                    result += digit;
                    ++sep_pos;
                }
            }

            if (++beg != end) c = *beg;
            else testeof = true;
        }

        bool success = true;
        // Digit grouping is checked. If grouping and found_grouping don't
        // match, then get very very upset, and set failbit.
        if (!found_grouping.empty())
        {
            // Add the ending grouping.
            found_grouping += static_cast<char>(sep_pos);

            success = FacetHelper::verify_grouping(m_grouping, found_grouping);
        }

        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 23. Num_get overflow result.
        if ((!sep_pos && !found_zero && !found_grouping.size()) || testfail)
        {
            v = 0;
            success = false;
        }
        else if (testoverflow)
        {
            if (negative && std::is_signed_v<TValue>)
                v = std::numeric_limits<TValue>::min();
            else
                v = std::numeric_limits<TValue>::max();
            success = false;
        }
        else
            v = negative ? -result : result;

        return std::pair(success, beg);
    }

    template <typename TIter, std::sentinel_for<TIter> TSent>
    std::pair<bool, TIter> extract_float(TIter beg, TSent end, ios_base<char_type>& io, std::string& xtrc) const
    {
        char_type c = char_type();

        // True if beg becomes equal to end.
        bool testeof = beg == end;

        // First check for sign.
        if (!testeof)
        {
            c = *beg;
            const bool plus = c == m_in_atoms[s_iplus];
            if ((plus || c == m_in_atoms[s_iminus])
                && (m_grouping.empty() || c != m_thousands_sep)
                && (c != m_decimal_point))
            {
                xtrc += plus ? '+' : '-';
                if (++beg != end) c = *beg;
                else
                    testeof = true;
            }
        }

        // Next, look for leading zeros.
        bool found_mantissa = false;
        int sep_pos = 0;
        while (!testeof)
        {
            if ((!m_grouping.empty() && c == m_thousands_sep) || c == m_decimal_point)
                break;
            else if (c == m_in_atoms[s_izero])
            {
                if (!found_mantissa)
                {
                    xtrc += '0';
                    found_mantissa = true;
                }
                ++sep_pos;

                if (++beg != end)
                    c = *beg;
                else
                    testeof = true;
            }
            else
                break;
        }

        // Only need acceptable digits for floating point numbers.
        bool found_dec = false;
        bool found_sci = false;
        std::string found_grouping;
        if (!m_grouping.empty())
            found_grouping.reserve(32);
        const char_type* lit_zero = m_in_atoms + s_izero;

        while (!testeof)
        {
            // According to 22.2.2.1.2, p8-9, first look for thousands_sep
            // and decimal_point.
            if (!m_grouping.empty() && c == m_thousands_sep)
            {
                if (!found_dec && !found_sci)
                {
                    // NB: Thousands separator at the beginning of a string
                    // is a no-no, as is two consecutive thousands separators.
                    if (sep_pos)
                    {
                        found_grouping += static_cast<char>(sep_pos);
                        sep_pos = 0;
                    }
                    else
                    {
                        // NB: convert_to_v will not assign v and will
                        // set the failbit.
                        xtrc.clear();
                        break;
                    }
                }
                else
                    break;
            }
            else if (c == m_decimal_point)
            {
                if (!found_dec && !found_sci)
                {
                    // If no grouping chars are seen, no grouping check
                    // is applied. Therefore found_grouping is adjusted
                    // only if decimal_point comes after some thousands_sep.
                    if (found_grouping.size())
                        found_grouping += static_cast<char>(sep_pos);
                    xtrc += '.';
                    found_dec = true;
                }
                else
                    break;
            }
            else
            {
                const char_type* q = std::find(lit_zero, lit_zero + 10, c);
                if (q != lit_zero + 10)
                {
                    xtrc += '0' + (q - lit_zero);
                    found_mantissa = true;
                    ++sep_pos;
                }
                else if ((c == m_in_atoms[s_ie] || c == m_in_atoms[s_iE])
                        && !found_sci && found_mantissa)
                {
                    // Scientific notation.
                    if (found_grouping.size() && !found_dec)
                        found_grouping += static_cast<char>(sep_pos);
                    xtrc += 'e';
                    found_sci = true;

                    // Remove optional plus or minus sign, if they exist.
                    if (++beg != end)
                    {
                        c = *beg;
                        const bool plus = c == m_in_atoms[s_iplus];
                        if ((plus || c == m_in_atoms[s_iminus])
                            && (m_grouping.empty() || c != m_thousands_sep)
                            && (c != m_decimal_point))
                            xtrc += plus ? '+' : '-';
                        else
                            continue;
                    }
                    else
                    {
                        testeof = true;
                        break;
                    }
                }
                else
                    break;
            }

            if (++beg != end)
                c = *beg;
            else
                testeof = true;
        }

        bool success = true;
        // Digit grouping is checked. If grouping and found_grouping don't
        // match, then get very very upset, and set failbit.
        if (found_grouping.size())
        {
            // Add the ending grouping if a decimal or 'e'/'E' wasn't found.
            if (!found_dec && !found_sci)
                found_grouping += static_cast<char>(sep_pos);

            success = FacetHelper::verify_grouping(m_grouping, found_grouping);
        }

        return std::pair(success, beg);
    }

    template <typename TValue>
    bool convert_to_v(const char* s, TValue& v) const noexcept
    {
        bool res = true;
        char* sanity;

        clocale_wrapper inter_locale("C");
        clocale_user guard(inter_locale.c_locale);

        if constexpr (std::is_same_v<TValue, float>)
            v = strtof(s, &sanity);
        else if constexpr (std::is_same_v<TValue, double>)
            v = strtod(s, &sanity);
        else
            v = strtold(s, &sanity);

        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 23. Num_get overflow result.
        if (sanity == s || *sanity != '\0')
        {
            v = 0.0l;
            res = false;
        }
        else if (v == std::numeric_limits<TValue>::infinity())
        {
            v = std::numeric_limits<TValue>::max();
            res = false;
        }
        else if (v == -std::numeric_limits<TValue>::infinity())
        {
            v = -std::numeric_limits<TValue>::max();
            res = false;
        }
        return res;
    }

private:
    // this trys to solve the case: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=39168
    void adjust_grouping()
    {
        if ((!m_grouping.empty()) && 
            (m_grouping[0] > 0) &&
            (m_grouping[0] < std::numeric_limits<char>::max()))
            return;
        else
            m_grouping.clear();
    }

private:
    std::shared_ptr<const ctype<CharT>> m_ctype;
    CharT m_decimal_point;
    CharT m_thousands_sep;
    std::basic_string<CharT>  m_true_name;
    std::basic_string<CharT>  m_false_name;
    std::vector<uint8_t>      m_grouping;
    
private:
    char_type m_in_atoms[26];
    char_type m_out_atoms[36];
    static constexpr int s_ominus       = 0;
    static constexpr int s_oplus        = 1;
    static constexpr int s_ox           = 2;
    static constexpr int s_oX           = 3;
    static constexpr int s_odigits      = 4;
    static constexpr int s_oudigits     = s_odigits + 16;
    static constexpr int s_oa           = s_odigits + 10;
    static constexpr int s_oe           = s_odigits + 14;
    static constexpr int s_oA           = s_oudigits + 10;
    static constexpr int s_oE           = s_oudigits + 14;
    
    static constexpr int s_iminus       = 0;
    static constexpr int s_iplus        = 1;
    static constexpr int s_ix           = 2;
    static constexpr int s_iX           = 3;
    static constexpr int s_izero        = 4;
    static constexpr int s_ie           = s_izero + 14;
    static constexpr int s_iE           = s_izero + 20;
    static constexpr int s_iend         = 26;
};

template<typename TConfPtr, typename TCtypePtr>
    requires (std::is_same_v<typename TConfPtr::element_type::char_type,
                             typename TCtypePtr::element_type::char_type>)
numeric(TConfPtr, TCtypePtr) -> numeric<typename TConfPtr::element_type::char_type>;
}