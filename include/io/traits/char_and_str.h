#pragma once
#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>
#include <io/io_base.h>
#include <io/traits/traits_base.h>
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
struct io_traits<TChar, TChar>
{
    /**
     * @lang{ZH}
     * @note `TChar` 为 `char` 时先经 `ctype<char>::widen()` 再写出，与下面
     *       `io_traits<TChar, char>` 的加宽路径保持一致；宽流上字符已是 `TChar`，直接写出。
     * @endif
     *
     * @lang{EN}
     * @note When `TChar` is `char` the character goes through `ctype<char>::widen()` first,
     *       matching the widening path of `io_traits<TChar, char>` below; on a wide stream the
     *       character already is a `TChar` and is written straight through.
     * @endif
     */
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, TChar c)
    {
        if constexpr (std::is_same_v<TChar, char>)
        {
            auto mp = loc.template get<ctype<char>>();
            if (!mp)
                throw stream_error("cannot get numeric facet");

            c = mp->widen(c);
        }

        if (io.width() != 0)
            return ostream_insert(iter, io, &c, 1);
        *iter++ = c;
        return iter;
    }

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
    requires (!std::is_same_v<TChar, char>)
struct io_traits<TChar, char>
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
struct io_traits<char, unsigned char>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, unsigned char c)
    {
        return io_traits<char, char>::swrite(iter, io, loc, static_cast<char>(c));
    }

    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char& c)
    {
        char tmp;
        auto res = io_traits<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

template <>
struct io_traits<char, signed char>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, signed char c)
    {
        return io_traits<char, char>::swrite(iter, io, loc, static_cast<char>(c));
    }

    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char& c)
    {
        char tmp;
        auto res = io_traits<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

template <typename TChar>
struct io_traits<TChar, TChar*>
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
struct io_traits<TChar, const TChar*>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, const TChar* c)
    {
        return io_traits<TChar, TChar*>::swrite(iter, io, loc, c);
    }
};

/**
 * @lang{ZH}
 * @brief 把一个窄字符串加宽后写入 `TChar` 流。
 *
 * @note 这条对应标准里对 `charT` 模板化的
 *       `operator<<(basic_ostream<charT>&, const char*)`：窄字符串可以写进**任意**字符类型的
 *       流，逐字符经 `ctype<TChar>::widen()` 加宽。少了它，`wos << "hi"` 会被
 *       `arithmetic.h` 的通用指针特化接走而打印地址（本库早先正是如此），或者干脆
 *       编译不过。
 * @note `TChar` 就是 `char` 时不走这里，而由下面的全特化 `io_traits<char, char*>` 承接：那时
 *       无需加宽，也就不必为此分配缓冲区。之所以要那条全特化，是因为
 *       `io_traits<TChar, TChar*>` 与 `io_traits<TChar, char*>` 在 `TChar == char` 上互不更
 *       特化，会直接构成歧义——`io_traits<TChar, TChar>` 与 `io_traits<TChar, char>` 之间同样
 *       的歧义则由后者的 `requires (!std::is_same_v<TChar, char>)` 排除。
 * @note 加宽必须先落到一段连续缓冲区再交给 `ostream_insert`，因为补齐要预先知道总宽度。
 *       这是本特化与直接写出的 `io_traits<TChar, TChar*>` 之间唯一的额外代价。
 * @param c 以空字符结尾的窄字符串。
 * @throw stream_error 若 `c` 为空指针，或 locale 中没有 `ctype<TChar>` facet。
 * @endif
 *
 * @lang{EN}
 * @brief Widens a narrow string and writes it to a `TChar` stream.
 *
 * @note This mirrors the standard's `operator<<(basic_ostream<charT>&, const char*)`, which
 *       is templated on `charT`: a narrow string may be written to a stream of **any**
 *       character type, each character widened through `ctype<TChar>::widen()`. Without it
 *       `wos << "hi"` is picked up by the generic pointer specialization in `arithmetic.h` and
 *       prints an address (as this library used to do), or fails to compile outright.
 * @note When `TChar` is `char` this specialization is not used; the explicit
 *       `io_traits<char, char*>` below takes over, where no widening -- and hence no buffer --
 *       is needed. That explicit specialization is required because `io_traits<TChar, TChar*>`
 *       and `io_traits<TChar, char*>` are neither more specialized than the other at
 *       `TChar == char` and would simply be ambiguous -- the same ambiguity between
 *       `io_traits<TChar, TChar>` and `io_traits<TChar, char>` is instead ruled out by the
 *       latter's `requires (!std::is_same_v<TChar, char>)`.
 * @note Widening has to land in a contiguous buffer before reaching `ostream_insert`, because
 *       padding needs the total width up front. That is the one extra cost this specialization
 *       carries over the straight-through `io_traits<TChar, TChar*>`.
 * @param c A null-terminated narrow string.
 * @throw stream_error If `c` is null, or the locale carries no `ctype<TChar>` facet.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, char*>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, const char* c)
    {
        if (c == nullptr) throw IOv2::stream_error("Cannot write NULL character sequence");

        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get ctype facet");

        size_t n = 0;
        for (const char* ptr = c; *ptr != 0; ++ptr, ++n);

        std::vector<TChar> buf(n);
        mp->widen_seq(c, c + n, buf.data());

        return ostream_insert(iter, io, buf.data(), n);
    }
};

template <typename TChar>
struct io_traits<TChar, const char*>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, const char* c)
    {
        return io_traits<TChar, char*>::swrite(iter, io, loc, c);
    }
};

template <>
struct io_traits<char, char*>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>&, const char* c)
    {
        if (c == nullptr) throw IOv2::stream_error("Cannot write NULL character sequence");

        size_t n = 0;
        for (const char* ptr = c; *ptr != 0; ++ptr, ++n);

        return ostream_insert(iter, io, c, n);
    }
};

template <>
struct io_traits<char, const char*>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, c);
    }
};

/**
 * @lang{ZH}
 * @brief 把 `signed char` / `unsigned char` 字符串写入 `char` 流。
 *
 * @note 标准为 `char` 流单独给出了 `operator<<(basic_ostream<char>&, const signed char*)` 与
 *       `const unsigned char*` 两个重载，规定就是 `return out << reinterpret_cast<const
 *       char*>(s);`——**按字节原样写出，不经加宽**，所以这里转交 `io_traits<char, char*>`。三种
 *       窄字符类型的对象表示相同，且通过 `char*` 读取任何对象都是允许的。
 * @note 宽流上没有对应重载。`wos << (const unsigned char*)s` 在标准里经隐式转换落到
 *       `operator<<(const void*)` 打印地址，本库交由 `arithmetic.h` 的通用指针特化得到
 *       同样结果——这正是那条特化的排除名单里不含 `signed char` / `unsigned char` 的原因。
 * @endif
 *
 * @lang{EN}
 * @brief Writes a `signed char` / `unsigned char` string to a `char` stream.
 *
 * @note The standard gives `char` streams their own
 *       `operator<<(basic_ostream<char>&, const signed char*)` and `const unsigned char*`
 *       overloads, specified as `return out << reinterpret_cast<const char*>(s);` -- the
 *       bytes are written **as they are, with no widening** -- so these delegate to
 *       `io_traits<char, char*>`. The three narrow character types share an object
 *       representation, and reading any object through a `char*` is permitted.
 * @note Wide streams have no counterpart overload. By the standard
 *       `wos << (const unsigned char*)s` falls through the implicit conversion to
 *       `operator<<(const void*)` and prints an address; here the generic pointer
 *       specialization in `arithmetic.h` produces the same result -- which is exactly why
 *       `signed char` / `unsigned char` are absent from that specialization's exclusion list.
 * @endif
 */
template <>
struct io_traits<char, unsigned char*>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const unsigned char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c));
    }
};

template <>
struct io_traits<char, const unsigned char*>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const unsigned char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c));
    }
};

template <>
struct io_traits<char, signed char*>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const signed char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c));
    }
};

template <>
struct io_traits<char, const signed char*>
{
    template <typename TIter>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const signed char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c));
    }
};

/**
 * @lang{ZH}
 * @brief 将一个以空白分隔的 token 提取到定长字符数组。
 *
 * @note **本库不提供向裸指针（`TChar*`）提取的 `sread`，`is >> ptr` 无法编译。** 这与
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
 * @note **This library provides no `sread` for a raw pointer (`TChar*`); `is >> ptr` does not
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
struct io_traits<TChar, TChar[N]>
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
struct io_traits<char, unsigned char[N]>
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
struct io_traits<char, signed char[N]>
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

template <typename TChar, typename TTraits, typename TAlloc>
struct io_traits<TChar, std::basic_string<TChar, TTraits, TAlloc>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, const std::basic_string<TChar, TTraits, TAlloc>& str)
    {
        return ostream_insert(iter, io, str.data(), str.size());
    }

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
