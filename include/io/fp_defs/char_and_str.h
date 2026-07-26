#pragma once
#include <algorithm>
#include <limits>
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
TIter ostream_insert(TIter iter, ios_base<TChar>& io, const TChar* s, std::streamsize n)
{
    const std::streamsize w = io.width();
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
TIter istream_extract(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* s, std::streamsize num)
{
    std::streamsize width = io.width();
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

    std::streamsize extracted = 0;

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

template <typename TChar>
struct reader<TChar, TChar*>
{
    /**
     * @lang{ZH}
     * @brief 将一个以空白分隔的 token 提取到裸指针所指的缓冲区。
     *
     * @warning **本重载要求调用方以 `setw(n)` 显式给出字段宽度，否则拒绝提取。**
     *          这是对 `std::istream` 的有意偏离，理由是内存安全：目标是裸指针时，本库
     *          无从得知缓冲区容量——`istream_extract` 的循环只有三个终止条件（写满
     *          `num`、输入流 EOF、遇到空白），而后两者描述的是**输入源**的状态，与目标
     *          缓冲区大小无关。因此唯一的越界防线就是 `num`，它只能来自 `io.width()`。
     *          标准库在这里选择"无 width 即无上界"，`is >> ptr` 会一路写到遇见空白为止，
     *          输入受攻击者控制时即为可利用的缓冲区溢出。本库改为显式报错。
     * @note 数组重载 `reader<TChar, TChar[N]>` **不**受此约束：它从类型中拿到真实容量
     *       `N`，本身就是安全的，无需 `setw`。
     * @note `width` 是一次性的（`istream_extract` 返回前会 `io.width(0)`），故链式提取
     *       `is >> setw(n) >> p1 >> p2` 中 `p2` 同样需要自己的 `setw`——这正是本检查要
     *       拦下的典型误用。
     * @note 由于 `width` 以 `std::uint8_t` 存储（见 `ios_base::width`），本重载可读入的
     *       token 上限为 255 个字符；更长的 token 请提取到 `std::basic_string`。
     * @param c 目标缓冲区；不得为空指针。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 `c` 为空指针，或未经 `setw(n)` 设置字段宽度（`width == 0`）。
     * @endif
     *
     * @lang{EN}
     * @brief Extracts one whitespace-delimited token into the buffer a raw pointer refers to.
     *
     * @warning **This overload requires the caller to supply a field width via `setw(n)`;
     *          otherwise it refuses to extract.** This is a deliberate divergence from
     *          `std::istream`, made for memory safety: when the target is a raw pointer the
     *          library cannot know the buffer's capacity -- `istream_extract`'s loop has only
     *          three termination conditions (`num` reached, input at EOF, whitespace found),
     *          and the latter two describe the state of the *input source* and say nothing
     *          about the destination's size. The only bound left is `num`, which can only
     *          come from `io.width()`. The standard library chooses "no width means no
     *          bound", so `is >> ptr` writes on until whitespace -- an exploitable buffer
     *          overflow when the input is attacker-controlled. This library reports an error
     *          instead.
     * @note The array overload `reader<TChar, TChar[N]>` is **not** subject to this: it takes
     *       the real capacity `N` from the type and is safe on its own, with no `setw` needed.
     * @note `width` is one-shot (`istream_extract` does `io.width(0)` before returning), so in
     *       a chained extraction `is >> setw(n) >> p1 >> p2` the `p2` step needs its own
     *       `setw` -- precisely the misuse this check is meant to catch.
     * @note Because `width` is stored as a `std::uint8_t` (see `ios_base::width`), this
     *       overload can read at most 255 characters; extract into a `std::basic_string` for
     *       longer tokens.
     * @param c The destination buffer; must not be a null pointer.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If `c` is a null pointer, or no field width was set via `setw(n)`
     *        (`width == 0`).
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* c)
    {
        if (c == nullptr) throw IOv2::stream_error("Cannot read NULL character sequence");

        if (io.width() == 0)
            throw IOv2::stream_error(
                "istream extraction into a raw pointer requires an explicit field width: "
                "the buffer capacity is unknown here, so use setw(n) to bound the read, "
                "or extract into an array or a std::basic_string instead");

        constexpr std::streamsize n = std::numeric_limits<std::streamsize>::max();
        return istream_extract(iter, iter_end, io, loc, c, n);
    }
};

template <typename TChar, size_t N>
struct reader<TChar, TChar[N]>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* c)
    {
        if constexpr (N <= 1)
            throw IOv2::stream_error("Character buffer not enough");
        
        constexpr std::streamsize n = N;
        return istream_extract(iter, iter_end, io, loc, c, n);
    }
};

template <>
struct reader<char, unsigned char*>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char* c)
    {
        return reader<char, char*>::sread(iter, iter_end, io, loc, reinterpret_cast<char*>(c));
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
        constexpr std::streamsize n = N;
        return istream_extract(iter, iter_end, io, loc, reinterpret_cast<char*>(c), n);
    }
};

template <>
struct reader<char, signed char*>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char* c)
    {
        return reader<char, char*>::sread(iter, iter_end, io, loc, reinterpret_cast<char*>(c));
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
        constexpr std::streamsize n = N;
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
        auto w = io.width();
        size_t n = w > 0  ? static_cast<size_t>(w) : str.max_size();
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