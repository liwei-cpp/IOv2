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

        char __in_atoms_c[] = "-+xX0123456789abcdefABCDEF";
        m_ctype->widen_seq(__in_atoms_c, __in_atoms_c + 26, m_in_atoms);

        char __out_atoms_c[] = "-+xX0123456789abcdef0123456789ABCDEF";
        m_ctype->widen_seq(__out_atoms_c, __out_atoms_c + 36, m_out_atoms);
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
    TIter put(TIter __s, ios_base<char_type>& __io, TValue __v) const { return insert_int(__s, __io, __v); }
    
    template <typename TIter, typename TValue>
        requires (std::is_floating_point_v<TValue>)
    TIter put(TIter __s, ios_base<char_type>& __io, TValue __v) const
    {
        if constexpr (std::is_same_v<TValue, long double>)
            return insert_float(__s, __io, __v, 'L');
        else
            return insert_float(__s, __io, __v, char());
    }

    template <typename TIter>
    TIter put(TIter __s, ios_base<char_type>& __io, const void* __v) const
    {
        const ios_defs::fmtflags __flags = __io.flags();
        const ios_defs::fmtflags __fmt = ~(ios_defs::basefield | ios_defs::uppercase);
        __io.flags((__flags & __fmt) | (ios_defs::hex | ios_defs::showbase));

        using _UIntPtrType = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                unsigned long,
                                                unsigned long long>;

        __s = insert_int(__s, __io, reinterpret_cast<_UIntPtrType>(__v));
        __io.flags(__flags);
        return __s;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter __beg, TSent __end, ios_base<char_type>& __io, bool& __v) const
    {
        bool success = true;
        
        if (!(__io.flags() & ios_defs::boolalpha))
        {
            // Parse bool values as long.
            long __l = -1;
            std::tie(success, __beg) = extract_int(__beg, __end, __io, __l);

            if (__l == 0 || __l == 1) __v = bool(__l);
            else
            {
                // _GLIBCXX_RESOLVE_LIB_DEFECTS
                // 23. Num_get overflow result.
                __v = true;
                success = false;
            }
        }
        else
        {
            // Parse bool values as alphanumeric.
            bool __testf = true;
            bool __testt = true;
            bool __donef = (m_false_name.empty());
            bool __donet = (m_true_name.empty());
            size_t __n = 0;
            while (!__donef || !__donet)
            {
                if (__beg == __end)
                    break;

                const CharT __c = *__beg;

                if (!__donef)
                    __testf = __c == m_false_name[__n];

                if (!__testf && __donet) break;

                if (!__donet)
                    __testt = __c == m_true_name[__n];

                if (!__testt && __donef) break;

                if (!__testt && !__testf) break;

                ++__n;
                ++__beg;

                __donef = !__testf || __n >= m_false_name.size();
                __donet = !__testt || __n >= m_true_name.size();
            }
            if (__testf && __n == m_false_name.size() && __n)
            {
                __v = false;
                if (__testt && __n == m_true_name.size())
                    success = false;
            }
            else if (__testt && __n == m_true_name.size() && __n)
            {
                __v = true;
            }
            else
            {
                // _GLIBCXX_RESOLVE_LIB_DEFECTS
                // 23. Num_get overflow result.
                __v = false;
                success = false;
            }
        }
        
        if (!success) throw stream_error("numeric::get fail: parse boolean fail");
        return __beg;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
        requires (std::is_integral_v<TValue> && (!std::is_same_v<TValue, bool>))
    TIter get(TIter __beg, TSent __end, ios_base<char_type>& __io, TValue& __v) const
    {
        auto [succ, res] = extract_int(__beg, __end, __io, __v);
        if (!succ) throw stream_error("numeric::get fail: parse integral fail");
        return res;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent>
    TIter get(TIter __beg, TSent __end, ios_base<char_type>& __io, void*& __v) const
    {
        // Prepare for hex formatted input.
        const auto __fmt = __io.flags();
        __io.flags((__fmt & ~ios_defs::basefield) | ios_defs::hex);

        using _UIntPtrType = std::conditional_t<(sizeof(const void*) <= sizeof(unsigned long)),
                                                unsigned long,
                                                unsigned long long>;
        _UIntPtrType __ul;
        auto [succ, res] = extract_int(__beg, __end, __io, __ul);

        // Reset from hex formatted input.
        __io.flags(__fmt);

        __v = reinterpret_cast<void*>(__ul);

        if (!succ) throw stream_error("numeric::get fail: parse address fail");
        return res;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
        requires (std::is_floating_point_v<TValue>)
    TIter get(TIter __beg, TSent __end, ios_base<char_type>& __io, TValue& __v) const
    {
        std::string __xtrc;
        __xtrc.reserve(32);
        auto [succ, __res] = extract_float(__beg, __end, __io, __xtrc);
        succ &= convert_to_v(__xtrc.c_str(), __v);
        
        if (!succ) throw stream_error("numeric::get fail: parse float fail");
        return __res;
    }

private:
    template <typename TIter, typename TValue>
    TIter insert_float(TIter __s, ios_base<char_type>& __io, TValue __v, char __mod) const
    {
        // Use default precision if out of range.
        const std::streamsize __prec = __io.precision() < 0 ? 6 : __io.precision();
        const int __max_digits = std::numeric_limits<TValue>::digits10;
        
        // [22.2.2.2.2] Stage 1, numeric conversion to character.
        int __len;
        // Long enough for the max format spec.
        char __fbuf[16];
        _S_format_float(__io.flags(), __fbuf, __mod);
        
        // Consider the possibility of long ios_base::fixed outputs
        const bool __fixed = __io.flags() & ios_defs::fixed;
        const int __max_exp = std::numeric_limits<TValue>::max_exponent10;

        // The size of the output string is computed as follows.
        // ios_base::fixed outputs may need up to __max_exp + 1 chars
        // for the integer part + __prec chars for the fractional part
        // + 3 chars for sign, decimal point, '\0'. On the other hand,
        // for non-fixed outputs __max_digits * 2 + __prec chars are
        // largely sufficient.
        const int __cs_size = __fixed ? __max_exp + __prec + 4
                                      : __max_digits * 2 + __prec;
        std::vector<char> vec_cs(__cs_size);
        char* __cs = vec_cs.data();
        
        {
            clocale_wrapper inter_locale("C");
            clocale_user guard(inter_locale.c_locale);
            __len = sprintf(__cs, __fbuf, __prec, __v);
        }
        
        // [22.2.2.2.2] Stage 2, convert to char_type, using correct
        // numpunct.decimal_point() values for '.' and adding grouping.
        std::vector<CharT> vec_ws(__len);
        CharT* __ws = vec_ws.data();
        m_ctype->widen_seq(__cs, __cs + __len, __ws);

        // Replace decimal point.
        const char* __p = std::find(__cs, __cs + __len, '.');
        CharT* __wp = 0;
        if (__p != __cs + __len)
        {
            __wp = __ws + (__p - __cs);
            *__wp = m_decimal_point;
        }
        
        // Add grouping, if necessary.
        if ((!m_grouping.empty())
            && (__wp || __len < 3 || (__cs[1] <= '9' && __cs[2] <= '9' && __cs[1] >= '0' && __cs[2] >= '0')))
        {
            // Grouping can add (almost) as many separators as the
            // number of digits, but no more.
            std::vector<CharT> vec_ws2(__len * 2);
            CharT* __ws2 = vec_ws2.data();

            std::streamsize __off = 0;
            if (__cs[0] == '-' || __cs[0] == '+')
            {
                __off = 1;
                __ws2[0] = __ws[0];
                __len -= 1;
            }

            group_float(m_grouping, m_thousands_sep, __wp,
                        __ws2 + __off, __ws + __off, __len);
            __len += __off;
            std::swap(vec_ws, vec_ws2);
            __ws = vec_ws.data();
        }
        
        // Pad.
        const std::streamsize __w = __io.width();
        if (__w > static_cast<std::streamsize>(__len))
        {
            std::vector<CharT> vec_ws3(__w);
            CharT* __ws3 = vec_ws3.data();
            bool startSign = (__ws[0] == m_out_atoms[s_ominus]) || (__ws[0] == m_out_atoms[s_oplus]);
            bool start0x = (__ws[0] == m_out_atoms[s_odigits]) && (__len > 1) && 
                           ((__ws[1] == m_out_atoms[s_ox]) || (__ws[1] == m_out_atoms[s_oX]));
            const ios_defs::fmtflags __adjust = __io.flags() & ios_defs::adjustfield;
            pad(__io.fill(), __w, __adjust, __ws3, __ws, __len, startSign, start0x);
            
            std::swap(vec_ws, vec_ws3);
            __ws = vec_ws.data();
        }
        __io.width(0);

        // [22.2.2.2.2] Stage 4.
        // Write resulting, fully-formatted string to output iterator.
        return std::copy(__ws, __ws + __len, __s);
    }
    
    template <typename TIter, typename TValue>
    TIter insert_int(TIter __s, ios_base<char_type>& __io, TValue __v) const
    {
        using __unsigned_type = std::make_unsigned_t<TValue>;
        
        const ios_defs::fmtflags __flags = __io.flags();

        // Long enough to hold hex, dec, and octal representations.
        const auto __ilen = 5 * sizeof(TValue);
        std::vector<char_type> cs_vec(__ilen);
        char_type* __cs = cs_vec.data();

        // [22.2.2.2.2] Stage 1, numeric conversion to character.
        // Result is returned right-justified in the buffer.
        const ios_defs::fmtflags __basefield = __flags & ios_defs::basefield;
        const bool __dec = (__basefield != ios_defs::oct && __basefield != ios_defs::hex);
        const __unsigned_type __u = ((__v > 0 || !__dec) ? __unsigned_type(__v)
                                                         : -__unsigned_type(__v));
        auto __len = int_to_char(__cs + __ilen, __u, __flags, __dec);
        __cs += __ilen - __len;
        
        // Add grouping, if necessary.
        if (!m_grouping.empty())
        {
            // Grouping can add (almost) as many separators as the number
            // of digits + space is reserved for numeric base or sign.
            std::vector<char_type> cs_vec2((__ilen + 1) * 2);
            char_type* __cs2 = cs_vec2.data() + 2;
            __len = FacetHelper::add_grouping(__cs2, m_thousands_sep, m_grouping, __cs, __cs + __len) - __cs2;
            std::swap(cs_vec, cs_vec2);
            __cs = cs_vec.data() + 2;
        }
        
        // Complete Stage 1, prepend numeric base or sign.
        if (__dec)
        { // Decimal.
            if (__v >= 0)
            {
                if (bool(__flags & ios_defs::showpos) && std::is_signed_v<TValue>)
                    *--__cs = m_out_atoms[s_oplus], ++__len;
            }
            else
                *--__cs = m_out_atoms[s_ominus], ++__len;
        }
        else if (bool(__flags & ios_defs::showbase) && __v)
        {
            if (__basefield == ios_defs::oct)
                *--__cs = m_out_atoms[s_odigits], ++__len;
            else
            {
                // 'x' or 'X'
                const bool __uppercase = __flags & ios_defs::uppercase;
                *--__cs = m_out_atoms[s_ox + __uppercase];
                // '0'
                *--__cs = m_out_atoms[s_odigits];
                    __len += 2;
            }
        }
        
        // Pad.
        const std::streamsize __w = __io.width();
        if (__w > static_cast<std::streamsize>(__len))
        {
            std::vector<char_type> cs_vec3(__w);
            char_type* __cs3 = cs_vec3.data();
            bool startSign = (__cs[0] == m_out_atoms[s_ominus]) || (__cs[0] == m_out_atoms[s_oplus]);
            bool start0x = (__cs[0] == m_out_atoms[s_odigits]) && (__len > 1) && 
                           ((__cs[1] == m_out_atoms[s_ox]) || (__cs[1] == m_out_atoms[s_oX]));
            const ios_defs::fmtflags __adjust = __io.flags() & ios_defs::adjustfield;
            pad(__io.fill(), __w, __adjust, __cs3, __cs, __len, startSign, start0x);
            std::swap(cs_vec, cs_vec3);
            __cs = cs_vec.data();
        }
        __io.width(0);

        // [22.2.2.2.2] Stage 4.
        // Write resulting, fully-formatted string to output iterator.
        return std::copy(__cs, __cs + __len, __s);
    }
    
    template <typename TValue>
    int int_to_char(CharT* __bufend, TValue __v, ios_defs::fmtflags __flags, bool __dec) const
    {
        auto __buf = __bufend;
        if (__dec)
        {   // Decimal.
            do
            {
                *--__buf = m_out_atoms[(__v % 10) + s_odigits];
                __v /= 10;
            } while (__v != 0);
        }
        else if ((__flags & ios_defs::basefield) == ios_defs::oct)
        { // Octal.
            do
            {
                *--__buf = m_out_atoms[(__v & 0x7) + s_odigits];
                __v >>= 3;
            } while (__v != 0);
        }
        else
        {   // Hex.
            const bool __uppercase = __flags & ios_defs::uppercase;
            const int __case_offset = __uppercase ? s_oudigits : s_odigits;
            do
            {
                *--__buf = m_out_atoms[(__v & 0xf) + __case_offset];
                __v >>= 4;
            } while (__v != 0);
        }
        return __bufend - __buf;
    }

    void pad(char_type __fill, std::streamsize __w, ios_defs::fmtflags __adjust,
             char_type* __new, const char_type* __cs, int& __len,
             bool startSign, bool start0x) const
    {
      // [22.2.2.2.2] Stage 3.
      // If necessary, pad.
      _S_pad(__adjust, __fill, __new, __cs, __w, __len, startSign, start0x);
      __len = static_cast<int>(__w);
    }
    
    void _S_pad(ios_defs::fmtflags __adjust, char_type __fill,
                char_type* __news, const char_type* __olds,
                std::streamsize __newlen, std::streamsize __oldlen,
                bool startSign, bool start0x) const
    {
        
        const size_t __plen = static_cast<size_t>(__newlen - __oldlen);

        // Padding last.
        if (__adjust == ios_defs::left)
        {
            std::copy(__olds, __olds + __oldlen, __news);
            std::fill_n(__news + __oldlen, __plen, __fill);
            return;
        }

        size_t __mod = 0;
        if (__adjust == ios_defs::internal)
        {
            // Pad after the sign, if there is one.
            // Pad after 0[xX], if there is one.
            // Who came up with these rules, anyway? Jeeze.
            if (startSign)
            {
                __news[0] = __olds[0];
                __mod = 1;
                ++__news;
            }
            else if (start0x)
            {
                __news[0] = __olds[0];
                __news[1] = __olds[1];
                __mod = 2;
                __news += 2;
            }
            // else Padding first.
        }
        std::fill_n(__news, __plen, __fill);
        std::copy(__olds + __mod, __olds + __oldlen, __news + __plen);
    }
    
    void _S_format_float(ios_defs::fmtflags __flags, char* __fptr, char __mod) const noexcept
    {
        *__fptr++ = '%';
        // [22.2.2.2.2] Table 60
        if (__flags & ios_defs::showpos)
            *__fptr++ = '+';
        if (__flags & ios_defs::showpoint)
            *__fptr++ = '#';
            
        ios_defs::fmtflags __fltfield = __flags & ios_defs::floatfield;
        if (__fltfield != (ios_defs::fixed | ios_defs::scientific))
        {
            // As per DR 231: not only when __flags & ios_base::fixed || __prec > 0
            *__fptr++ = '.';
            *__fptr++ = '*';
        }
        
        if (__mod)
            *__fptr++ = __mod;
        // [22.2.2.2.2] Table 58
        if (__fltfield == ios_defs::fixed)
            *__fptr++ = 'f';
        else if (__fltfield == ios_defs::scientific)
            *__fptr++ = (__flags & ios_defs::uppercase) ? 'E' : 'e';
        else if (__fltfield == (ios_defs::fixed | ios_defs::scientific))
            *__fptr++ = (__flags & ios_defs::uppercase) ? 'A' : 'a';
        else
            *__fptr++ = (__flags & ios_defs::uppercase) ? 'G' : 'g';
        *__fptr = '\0';
    }
    
    void group_float(const std::vector<uint8_t>& __grouping, char_type __sep, const char_type* __p, char_type* __new, char_type* __cs, int& __len) const
    {
        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 282. What types does numpunct grouping refer to?
        // Add grouping, if necessary.
        const int __declen = __p ? __p - __cs : __len;
        char_type* __p2 = FacetHelper::add_grouping(__new, __sep, __grouping, __cs, __cs + __declen);

        // Tack on decimal part.
        int __newlen = __p2 - __new;
        if (__p)
        {
            std::copy(__p, __p + __len - __declen, __p2);
            __newlen += __len - __declen;
        }
        __len = __newlen;
    }
    
    template <typename TIter, std::sentinel_for<TIter> TSent, typename TValue>
    std::pair<bool, TIter> extract_int(TIter __beg, TSent __end, ios_base<char_type>& __io, TValue& __v) const
    {
        using __unsigned_type = std::make_unsigned_t<TValue>;
        
        CharT __c{};

        // NB: Iff __basefield == 0, __base can change based on contents.
        const ios_defs::fmtflags __basefield = __io.flags() & ios_defs::basefield;
        const bool __oct = __basefield == ios_defs::oct;
        int __base = __oct ? 8 : (__basefield == ios_defs::hex ? 16 : 10);

        // True if __beg becomes equal to __end.
        bool __testeof = __beg == __end;

        // First check for sign.
        bool __negative = false;
        if (!__testeof)
        {
            __c = *__beg;
            __negative = __c == m_in_atoms[s_ominus];
            if ((__negative || __c == m_in_atoms[s_oplus])
                && (m_grouping.empty() || __c != m_thousands_sep)
                && (__c != m_decimal_point))
            {
                if (++__beg != __end)
                    __c = *__beg;
                else
                    __testeof = true;
            }
        }

        // Next, look for leading zeros and check required digits
        // for base formats.
        bool __found_zero = false;
        int __sep_pos = 0;
        while (!__testeof)
        {
            if ((!m_grouping.empty() && __c == m_thousands_sep)
                || __c == m_decimal_point)
                break;
            else if (__c == m_in_atoms[s_odigits] && (!__found_zero || __base == 10))
            {
                __found_zero = true;
                ++__sep_pos;
                if (__basefield == 0) __base = 8;
                if (__base == 8) __sep_pos = 0;
            }
            else if (__found_zero && (__c == m_in_atoms[s_ox] || __c == m_in_atoms[s_oX]))
            {
                if (__basefield == 0) __base = 16;
                if (__base == 16)
                {
                    __found_zero = false;
                    __sep_pos = 0;
                }
                else
                    break;
            }
            else
                break;

            if (++__beg != __end)
            {
                __c = *__beg;
                if (!__found_zero) break;
            }
            else
                __testeof = true;
        }

        // At this point, base is determined. If not hex, only allow
        // base digits as valid input.
        const size_t __len = (__base == 16 ? s_iend - s_izero : __base);

        // Extract.
        std::string __found_grouping;
        if (!m_grouping.empty()) __found_grouping.reserve(32);
        bool __testfail = false;
        bool __testoverflow = false;
        const __unsigned_type __max = (__negative && std::is_signed_v<TValue>)
                                        ? -static_cast<__unsigned_type>(std::numeric_limits<TValue>::min()) : std::numeric_limits<TValue>::max();
        const __unsigned_type __smax = __max / __base;
        __unsigned_type __result = 0;
        int __digit = 0;
        const char_type* __lit_zero = m_in_atoms + s_izero;

        while (!__testeof)
        {
            // According to 22.2.2.1.2, p8-9, first look for thousands_sep
            // and decimal_point.
            if (!m_grouping.empty() && __c == m_thousands_sep)
            {
                // NB: Thousands separator at the beginning of a string
                // is a no-no, as is two consecutive thousands separators.
                if (__sep_pos)
                {
                    __found_grouping += static_cast<char>(__sep_pos);
                    __sep_pos = 0;
                }
                else
                {
                    __testfail = true;
                    break;
                }
            }
            else if (__c == m_decimal_point) break;
            else
            {
                const char_type* __q = std::find(__lit_zero, __lit_zero + __len, __c);
                if (__q == __lit_zero + __len) break;

                __digit = __q - __lit_zero;
                if (__digit > 15) __digit -= 6;
                if (__result > __smax) __testoverflow = true;
                else
                {
                    __result *= __base;
                    __testoverflow |= __result > __max - __digit;
                    __result += __digit;
                    ++__sep_pos;
                }
            }
            
            if (++__beg != __end) __c = *__beg;
            else __testeof = true;
        }

        bool success = true;
        // Digit grouping is checked. If grouping and found_grouping don't
        // match, then get very very upset, and set failbit.
        if (!__found_grouping.empty())
        {
            // Add the ending grouping.
            __found_grouping += static_cast<char>(__sep_pos);

            success = FacetHelper::verify_grouping(m_grouping, __found_grouping);
        }

        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 23. Num_get overflow result.
        if ((!__sep_pos && !__found_zero && !__found_grouping.size()) || __testfail)
        {
            __v = 0;
            success = false;
        }
        else if (__testoverflow)
        {
            if (__negative && std::is_signed_v<TValue>)
                __v = std::numeric_limits<TValue>::min();
            else
                __v = std::numeric_limits<TValue>::max();
            success = false;
        }
        else
            __v = __negative ? -__result : __result;

        return std::pair(success, __beg);
    }

    template <typename TIter, std::sentinel_for<TIter> TSent>
    std::pair<bool, TIter> extract_float(TIter __beg, TSent __end, ios_base<char_type>& __io, std::string& __xtrc) const
    {
        char_type __c = char_type();

        // True if __beg becomes equal to __end.
        bool __testeof = __beg == __end;

        // First check for sign.
        if (!__testeof)
        {
            __c = *__beg;
            const bool __plus = __c == m_in_atoms[s_iplus];
            if ((__plus || __c == m_in_atoms[s_iminus])
                && (m_grouping.empty() || __c != m_thousands_sep)
                && (__c != m_decimal_point))
            {
                __xtrc += __plus ? '+' : '-';
                if (++__beg != __end) __c = *__beg;
                else
                    __testeof = true;
            }
        }

        // Next, look for leading zeros.
        bool __found_mantissa = false;
        int __sep_pos = 0;
        while (!__testeof)
        {
            if ((!m_grouping.empty() && __c == m_thousands_sep) || __c == m_decimal_point)
                break;
            else if (__c == m_in_atoms[s_izero])
            {
                if (!__found_mantissa)
                {
                    __xtrc += '0';
                    __found_mantissa = true;
                }
                ++__sep_pos;

                if (++__beg != __end)
                    __c = *__beg;
                else
                    __testeof = true;
            }
            else
                break;
        }

        // Only need acceptable digits for floating point numbers.
        bool __found_dec = false;
        bool __found_sci = false;
        std::string __found_grouping;
        if (!m_grouping.empty())
            __found_grouping.reserve(32);
        const char_type* __lit_zero = m_in_atoms + s_izero;

        while (!__testeof)
        {
            // According to 22.2.2.1.2, p8-9, first look for thousands_sep
            // and decimal_point.
            if (!m_grouping.empty() && __c == m_thousands_sep)
            {
                if (!__found_dec && !__found_sci)
                {
                    // NB: Thousands separator at the beginning of a string
                    // is a no-no, as is two consecutive thousands separators.
                    if (__sep_pos)
                    {
                        __found_grouping += static_cast<char>(__sep_pos);
                        __sep_pos = 0;
                    }
                    else
                    {
                        // NB: convert_to_v will not assign __v and will
                        // set the failbit.
                        __xtrc.clear();
                        break;
                    }
                }
                else
                    break;
            }
            else if (__c == m_decimal_point)
            {
                if (!__found_dec && !__found_sci)
                {
                    // If no grouping chars are seen, no grouping check
                    // is applied. Therefore __found_grouping is adjusted
                    // only if decimal_point comes after some thousands_sep.
                    if (__found_grouping.size())
                        __found_grouping += static_cast<char>(__sep_pos);
                    __xtrc += '.';
                    __found_dec = true;
                }
                else
                    break;
            }
            else
            {
                const char_type* __q = std::find(__lit_zero, __lit_zero + 10, __c);
                if (__q != __lit_zero + 10)
                {
                    __xtrc += '0' + (__q - __lit_zero);
                    __found_mantissa = true;
                    ++__sep_pos;
                }
                else if ((__c == m_in_atoms[s_ie] || __c == m_in_atoms[s_iE])
                        && !__found_sci && __found_mantissa)
                {
                    // Scientific notation.
                    if (__found_grouping.size() && !__found_dec)
                        __found_grouping += static_cast<char>(__sep_pos);
                    __xtrc += 'e';
                    __found_sci = true;

                    // Remove optional plus or minus sign, if they exist.
                    if (++__beg != __end)
                    {
                        __c = *__beg;
                        const bool __plus = __c == m_in_atoms[s_iplus];
                        if ((__plus || __c == m_in_atoms[s_iminus])
                            && (m_grouping.empty() || __c != m_thousands_sep)
                            && (__c != m_decimal_point))
                            __xtrc += __plus ? '+' : '-';
                        else
                            continue;
                    }
                    else
                    {
                        __testeof = true;
                        break;
                    }
                }
                else
                    break;
            }

            if (++__beg != __end)
                __c = *__beg;
            else
                __testeof = true;
        }

        bool success = true;
        // Digit grouping is checked. If grouping and found_grouping don't
        // match, then get very very upset, and set failbit.
        if (__found_grouping.size())
        {
            // Add the ending grouping if a decimal or 'e'/'E' wasn't found.
            if (!__found_dec && !__found_sci)
                __found_grouping += static_cast<char>(__sep_pos);

            success = FacetHelper::verify_grouping(m_grouping, __found_grouping);
        }

        return std::pair(success, __beg);
    }

    template <typename TValue>
    bool convert_to_v(const char* __s, TValue& __v) const noexcept
    {
        bool res = true;
        char* __sanity;
        
        clocale_wrapper inter_locale("C");
        clocale_user guard(inter_locale.c_locale);
        
        if constexpr (std::is_same_v<TValue, float>)
            __v = strtof(__s, &__sanity);
        else if constexpr (std::is_same_v<TValue, double>)
            __v = strtod(__s, &__sanity);
        else
            __v = strtold(__s, &__sanity);

        // _GLIBCXX_RESOLVE_LIB_DEFECTS
        // 23. Num_get overflow result.
        if (__sanity == __s || *__sanity != '\0')
        {
            __v = 0.0l;
            res = false;
        }
        else if (__v == std::numeric_limits<TValue>::infinity())
        {
            __v = std::numeric_limits<TValue>::max();
            res = false;
        }
        else if (__v == -std::numeric_limits<TValue>::infinity())
        {
            __v = -std::numeric_limits<TValue>::max();
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