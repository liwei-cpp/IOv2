#pragma once
#include <common/metafunctions.h>
#include <facet/ctype.h>
#include <facet/facet_helper.h>
#include <facet/numeric_details.h>
#include <io/io_base.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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
        if (!p_obj || !p_ctype) throw std::runtime_error("shared_ptr is empty");
        m_decimal_point = p_obj->decimal_point();
        m_thousands_sep = p_obj->thousands_sep();
        m_true_name = p_obj->truename();
        m_false_name = p_obj->falsename();
        // grouping() is contracted to return the INTERNAL convention
        // (1–255 = group size, 0 = stop, last element implicitly repeats).
        // numeric_conf and any user-derived override are both expected to
        // satisfy this contract — POSIX-style normalisation is done at the
        // POSIX boundary in numeric_conf, not here.
        m_grouping = p_obj->grouping();

        // string_view carries the length (no trailing '\0' counted), so the
        // widened range matches m_in_atoms/m_out_atoms exactly. static_assert
        // keeps the source literal and the destination array in lock-step.
        constexpr std::string_view in_atoms = "-+xX0123456789abcdefABCDEF";
        static_assert(in_atoms.size() == std::extent_v<decltype(m_in_atoms)>);
        m_ctype->widen_seq(in_atoms.data(), in_atoms.data() + in_atoms.size(), m_in_atoms);

        constexpr std::string_view out_atoms = "-+xX0123456789abcdef0123456789ABCDEF";
        static_assert(out_atoms.size() == std::extent_v<decltype(m_out_atoms)>);
        m_ctype->widen_seq(out_atoms.data(), out_atoms.data() + out_atoms.size(), m_out_atoms);

        if constexpr (std::is_same_v<CharT, wchar_t> ||
                      std::is_same_v<CharT, char32_t>)
        {
            assert(atoms_pairwise_distinct(m_in_atoms,
                                           std::extent_v<decltype(m_in_atoms)>));
            // m_out_atoms intentionally aliases "0..9" at [4..13] and
            // [20..29] (lowercase / uppercase hex digit slots). Build a
            // 26-position view that skips that overlap before checking
            // distinctness on the semantically distinct output positions.
            CharT out_view[26];
            std::copy(m_out_atoms,      m_out_atoms + 20, out_view);
            std::copy(m_out_atoms + 30, m_out_atoms + 36, out_view + 20);
            assert(atoms_pairwise_distinct(out_view, 26));
        }
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
        fmtflags_guard guard(io);
        io.flags((io.flags() & ~(ios_defs::basefield | ios_defs::uppercase))
                 | (ios_defs::hex | ios_defs::showbase));

        using uintptr_carrier_t = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                     unsigned long,
                                                     unsigned long long>;

        return insert_int(s, io, reinterpret_cast<uintptr_carrier_t>(v));
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
        fmtflags_guard guard(io);
        io.flags((io.flags() & ~ios_defs::basefield) | ios_defs::hex);

        using uintptr_carrier_t = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                     unsigned long,
                                                     unsigned long long>;
        uintptr_carrier_t ul;
        auto [succ, res] = extract_int(beg, end, io, ul);

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
    struct fmtflags_guard
    {
        fmtflags_guard(ios_base<char_type>& i) : m_io(i), m_saved(i.flags()) {}
        ~fmtflags_guard() { m_io.flags(m_saved); }
        fmtflags_guard(const fmtflags_guard&) = delete;
        fmtflags_guard& operator=(const fmtflags_guard&) = delete;
    private:
        ios_base<char_type>& m_io;
        ios_defs::fmtflags m_saved;
    };

    template <typename TIter, typename TValue>
    TIter insert_float(TIter s, ios_base<char_type>& io, TValue v, char mod) const
    {
        // Use default precision if out of range.
        const std::streamsize prec = io.precision() < 0 ? 6 : io.precision();
        const int max_digits = std::numeric_limits<TValue>::digits10;

        // [22.2.2.2.2] Stage 1, numeric conversion to character.
        size_t len = 0;
        char fbuf[16];
        format_float_(io.flags(), fbuf, mod);

        const ios_defs::fmtflags fltfield = io.flags() & ios_defs::floatfield;

        // Initial buffer size estimate (GCC style).
        // For non-fixed fields, max_digits * 3 is a safe heuristic.
        size_t cs_size = static_cast<size_t>(max_digits) * 3 + 32;
        if (fltfield == ios_defs::fixed)
            cs_size = static_cast<size_t>(std::numeric_limits<TValue>::max_exponent10) + static_cast<size_t>(prec) + 32;

        // Cap initial allocation to a reasonable size (e.g., 2048) to avoid huge initial pressure.
        if (cs_size > 2048) cs_size = 2048;

        std::vector<char> vec_cs(cs_size);

        {
            clocale_wrapper inter_locale("C");
            clocale_user guard(inter_locale);

            auto do_snprintf = [&](char* buf, size_t size) {
                if (fltfield == (ios_defs::fixed | ios_defs::scientific))
                    return snprintf(buf, size, fbuf, v);
                else
                    return snprintf(buf, size, fbuf, static_cast<int>(prec), v);
            };

            // snprintf returns int. Trap negative (encoding error) before
            // promoting to size_t — otherwise -1 becomes SIZE_MAX and
            // bypasses every downstream bound check.
            int n = do_snprintf(vec_cs.data(), cs_size);
            if (n < 0)
                throw stream_error("numeric::put fail: floating-point conversion failed");
            len = static_cast<size_t>(n);

            // If buffer was too small, snprintf returns required length.
            if (len >= cs_size)
            {
                cs_size = len + 1;
                vec_cs.resize(cs_size);
                n = do_snprintf(vec_cs.data(), cs_size);
                if (n < 0)
                    throw stream_error("numeric::put fail: floating-point conversion failed");
                len = static_cast<size_t>(n);
            }
        }

        // len == 0 is also treated as failure: every well-formed numeric
        // print (including 0, "inf", "nan") emits at least one character,
        // and the downstream path assumes vec_ws(len) has at least one
        // element before dereferencing vec_ws.data().
        if (len == 0 || len >= cs_size)
            throw stream_error("numeric::put fail: floating-point conversion failed");

        const char* cs = vec_cs.data();

        // [22.2.2.2.2] Stage 2, convert to char_type, using correct
        // numeric_conf.decimal_point() values for '.' and adding grouping.
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
            && (wp || len < 3u || (cs[1] <= '9' && cs[2] <= '9' && cs[1] >= '0' && cs[2] >= '0')))
        {
            // Grouping can add (almost) as many separators as the
            // number of digits, but no more.
            std::vector<CharT> vec_ws2(len * 2);
            CharT* ws2 = vec_ws2.data();

            size_t off = 0;
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
            bool start0x = (ws[0] == m_out_atoms[s_odigits]) && (len > 1u) &&
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
        size_t len = static_cast<size_t>(int_to_char(cs + ilen, u, flags, dec));
        cs += ilen - len;

        // Add grouping, if necessary.
        if (!m_grouping.empty())
        {
            // Grouping can add (almost) as many separators as the number
            // of digits + space is reserved for numeric base or sign.
            std::vector<char_type> cs_vec2((ilen + 1) * 2);
            char_type* cs2 = cs_vec2.data() + 2;
            len = static_cast<size_t>(FacetHelper::add_grouping(cs2, m_thousands_sep, m_grouping, cs, cs + len) - cs2);
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
            bool start0x = (cs[0] == m_out_atoms[s_odigits]) && (len > 1u) &&
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
             char_type* new_buf, const char_type* cs, size_t& len,
             bool startSign, bool start0x) const
    {
      // [22.2.2.2.2] Stage 3.
      // If necessary, pad.
      pad_impl_(adjust, fill, new_buf, cs, w, static_cast<std::streamsize>(len), startSign, start0x);
      len = static_cast<size_t>(w);
    }

    void pad_impl_(ios_defs::fmtflags adjust, char_type fill,
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

    void format_float_(ios_defs::fmtflags flags, char* fptr, char mod) const noexcept
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

    void group_float(const std::vector<uint8_t>& grouping, char_type sep, const char_type* p, char_type* new_buf, char_type* cs, size_t& len) const
    {
        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 282. What types does numpunct grouping refer to?
        // Add grouping, if necessary.
        const size_t declen = p ? static_cast<size_t>(p - cs) : len;
        char_type* p2 = FacetHelper::add_grouping(new_buf, sep, grouping, cs, cs + declen);

        // Tack on decimal part.
        size_t newlen = static_cast<size_t>(p2 - new_buf);
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
            negative = c == m_in_atoms[s_iminus];
            if ((negative || c == m_in_atoms[s_iplus])
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
            else if (c == m_in_atoms[s_izero] && (!found_zero || base == 10))
            {
                found_zero = true;
                ++sep_pos;
                if (basefield == 0) base = 8;
                if (base == 8) sep_pos = 0;
            }
            else if (found_zero && (c == m_in_atoms[s_ix] || c == m_in_atoms[s_iX]))
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
        std::vector<uint8_t> found_grouping;
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
                    // found_grouping stores each group's digit count in a
                    // uint8_t. A single group whose size cannot fit in that
                    // byte is rejected rather than silently truncated.
                    if (sep_pos > std::numeric_limits<uint8_t>::max())
                    {
                        testfail = true;
                        break;
                    }
                    found_grouping.push_back(static_cast<uint8_t>(sep_pos));
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
            // Add the ending grouping. Reject inputs whose final group
            // exceeds the uint8_t storage in found_grouping.
            if (sep_pos > std::numeric_limits<uint8_t>::max())
            {
                testfail = true;
            }
            else
            {
                found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                success = FacetHelper::verify_grouping(m_grouping, found_grouping);
            }
        }

        // LWG 23 (Num_get overflow result): when the field overflows,
        // v must be set to numeric_limits::max() / min() with failbit.
        //
        // testoverflow is checked BEFORE testfail by design. Once the
        // digit-accumulation loop sets testoverflow, ++sep_pos is skipped
        // for every subsequent digit, so sep_pos freezes. A later, otherwise
        // well-formed thousands_sep then fails the grouping check and sets
        // testfail — but that testfail is a side effect of the overflow
        // short-circuit, not an independent structural error in the input.
        //
        // Letting testfail win in that overlap would map a structurally
        // valid, numerically out-of-range input (e.g. "12,345,678,901,234,567"
        // into uint32_t under grouping "\3") to v = 0 instead of v = max,
        // contradicting LWG 23. We diverge from libstdc++'s ordering here
        // (which has the same latent issue) to keep the overflow signal
        // dominant whenever it fires.
        if (testoverflow)
        {
            if (negative && std::is_signed_v<TValue>)
                v = std::numeric_limits<TValue>::min();
            else
                v = std::numeric_limits<TValue>::max();
            success = false;
        }
        else if ((!sep_pos && !found_zero && !found_grouping.size()) || testfail)
        {
            v = 0;
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
        std::vector<uint8_t> found_grouping;
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
                        // found_grouping stores each group's digit count in a
                        // uint8_t. Reject inputs whose group size cannot fit.
                        if (sep_pos > std::numeric_limits<uint8_t>::max())
                        {
                            xtrc.clear();
                            break;
                        }
                        found_grouping.push_back(static_cast<uint8_t>(sep_pos));
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
                    {
                        if (sep_pos > std::numeric_limits<uint8_t>::max())
                        {
                            xtrc.clear();
                            break;
                        }
                        found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                    }
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
                    {
                        if (sep_pos > std::numeric_limits<uint8_t>::max())
                        {
                            xtrc.clear();
                            break;
                        }
                        found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                    }
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
        if (!found_grouping.empty())
        {
            // Add the ending grouping if a decimal or 'e'/'E' wasn't found.
            if (!found_dec && !found_sci)
            {
                if (sep_pos > std::numeric_limits<uint8_t>::max())
                {
                    // Final group exceeds the uint8_t storage in
                    // found_grouping. Fail cleanly rather than truncate.
                    success = false;
                }
                else
                {
                    found_grouping.push_back(static_cast<uint8_t>(sep_pos));
                    success = FacetHelper::verify_grouping(m_grouping, found_grouping);
                }
            }
            else
            {
                success = FacetHelper::verify_grouping(m_grouping, found_grouping);
            }
        }

        return std::pair(success, beg);
    }

    template <typename TValue>
    bool convert_to_v(const char* s, TValue& v) const
    {
        bool res = true;
        char* sanity;

        clocale_wrapper inter_locale("C");
        clocale_user guard(inter_locale);

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

    // Verify ctype::widen is injective on the digit / sign / base / scientific
    // atom set. extract_int / extract_float compare incoming wide characters
    // against entries in m_in_atoms (directly or via std::find) and assume
    // distinct narrow source characters widen to distinct wide values; a
    // collision (e.g. widen('a') == widen('A') in an exotic locale) would
    // make certain digits unreachable on the input side, and insert_int /
    // insert_float would emit indistinguishable glyphs for semantically
    // different positions on the output side. The standard library silently
    // assumes injectivity on this 26-character set; IOv2 catches violations
    // at construction time in debug / coverage / sanitizer builds via the
    // asserts in the constructor. A future unit test can exercise the
    // failure branch by deriving from ctype<CharT> and overriding widen_seq
    // to be intentionally non-injective.
    static bool atoms_pairwise_distinct(const CharT* p, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j)
                if (p[i] == p[j]) return false;
        return true;
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
