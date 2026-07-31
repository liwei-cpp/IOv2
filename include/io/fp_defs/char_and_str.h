#pragma once
#include <algorithm>
#include <string>
#include <type_traits>
#include <io/io_base.h>
#include <io/fp_defs/base_fp.h>
#include <facet/ctype.h>
#include <locale/locale.h>

namespace IOv2
{
template <typename TIter, typename TChar>
    requires (std::is_same_v<TChar, typename TIter::value_type>)
TIter ostream_insert(TIter iter, ios_base<TChar>& io, const TChar* s, size_t n)
{
    const size_t w = io.width();
    if (w > n)
    {
        const bool left = ((io.flags() & ios_defs::adjustfield) == ios_defs::left);
        const TChar f = io.fill();
        if (!left)
            iter = std::fill_n(iter, w - n, f);
        iter = std::copy(s, s + n, iter);
        if (left)
            iter = std::fill_n(iter, w - n, f);
    }
    else
        iter = std::copy(s, s + n, iter);
    io.width(0);
    return iter;
}

template <typename TIter, std::sentinel_for<TIter> TSent, typename TChar>
    requires (std::is_same_v<TChar, typename TIter::value_type>)
TIter istream_extract(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* s, size_t num)
{
    const size_t width = io.width();
    if (0 < width && width < num)
        num = width;

    if (num <= 1)
    {
        if (num == 1) *s = TChar{};
        io.width(0);
        throw stream_error("DO not have enough buffer to save character");
    }

    auto ct = loc.template get<ctype<TChar>>();
    if (!ct)
        throw stream_error("cannot get ctype facet");

    if (iter == iter_end) throw stream_error("cannot extract character from empty stream");

    size_t extracted = 0;

    while (extracted < num - 1
           && (iter != iter_end)
           && !(ct->is_any(base_ft<ctype>::space, *iter)))
    {
        *s++ = *iter;
        ++extracted;
        ++iter;
    }

    *s = TChar{};

    io.width(0);

    if (extracted == 0)
        throw stream_error("istream extraction fail: no characters extracted");

    return iter;
}

template <typename TChar>
struct writer<TChar, TChar>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, TChar c)
    {
        if (io.width() != 0)
            return ostream_insert(iter, io, &c, 1);
        *iter++ = c;
        return iter;
    }
};

template <typename TChar>
struct reader<TChar, TChar>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>&, TChar& c)
    {
        if (iter == iter_end)
            throw stream_error("Cannot parse character");

        c = *iter;
        return ++iter;
    }
};

template <typename TChar>
struct writer<TChar, char>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, char c)
    {
        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        TChar wc = mp->widen(c);
        if (io.width() != 0)
            return ostream_insert(iter, io, &wc, 1);
        *iter++ = wc;
        return iter;
    }
};

template <>
struct writer<char, char>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, char c)
    {
        auto mp = loc.template get<ctype<char>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        char wc = mp->widen(c);
        if (io.width() != 0)
            return ostream_insert(iter, io, &wc, 1);
        *iter++ = wc;
        return iter;
    }
};

template <typename TChar>
struct writer<TChar, TChar*>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, const TChar* c)
    {
        if (c == nullptr) throw IOv2::stream_error("Cannot write NULL character sequence");

        size_t n = 0;
        for (const TChar* ptr = c; *ptr != 0; ++ptr, ++n);

        return ostream_insert(iter, io, c, n);
    }
};

template <typename TChar>
struct writer<TChar, const TChar*>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, const TChar* c)
    {
        return writer<TChar, TChar*>::swrite(iter, io, loc, c);
    }
};

/**
 * @lang{ZH}
 * @brief 将一个以空白分隔的 token 提取到定长字符数组。
 *
 * @note **本库不提供向裸指针（`TChar*`）提取的 reader，`is >> ptr` 无法编译。** 这与
 *       C++20 起的 `std::istream` 一致：P0487R1 删除了 `operator>>(basic_istream&, charT*)`，
 *       只保留数组引用形式 `charT (&)[N]`。理由是内存安全——目标是裸指针时，库无从得知
 *       缓冲区容量：`istream_extract` 的循环只有三个终止条件（写满 `num`、输入流 EOF、
 *       遇到空白），而后两者描述的是**输入源**的状态，与目标缓冲区大小无关。C++17 及更早
 *       的规定是"`width == 0` 即无上界"，于是 `is >> ptr` 会一路写到遇见空白为止，输入
 *       受攻击者控制时即为可利用的缓冲区溢出。
 * @note 本重载安全的原因是上界 `N` 来自**类型**而非流状态：实际读入量为
 *       `min(width, N) - 1` 个字符加一个终止符。因此 `setw()` 在这里只能把边界**收紧**，
 *       永远不可能放宽；即便携带了来自上一次操作的陈旧 `width`（算术提取、`get_money`、
 *       `get_time` 等都不消费 `width`，与标准一致），也绝不会越过 `N`。
 * @note 需要运行期确定容量的缓冲区，请提取到 `std::basic_string`（自动增长），或改用
 *       非格式化的 `istream::read(s, n)` / `get(s, n)`，二者都显式接收容量。
 * @param c 目标缓冲区。
 * @return 指向最后一个被消费字符之后的输入迭代器。
 * @throw stream_error 若 `N <= 1`（放不下终止符），或未提取到任何字符。
 * @endif
 *
 * @lang{EN}
 * @brief Extracts one whitespace-delimited token into a fixed-size character array.
 *
 * @note **This library provides no reader for a raw pointer (`TChar*`); `is >> ptr` does not
 *       compile.** This matches `std::istream` as of C++20: P0487R1 removed
 *       `operator>>(basic_istream&, charT*)`, keeping only the array-reference form
 *       `charT (&)[N]`. The reason is memory safety -- when the target is a raw pointer the
 *       library cannot know the buffer's capacity: `istream_extract`'s loop has only three
 *       termination conditions (`num` reached, input at EOF, whitespace found), and the
 *       latter two describe the state of the *input source* and say nothing about the
 *       destination's size. The rule through C++17 was "`width == 0` means no bound", so
 *       `is >> ptr` wrote on until whitespace -- an exploitable buffer overflow when the
 *       input is attacker-controlled.
 * @note What makes this overload safe is that the bound `N` comes from the **type** rather
 *       than from stream state: at most `min(width, N) - 1` characters plus a terminator are
 *       stored. `setw()` can therefore only **tighten** the bound here, never loosen it --
 *       even a stale `width` left over from an earlier operation (arithmetic extraction,
 *       `get_money` and `get_time` do not consume `width`, matching the standard) can never
 *       reach past `N`.
 * @note For a buffer whose capacity is only known at run time, extract into a
 *       `std::basic_string` (which grows on demand), or use the unformatted
 *       `istream::read(s, n)` / `get(s, n)`, both of which take the capacity explicitly.
 * @param c The destination buffer.
 * @return An input iterator past the last consumed character.
 * @throw stream_error If `N <= 1` (no room for the terminator), or no characters were
 *        extracted.
 * @endif
 */
template <typename TChar, size_t N>
struct reader<TChar, TChar[N]>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* c)
    {
        if constexpr (N <= 1)
            throw IOv2::stream_error("Character buffer not enough");

        constexpr size_t n = N;
        return istream_extract(iter, iter_end, io, loc, c, n);
    }
};

template <size_t N>
struct reader<char, unsigned char[N]>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char* c)
    {
        if constexpr (N <= 1)
            throw IOv2::stream_error("Character buffer not enough");
        constexpr size_t n = N;
        return istream_extract(iter, iter_end, io, loc, reinterpret_cast<char*>(c), n);
    }
};

template <size_t N>
struct reader<char, signed char[N]>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char* c)
    {
        if constexpr (N <= 1)
            throw IOv2::stream_error("Character buffer not enough");
        constexpr size_t n = N;
        return istream_extract(iter, iter_end, io, loc, reinterpret_cast<char*>(c), n);
    }
};

template <>
struct reader<char, unsigned char>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char& c)
    {
        char tmp;
        auto res = reader<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

template <>
struct reader<char, signed char>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char& c)
    {
        char tmp;
        auto res = reader<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

template <typename TChar, typename TTraits, typename TAlloc>
struct writer<TChar, std::basic_string<TChar, TTraits, TAlloc>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, const std::basic_string<TChar, TTraits, TAlloc>& str)
    {
        return ostream_insert(iter, io, str.data(), str.size());
    }
};

template <typename TChar, typename TTraits, typename TAlloc>
struct reader<TChar, std::basic_string<TChar, TTraits, TAlloc>>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, std::basic_string<TChar, TTraits, TAlloc>& str)
    {
        str.erase();
        TChar buf[128];
        size_t len = 0;
        const size_t w = io.width();
        const size_t n = w > 0 ? w : str.max_size();
        size_t extracted = 0;

        auto ct = loc.template get<ctype<TChar>>();
        if (!ct)
            throw stream_error("cannot get ctype facet");
        while (extracted < n
               && (iter != iter_end)
               && !(ct->is_any(base_ft<ctype>::space, *iter)))
        {
            if (len == 128)
            {
                str.append(buf, 128);
                len = 0;
            }
            buf[len++] = *iter;
            ++extracted;
            ++iter;
        }
        str.append(buf, len);

        io.width(0);

        if (extracted == 0)
            throw stream_error("istream extraction fail: no characters extracted");

        return iter;
    }
};
}