// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file char_and_str.h
 * @lang{ZH}
 * 为字符与字符串特化 `io_traits`：单个字符、C 风格字符串、定长字符数组与
 * `std::basic_string`，并提供所有插入器共用的补齐点 `ostream_insert` 与三个定长数组
 * 特化共用的提取点 `istream_extract`。
 *
 * 特化的取舍逐条对照标准 `<ostream>` / `<istream>` 的字符与字符串重载：
 * - **同字符类型**（`TChar` / `TChar*` / `TChar[N]` / `basic_string<TChar>`）对任何流都成立；
 * - **窄字符与窄字符串**（`char` / `char*`）可以写进任意字符类型的流，逐字符经
 *   `ctype<TChar>::widen()` 加宽，对应标准里对 `charT` 模板化的那两条重载；
 * - **`signed char` / `unsigned char`** 及其指针、数组只对 `char` 流成立，按字节原样处理，
 *   对应标准只给 `basic_ostream<char>` / `basic_istream<char>` 的那几条重载。宽流上它们落到
 *   `IOv2/io/traits/arithmetic.h`，行为与标准相同：值按数值写出，指针按地址写出。
 *
 * 提取端刻意**只有**引用到定长数组与 `basic_string` 两种形状，没有裸指针形式，理由见
 * `io_traits<TChar, TChar[N]>` 的说明。每一对 `T*` / `const T*` 特化都成对出现，
 * `const` 版一律转交非 `const` 版，这样 `operator<<` 无论收到哪种键都能命中。
 *
 * 本文件不包含 `arithmetic.h`，两者相互独立；但 `arithmetic.h` 的排除名单是按本文件的存在
 * 设计的——缺了本文件时 <tt>os << 'x'</tt>、`os << (signed char)` 等是编译错误而不是按数值写出。
 * @endif
 *
 * @lang{EN}
 * Specializes `io_traits` for characters and strings -- single characters, C-style strings,
 * fixed-size character arrays and `std::basic_string` -- and provides `ostream_insert`, the
 * padding point every inserter shares, and `istream_extract`, the extraction point the three
 * fixed-size array specializations share.
 *
 * Which specializations exist follows the character and string overloads of the standard's
 * `<ostream>` / `<istream>` case by case:
 * - **the stream's own character type** (`TChar` / `TChar*` / `TChar[N]` /
 *   `basic_string<TChar>`) works on every stream;
 * - **narrow characters and strings** (`char` / `char*`) may be written to a stream of any
 *   character type, each character widened through `ctype<TChar>::widen()`, matching the two
 *   overloads the standard templates on `charT`;
 * - **`signed char` / `unsigned char`**, their pointers and arrays work on `char` streams only
 *   and are handled byte for byte, matching the overloads the standard gives only to
 *   `basic_ostream<char>` / `basic_istream<char>`. On a wide stream they fall to
 *   `IOv2/io/traits/arithmetic.h` and behave as in the standard: the value prints as a number,
 *   the pointer as an address.
 *
 * The extraction side deliberately has **only** two shapes, a reference to a fixed-size array
 * and a `basic_string`, and no raw-pointer form; see `io_traits<TChar, TChar[N]>` for why. Every
 * `T*` / `const T*` specialization comes as a pair, the `const` one always forwarding to the
 * other, so that `operator<<` hits whichever key it is handed.
 *
 * This file does not include `arithmetic.h`; the two are independent. But the exclusion lists
 * in `arithmetic.h` are designed around this file's presence: without it, <tt>os << 'x'</tt>,
 * `os << (signed char)` and the like are compile errors rather than numeric writes.
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/facet/ctype.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/locale/locale.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 把一段字符序列按字段宽度补齐后写出——库内所有插入器共用的补齐点。
 *
 * @note **`width()` 在补齐之前就被消费掉**，因此后面任何一步抛出都不会把它漏给下一次插入。
 *       这也是「一次性流状态」的落实处：本库的插入器一律经过这里，`std::put_time` 那种
 *       既不补齐也不消费的例外由各自的文档单独说明。
 * @note 补齐边由 `adjustfield` 决定，只分左右两种：`left` 补在后面，其余（含 `internal`）
 *       一律补在前面。`internal` 在这里没有独立含义——它要求把填充插到符号或基数前缀之后，
 *       而一段裸字符序列两者都没有。
 * @note 宽度以**字符**计而非字节；`w <= n` 时一个填充字符都不写。填充量超过
 *       `ios_defs::max_pad_count` 时抛出而不是照办，免得一个失手的 `setw()` 变成任意长度的
 *       写入。
 * @tparam TIter 输出迭代器类型，须满足 `char_sink_for<TIter, TChar>`。
 * @tparam TChar 流的字符类型。
 * @param iter 输出迭代器。
 * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
 * @param s 待写出的字符序列，不要求以空字符结尾。
 * @param n `s` 的长度。
 * @return 写完之后的输出迭代器。
 * @throw stream_error 若所需填充量超过 `ios_defs::max_pad_count`。
 * @endif
 *
 * @lang{EN}
 * @brief Writes a character sequence padded to the field width -- the one padding point every
 *        inserter in this library shares.
 *
 * @note **`width()` is consumed before any padding happens**, so a throw further down cannot
 *       leak it into the next insertion. This is where "one-shot stream state" is actually
 *       enforced: every inserter here routes through this function, and the exceptions that
 *       neither pad nor consume (`std::put_time` and its like) say so in their own docs.
 * @note `adjustfield` selects one of two sides only: `left` pads after the sequence, everything
 *       else -- `internal` included -- pads before it. `internal` has no separate meaning here,
 *       since it asks for the fill to go after a sign or base prefix and a bare character
 *       sequence has neither.
 * @note The width counts **characters**, not bytes, and no fill is written at all when
 *       `w <= n`. A fill count above `ios_defs::max_pad_count` throws rather than being
 *       honored, so that one stray `setw()` cannot turn into a write of arbitrary length.
 * @tparam TIter The output iterator type; must satisfy `char_sink_for<TIter, TChar>`.
 * @tparam TChar The stream's character type.
 * @param iter The output iterator.
 * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
 * @param s The sequence to write; it need not be null-terminated.
 * @param n The length of @p s.
 * @return The output iterator past what was written.
 * @throw stream_error If the required fill count exceeds `ios_defs::max_pad_count`.
 * @endif
 */
template <typename TIter, typename TChar>
    requires (char_sink_for<TIter, TChar>)
TIter ostream_insert(TIter iter, ios_base<TChar>& io, const TChar* s, std::size_t n)
{
    const std::size_t w = io.width(0);
    if (w > n)
    {
        const std::size_t pad = w - n;
        if (pad > ios_defs::max_pad_count)
            throw stream_error("ostream insert fail: fill count exceeds max_pad_count");

        const bool left = ((io.flags() & ios_defs::adjustfield) == ios_defs::left);
        const TChar f = io.fill();
        if (!left)
            iter = std::fill_n(iter, pad, f);
        iter = std::copy(s, s + n, iter);
        if (left)
            iter = std::fill_n(iter, pad, f);
    }
    else
        iter = std::copy(s, s + n, iter);
    return iter;
}

/**
 * @lang{ZH}
 * @brief 把一个以空白分隔的词读进调用方给的定容缓冲区——三个定长数组特化共用的提取点。
 *
 * @note **上界只可能被收紧。** `num` 由调用方按目标数组的 `N` 给定，`width()` 只在
 *       `0 < width < num` 时才生效，故 `setw()` 永远越不过 `N`。`width()` 与写侧一样在最前面
 *       就被消费掉。库里没有裸指针版的提取器，原因见 `io_traits<TChar, TChar[N]>` 的说明。
 * @note **只要本函数被调用且缓冲区放得下终止符，每一条出口都会写终止符**——正常结束、
 *       `num == 1` 放不下任何字符、缺 `ctype` facet、一个字符都没读到，四种情形一致。
 * @warning 这条保证**只覆盖本函数内部**。哨兵失败时（空输入、全空白输入、流已处于失败态）
 *          提取运算符按 [istream.formatted.reqmts] 「不尝试获取任何输入」就返回，
 *          **本函数根本不被调用，目标数组一个字节都不被触碰**。此行为与标准及 libstdc++
 *          一致（实测三格逐项相同），故调用方必须**先判流状态再用缓冲区**，不能因为本条
 *          `@note` 就对一个未初始化的数组直接 `strlen()`。
 * @note 不跳过前导空白，那是 sentry（`skipws`）的职责：本函数遇到的第一个字符若是空白，
 *       直接以「未提取到字符」失败。判空白需要 `ctype`，故 facet 缺失时无法开工。
 *       返回的迭代器停在分隔符之前，分隔符不被消费。
 * @tparam TIter 输入迭代器类型，其 `value_type` 须为 `TChar`。
 * @tparam TSent `TIter` 的哨位类型。
 * @tparam TChar 流的字符类型。
 * @param iter 输入迭代器。
 * @param iter_end 输入哨位。
 * @param io 提供 `width()` 的流。
 * @param loc 提供 `ctype<TChar>` 的 locale。
 * @param s 目标缓冲区。
 * @param num 目标缓冲区的容量，含终止符。
 * @return 指向最后一个被消费字符之后的输入迭代器。
 * @throw stream_error 若 `num <= 1`、locale 中没有 `ctype<TChar>` facet，或未提取到任何字符。
 * @endif
 *
 * @lang{EN}
 * @brief Reads one whitespace-delimited token into a caller-supplied fixed-capacity buffer --
 *        the extraction point the three fixed-size array specializations share.
 *
 * @note **The bound can only ever be tightened.** `num` comes from the caller as the target
 *       array's `N`, and `width()` applies only where `0 < width < num`, so `setw()` can never
 *       reach past `N`. As on the write side, `width()` is consumed up front. There is no
 *       raw-pointer extractor in this library; see `io_traits<TChar, TChar[N]>` for why.
 * @note **Every exit writes the terminator whenever the buffer has room for one** -- a normal
 *       stop, a `num == 1` buffer with room for nothing else, a missing `ctype` facet, and
 *       extracting no characters all behave alike.
 * @warning That guarantee covers **the inside of this function only**. When the sentry fails --
 *          empty input, all-whitespace input, a stream already in a failed state -- the
 *          extractor returns "without attempting to obtain any input"
 *          ([istream.formatted.reqmts]), **this function is never called, and not one byte of
 *          the target array is touched**. That matches the standard and libstdc++ (measured
 *          identical on every case), so a caller must **check the stream state before using the
 *          buffer**; this note is not licence to `strlen()` an uninitialized array.
 * @note Leading whitespace is not skipped here -- that is the sentry's job (`skipws`). A first
 *       character that is whitespace fails outright as "no characters extracted". Testing for
 *       whitespace needs `ctype`, which is why a missing facet stops the work before it starts.
 *       The returned iterator stops before the delimiter, which is left unconsumed.
 * @tparam TIter The input iterator type; its `value_type` must be `TChar`.
 * @tparam TSent The sentinel type for `TIter`.
 * @tparam TChar The stream's character type.
 * @param iter The input iterator.
 * @param iter_end The input sentinel.
 * @param io The stream supplying `width()`.
 * @param loc The locale supplying `ctype<TChar>`.
 * @param s The destination buffer.
 * @param num The capacity of @p s, terminator included.
 * @return An input iterator past the last consumed character.
 * @throw stream_error If `num <= 1`, the locale carries no `ctype<TChar>` facet, or no
 *        characters were extracted.
 * @endif
 */
template <typename TIter, std::sentinel_for<TIter> TSent, typename TChar>
    requires (std::is_same_v<TChar, typename TIter::value_type>)
TIter istream_extract(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* s, std::size_t num)
{
    const std::size_t width = io.width(0);
    if (0 < width && width < num)
        num = width;

    if (num <= 1)
    {
        if (num == 1) *s = TChar{};
        throw stream_error("Do not have enough buffer to save character");
    }

    auto ct = loc.template get<ctype<TChar>>();
    if (!ct)
    {
        *s = TChar{};
        throw stream_error("cannot get ctype facet");
    }

    std::size_t extracted = 0;

    try
    {
        while (extracted < num - 1 && (iter != iter_end))
        {
            const TChar c = *iter;
            if (ct->is_any(base_ft<ctype>::space, c))
                break;

            *s++ = c;
            ++extracted;
            ++iter;
        }

        *s = TChar{};
    }
    catch (...)
    {
        *s = TChar{};
        throw;
    }

    if (extracted == 0)
        throw stream_error("istream extraction fail: no characters extracted");

    return iter;
}

/**
 * @lang{ZH}
 * @brief 流自身字符类型的单个字符的读写：`os << c` / `is >> c`。
 *
 * 对应标准的 `operator<<(basic_ostream<charT>&, charT)` 与
 * `operator>>(basic_istream<charT>&, charT&)`。两个方向都不需要 locale：写出不加宽，读入
 * 也不判空白——前导空白已由 sentry 按 `skipws` 跳过，之后的第一个字符无论是什么都被取走。
 *
 * @tparam TChar 流的字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Reading and writing a single character of the stream's own character type:
 *        `os << c` / `is >> c`.
 *
 * Matches the standard's `operator<<(basic_ostream<charT>&, charT)` and
 * `operator>>(basic_istream<charT>&, charT&)`. Neither direction needs the locale: writing
 * does no widening, and reading does no whitespace test -- leading whitespace has already been
 * skipped by the sentry per `skipws`, and whatever character comes next is taken.
 *
 * @tparam TChar The stream's character type.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, TChar>
{
    /**
     * @lang{ZH}
     * @brief 写出一个字符，按需补齐到字段宽度。
     *
     * @note 字符已经是流的字符类型，直接写出，不加宽，也不查 locale。
     * @note `width()` 为 0 时直接写一个字符，不经 `ostream_insert`，也就没有什么可消费；
     *       非 0 时经 `ostream_insert` 补齐，`width()` 由它消费。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param c 要写出的字符。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Writes one character, padded to the field width where one is set.
     *
     * @note The character already is the stream's character type: it is written straight
     *       through, with no widening and no locale lookup.
     * @note With `width()` at 0 the character is written directly, bypassing `ostream_insert`,
     *       so there is nothing to consume; otherwise it goes through `ostream_insert`, which
     *       pads and consumes `width()`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param c The character to write.
     * @return The output iterator past what was written.
     * @throw stream_error If the required fill count exceeds `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, TChar c)
    {
        if (io.width() != 0)
            return ostream_insert(iter, io, &c, 1);
        *iter++ = c;
        return iter;
    }

    /**
     * @lang{ZH}
     * @brief 取走输入中的下一个字符。
     *
     * 与标准一致，本函数**不消费** `width()`，也不看它。
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param c 接收字符的变量。
     * @return 指向被消费字符之后的输入迭代器。
     * @throw stream_error 若输入已到末尾。
     * @endif
     *
     * @lang{EN}
     * @brief Takes the next character from the input.
     *
     * As in the standard, this function neither consumes nor consults `width()`.
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param c Receives the character.
     * @return An input iterator past the consumed character.
     * @throw stream_error If the input is already at its end.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>&, const locale<TChar>&, TChar& c)
    {
        if (iter == iter_end)
            throw stream_error("Cannot parse character");

        c = *iter;
        return ++iter;
    }
};

/**
 * @lang{ZH}
 * @brief 把一个窄字符加宽后写入非 `char` 流：<tt>wos << 'x'</tt>。
 *
 * 对应标准里对 `charT` 模板化的 `operator<<(basic_ostream<charT>&, char)`。只有插入方向：
 * 标准没有 `operator>>(basic_istream<wchar_t>&, char&)`，因此 `wis >> c`（`c` 为 `char`）
 * 编译不过。`TChar` 为 `char` 时由 `io_traits<TChar, TChar>` 承接，这里的 `requires`
 * 只是避免与之二义。
 *
 * @tparam TChar 流的字符类型，不能是 `char`。
 * @endif
 *
 * @lang{EN}
 * @brief Widens a narrow character and writes it to a non-`char` stream: <tt>wos << 'x'</tt>.
 *
 * Matches the standard's `operator<<(basic_ostream<charT>&, char)`, templated on `charT`.
 * Insertion only: the standard has no `operator>>(basic_istream<wchar_t>&, char&)`, so
 * `wis >> c` with a `char` `c` does not compile. When `TChar` is `char`,
 * `io_traits<TChar, TChar>` takes over; the `requires` here only avoids the ambiguity.
 *
 * @tparam TChar The stream's character type; must not be `char`.
 * @endif
 */
template <typename TChar>
    requires (!std::is_same_v<TChar, char>)
struct io_traits<TChar, char>
{
    /**
     * @lang{ZH}
     * @brief 经 `ctype<TChar>::widen()` 加宽一个窄字符后写出，按需补齐到字段宽度。
     *
     * `width_guard` 保证取不到 facet 而抛出时宽度同样清零；非 0 宽度经 `ostream_insert` 补齐。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 提供 `ctype<TChar>` facet 的 locale。
     * @param c 要写出的窄字符。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 locale 中没有 `ctype<TChar>` facet，或所需填充量超过
     *        `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Widens a narrow character through `ctype<TChar>::widen()` and writes it, padded
     *        to the field width where one is set.
     *
     * `width_guard` makes sure the width is zeroed as well when the facet is missing and the
     * function throws; a non-zero width is padded through `ostream_insert`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc The locale supplying the `ctype<TChar>` facet.
     * @param c The narrow character to write.
     * @return The output iterator past what was written.
     * @throw stream_error If the locale carries no `ctype<TChar>` facet, or the required fill
     *        count exceeds `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, char c)
    {
        auto width_guard = io.width_guard();
        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get ctype facet");

        TChar wc = mp->widen(c);
        if (io.width() != 0)
            return ostream_insert(iter, io, &wc, 1);
        *iter++ = wc;
        return iter;
    }
};

/**
 * @lang{ZH}
 * @brief `char` 流上 `unsigned char` 的字符语义读写：`os << uc` / `is >> uc`。
 *
 * 对应标准只给 `basic_ostream<char>` / `basic_istream<char>` 的
 * `operator<<(unsigned char)` 与 `operator>>(unsigned char&)`，两个方向都按**字符**而非数值
 * 处理，并直接转交 `io_traits<char, char>`。`static_cast` 往返在两个方向上都保值：C++20 起
 * 整数转换按模 2ⁿ 定义。宽流上标准没有这两条重载，本库同样不提供——`wos << uc` 落到
 * `arithmetic.h` 按数值写出，`wis >> uc` 编译不过，见那里的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Character-semantics reading and writing of `unsigned char` on a `char` stream:
 *        `os << uc` / `is >> uc`.
 *
 * Matches the `operator<<(unsigned char)` and `operator>>(unsigned char&)` the standard gives
 * only to `basic_ostream<char>` / `basic_istream<char>`: both directions treat the value as a
 * **character**, not a number, and forward straight to `io_traits<char, char>`. The
 * `static_cast` round trip is value-preserving either way, integer conversion being defined
 * modulo 2ⁿ since C++20. Wide streams have neither overload in the standard and get none here:
 * `wos << uc` falls to `arithmetic.h` and prints a number, `wis >> uc` does not compile; see
 * the notes there.
 * @endif
 */
template <>
struct io_traits<char, unsigned char>
{
    /**
     * @lang{ZH}
     * @brief 按字节转交 `io_traits<char, char>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 要写出的字符。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Forwards the byte to `io_traits<char, char>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c The character to write.
     * @return The output iterator past what was written.
     * @throw stream_error If the required fill count exceeds `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, unsigned char c)
    {
        return io_traits<char, char>::swrite(iter, io, loc, static_cast<char>(c));
    }

    /**
     * @lang{ZH}
     * @brief 经 `io_traits<char, char>::sread` 取走下一个字符，按字节写入 @p c。
     *
     * 抛出时 @p c 不被改动。
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param io 未使用，仅为转交。
     * @param loc 未使用，仅为转交。
     * @param c 接收字符的变量。
     * @return 指向被消费字符之后的输入迭代器。
     * @throw stream_error 若输入已到末尾。
     * @endif
     *
     * @lang{EN}
     * @brief Takes the next character through `io_traits<char, char>::sread` and stores the
     *        byte in @p c.
     *
     * @p c is left untouched on a throw.
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param io Unused; passed through.
     * @param loc Unused; passed through.
     * @param c Receives the character.
     * @return An input iterator past the consumed character.
     * @throw stream_error If the input is already at its end.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char& c)
    {
        char tmp{};
        auto res = io_traits<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

/**
 * @lang{ZH}
 * @brief `char` 流上 `signed char` 的字符语义读写：`os << sc` / `is >> sc`。
 *
 * 与 `io_traits<char, unsigned char>` 逐条相同，见那里的说明；标准同样只给 `char` 流
 * `operator<<(signed char)` 与 `operator>>(signed char&)`。
 * @endif
 *
 * @lang{EN}
 * @brief Character-semantics reading and writing of `signed char` on a `char` stream:
 *        `os << sc` / `is >> sc`.
 *
 * Identical point for point to `io_traits<char, unsigned char>`; see the notes there. The
 * standard likewise gives `operator<<(signed char)` and `operator>>(signed char&)` to `char`
 * streams only.
 * @endif
 */
template <>
struct io_traits<char, signed char>
{
    /**
     * @lang{ZH}
     * @brief 按字节转交 `io_traits<char, char>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 要写出的字符。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Forwards the byte to `io_traits<char, char>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c The character to write.
     * @return The output iterator past what was written.
     * @throw stream_error If the required fill count exceeds `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, signed char c)
    {
        return io_traits<char, char>::swrite(iter, io, loc, static_cast<char>(c));
    }

    /**
     * @lang{ZH}
     * @brief 经 `io_traits<char, char>::sread` 取走下一个字符，按字节写入 @p c。
     *
     * 抛出时 @p c 不被改动。
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param io 未使用，仅为转交。
     * @param loc 未使用，仅为转交。
     * @param c 接收字符的变量。
     * @return 指向被消费字符之后的输入迭代器。
     * @throw stream_error 若输入已到末尾。
     * @endif
     *
     * @lang{EN}
     * @brief Takes the next character through `io_traits<char, char>::sread` and stores the
     *        byte in @p c.
     *
     * @p c is left untouched on a throw.
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param io Unused; passed through.
     * @param loc Unused; passed through.
     * @param c Receives the character.
     * @return An input iterator past the consumed character.
     * @throw stream_error If the input is already at its end.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char& c)
    {
        char tmp{};
        auto res = io_traits<char, char>::sread(iter, iter_end, io, loc, tmp);
        c = tmp;
        return res;
    }
};

/**
 * @lang{ZH}
 * @brief 写出与流同字符类型的 C 风格字符串：`os << str`。
 *
 * 对应标准的 `operator<<(basic_ostream<charT>&, const charT*)`。只有插入方向——提取到裸
 * 指针在本库不存在，理由见 `io_traits<TChar, TChar[N]>`。字符串字面量与字符数组（`TChar[N]`）
 * 最终也走这里：`operator<<` 对数组键找不到可插入的特化后按 `decay_t` 重试，落到本特化或
 * 转交过来的 `io_traits<TChar, const TChar*>`。
 *
 * @note 空指针**抛出**而不是静默跳过。标准把 `s` 为空指针定为未定义行为（libstdc++ 置
 *       `badbit`），本库选择明确报错，经运算符转为状态位。
 * @tparam TChar 流的字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Writes a C-style string of the stream's own character type: `os << str`.
 *
 * Matches the standard's `operator<<(basic_ostream<charT>&, const charT*)`. Insertion only:
 * extraction into a raw pointer does not exist in this library, see `io_traits<TChar, TChar[N]>`
 * for why. String literals and character arrays (`TChar[N]`) end up here too: finding no
 * insertable specialization for the array key, `operator<<` retries with `decay_t` and lands
 * either on this one or on `io_traits<TChar, const TChar*>`, which forwards here.
 *
 * @note A null pointer **throws** rather than being skipped silently. The standard makes a
 *       null `s` undefined behavior (libstdc++ sets `badbit`); this library reports it
 *       explicitly, and the operator turns it into a state bit.
 * @tparam TChar The stream's character type.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, TChar*>
{
    /**
     * @lang{ZH}
     * @brief 数一遍长度后经 `ostream_insert` 写出，按需补齐到字段宽度。
     *
     * `width_guard` 保证因空指针抛出时宽度同样清零。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Measures the length and writes through `ostream_insert`, padded to the field width
     *        where one is set.
     *
     * `width_guard` makes sure the width is zeroed as well when the null-pointer check throws.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, const TChar* c)
    {
        auto width_guard = io.width_guard();
        if (c == nullptr)
            throw IOv2::stream_error("Cannot write NULL character sequence");

        std::size_t n = 0;
        for (const TChar* ptr = c; *ptr != 0; ++ptr, ++n);

        return ostream_insert(iter, io, c, n);
    }
};

/**
 * @lang{ZH}
 * @brief `io_traits<TChar, TChar*>` 的 `const` 指针版本，全部转交后者。
 *
 * 两个键都要有：`operator<<` 按实参的衰退类型查找特化，`const TChar*` 与 `TChar*` 是
 * 不同的键。
 * @tparam TChar 流的字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief The `const`-pointer counterpart of `io_traits<TChar, TChar*>`; forwards everything to
 *        it.
 *
 * Both keys are needed: `operator<<` looks the specialization up by the argument's decayed
 * type, and `const TChar*` and `TChar*` are different keys.
 * @tparam TChar The stream's character type.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, const TChar*>
{
    /**
     * @lang{ZH}
     * @brief 转交 `io_traits<TChar, TChar*>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Forwards to `io_traits<TChar, TChar*>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
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
 *       无需加宽，也就不必为此分配缓冲区。
 * @note 加宽必须先落到一段连续缓冲区再交给 `ostream_insert`，因为补齐要预先知道总宽度。
 *       这是本特化与直接写出的 `io_traits<TChar, TChar*>` 之间唯一的额外代价。
 * @tparam TChar 流的字符类型；为 `char` 时被下面的全特化覆盖。
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
 *       is needed.
 * @note Widening has to land in a contiguous buffer before reaching `ostream_insert`, because
 *       padding needs the total width up front. That is the one extra cost this specialization
 *       carries over the straight-through `io_traits<TChar, TChar*>`.
 * @tparam TChar The stream's character type; for `char` the explicit specialization below
 *         takes precedence.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, char*>
{
    /**
     * @lang{ZH}
     * @brief 把整段窄字符串经 `ctype<TChar>::widen_seq()` 加宽到临时缓冲区，再经
     *        `ostream_insert` 写出。
     *
     * 空指针与 facet 的检查都在分配缓冲区之前；`width_guard` 保证这两处抛出时宽度同样清零。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 提供 `ctype<TChar>` facet 的 locale。
     * @param c 以空字符结尾的窄字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或 locale 中没有 `ctype<TChar>` facet，或所需
     *        填充量超过 `ios_defs::max_pad_count`。
     * @throw std::bad_alloc 若临时缓冲区分配失败。
     * @endif
     *
     * @lang{EN}
     * @brief Widens the whole narrow string through `ctype<TChar>::widen_seq()` into a
     *        temporary buffer, then writes it through `ostream_insert`.
     *
     * Both the null-pointer and the facet check come before the buffer is allocated;
     * `width_guard` makes sure the width is zeroed as well when either throws.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc The locale supplying the `ctype<TChar>` facet.
     * @param c A null-terminated narrow string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the locale carries no `ctype<TChar>` facet, or the
     *        required fill count exceeds `ios_defs::max_pad_count`.
     * @throw std::bad_alloc If the temporary buffer cannot be allocated.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, const char* c)
    {
        auto width_guard = io.width_guard();
        if (c == nullptr)
            throw IOv2::stream_error("Cannot write NULL character sequence");

        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get ctype facet");

        std::size_t n = 0;
        for (const char* ptr = c; *ptr != 0; ++ptr, ++n);

        std::vector<TChar> buf(n);
        mp->widen_seq(c, c + n, buf.data());

        return ostream_insert(iter, io, buf.data(), n);
    }
};

/**
 * @lang{ZH}
 * @brief `io_traits<TChar, char*>` 的 `const` 指针版本，全部转交后者。
 *
 * 字符串字面量（`const char[N]`）经 `operator<<` 的 `decay_t` 重试落到的正是这个键，
 * 所以 `wos << "hi"` 走的是这里。
 * @tparam TChar 流的字符类型；为 `char` 时被下面的全特化覆盖。
 * @endif
 *
 * @lang{EN}
 * @brief The `const`-pointer counterpart of `io_traits<TChar, char*>`; forwards everything to
 *        it.
 *
 * This is the key a string literal (`const char[N]`) lands on after `operator<<`'s `decay_t`
 * retry, so `wos << "hi"` comes through here.
 * @tparam TChar The stream's character type; for `char` the explicit specialization below
 *         takes precedence.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, const char*>
{
    /**
     * @lang{ZH}
     * @brief 转交 `io_traits<TChar, char*>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 提供 `ctype<TChar>` facet 的 locale。
     * @param c 以空字符结尾的窄字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或 locale 中没有 `ctype<TChar>` facet，或所需
     *        填充量超过 `ios_defs::max_pad_count`。
     * @throw std::bad_alloc 若临时缓冲区分配失败。
     * @endif
     *
     * @lang{EN}
     * @brief Forwards to `io_traits<TChar, char*>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc The locale supplying the `ctype<TChar>` facet.
     * @param c A null-terminated narrow string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the locale carries no `ctype<TChar>` facet, or the
     *        required fill count exceeds `ios_defs::max_pad_count`.
     * @throw std::bad_alloc If the temporary buffer cannot be allocated.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>& loc, const char* c)
    {
        return io_traits<TChar, char*>::swrite(iter, io, loc, c);
    }
};

/**
 * @lang{ZH}
 * @brief `char` 流上的窄字符串：`os << "hi"`。
 *
 * 全特化，优先于加宽版的 `io_traits<TChar, char*>`：字符类型已经一致，直接写出，不查
 * locale，不分配缓冲区。它还有一个必须存在的理由：`TChar == char` 时
 * `io_traits<TChar, TChar*>` 与 `io_traits<TChar, char*>` 两个偏特化同样匹配、互不更特化，
 * 没有这个全特化便是二义。同时也是 `io_traits<char, signed char*>` /
 * `io_traits<char, unsigned char*>` 四个特化的最终去处。
 * @endif
 *
 * @lang{EN}
 * @brief A narrow string on a `char` stream: `os << "hi"`.
 *
 * An explicit specialization that beats the widening `io_traits<TChar, char*>`: the character
 * types already agree, so it writes straight through with no locale lookup and no buffer. It
 * also has to exist: with `TChar == char` the partial specializations `io_traits<TChar, TChar*>`
 * and `io_traits<TChar, char*>` match equally well and neither is more specialized, so without
 * it the lookup would be ambiguous. It is also where the four `io_traits<char, signed char*>` / `io_traits<char, unsigned char*>`
 * specializations end up.
 * @endif
 */
template <>
struct io_traits<char, char*>
{
    /**
     * @lang{ZH}
     * @brief 数一遍长度后经 `ostream_insert` 写出，按需补齐到字段宽度。
     *
     * `width_guard` 保证因空指针抛出时宽度同样清零。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Measures the length and writes through `ostream_insert`, padded to the field width
     *        where one is set.
     *
     * `width_guard` makes sure the width is zeroed as well when the null-pointer check throws.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>&, const char* c)
    {
        auto width_guard = io.width_guard();
        if (c == nullptr)
            throw IOv2::stream_error("Cannot write NULL character sequence");

        std::size_t n = 0;
        for (const char* ptr = c; *ptr != 0; ++ptr, ++n);

        return ostream_insert(iter, io, c, n);
    }
};

/**
 * @lang{ZH}
 * @brief `io_traits<char, char*>` 的 `const` 指针版本，全部转交后者。
 * @endif
 *
 * @lang{EN}
 * @brief The `const`-pointer counterpart of `io_traits<char, char*>`; forwards everything to
 *        it.
 * @endif
 */
template <>
struct io_traits<char, const char*>
{
    /**
     * @lang{ZH}
     * @brief 转交 `io_traits<char, char*>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Forwards to `io_traits<char, char*>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
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
    /**
     * @lang{ZH}
     * @brief `reinterpret_cast` 成 `const char*` 后转交 `io_traits<char, char*>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Casts to `const char*` (a `reinterpret_cast`) and forwards to
     *        `io_traits<char, char*>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const unsigned char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
};

/**
 * @lang{ZH}
 * @brief `io_traits<char, unsigned char*>` 的 `const` 指针版本；说明见那里。
 * @endif
 *
 * @lang{EN}
 * @brief The `const`-pointer counterpart of `io_traits<char, unsigned char*>`; see the notes
 *        there.
 * @endif
 */
template <>
struct io_traits<char, const unsigned char*>
{
    /**
     * @lang{ZH}
     * @brief `reinterpret_cast` 成 `const char*` 后转交 `io_traits<char, char*>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Casts to `const char*` (a `reinterpret_cast`) and forwards to
     *        `io_traits<char, char*>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const unsigned char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
};

/**
 * @lang{ZH}
 * @brief `signed char` 字符串写入 `char` 流；与 `io_traits<char, unsigned char*>` 相同，
 *        说明见那里。
 * @endif
 *
 * @lang{EN}
 * @brief A `signed char` string on a `char` stream; identical to
 *        `io_traits<char, unsigned char*>`, see the notes there.
 * @endif
 */
template <>
struct io_traits<char, signed char*>
{
    /**
     * @lang{ZH}
     * @brief `reinterpret_cast` 成 `const char*` 后转交 `io_traits<char, char*>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Casts to `const char*` (a `reinterpret_cast`) and forwards to
     *        `io_traits<char, char*>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const signed char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
};

/**
 * @lang{ZH}
 * @brief `io_traits<char, signed char*>` 的 `const` 指针版本；说明见
 *        `io_traits<char, unsigned char*>`。
 * @endif
 *
 * @lang{EN}
 * @brief The `const`-pointer counterpart of `io_traits<char, signed char*>`; see the notes on
 *        `io_traits<char, unsigned char*>`.
 * @endif
 */
template <>
struct io_traits<char, const signed char*>
{
    /**
     * @lang{ZH}
     * @brief `reinterpret_cast` 成 `const char*` 后转交 `io_traits<char, char*>::swrite`。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param loc 未使用，仅为转交。
     * @param c 以空字符结尾的字符串。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 `c` 为空指针，或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Casts to `const char*` (a `reinterpret_cast`) and forwards to
     *        `io_traits<char, char*>::swrite`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param loc Unused; passed through.
     * @param c A null-terminated string.
     * @return The output iterator past what was written.
     * @throw stream_error If `c` is null, or the required fill count exceeds
     *        `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, char>)
    static TIter swrite(TIter iter, ios_base<char>& io, const locale<char>& loc, const signed char* c)
    {
        return io_traits<char, char*>::swrite(iter, io, loc, reinterpret_cast<const char*>(c)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
};

/**
 * @lang{ZH}
 * @brief 将一个以空白分隔的 token 提取到定长字符数组。
 *
 * @note **本库不提供向裸指针（`TChar*`）提取的 `sread`，`is >> ptr` 无法编译。** 这与
 *       C++20 起的 `std::istream` 一致（P0487R1 删除了 `operator>>(basic_istream&, charT*)`，
 *       只保留数组引用形式）。理由是内存安全：目标是裸指针时库无从得知缓冲区容量，
 *       C++17 及更早的"`width == 0` 即无上界"会让 `is >> ptr` 一路写到遇见空白为止，
 *       输入受攻击者控制时即为可利用的缓冲区溢出。
 * @note 本重载安全的原因是上界 `N` 来自**类型**而非流状态：至多读入
 *       `width == 0 ? N - 1 : min(width, N) - 1` 个字符，再加一个终止符。`width == 0`（默认值）
 *       是「不设宽度」而非「宽度为零」，故上界退回 `N`——把 0 代进 `min` 会得到无意义的结果。
 *       因此 `setw()` 在这里只能把边界**收紧**，
 *       永远不可能放宽；即便携带了来自上一次操作的陈旧 `width`（算术提取、`get_money`、
 *       `get_time` 等都不消费 `width`，与标准一致），也绝不会越过 `N`。
 * @note 需要运行期确定容量的缓冲区，请提取到 `std::basic_string`（自动增长），或改用
 *       非格式化的 `istream::read(s, n)` / `get(s, n)`，二者都显式接收容量。
 * @tparam TChar 流的字符类型。
 * @tparam N 目标数组的长度，含终止符。
 * @endif
 *
 * @lang{EN}
 * @brief Extracts one whitespace-delimited token into a fixed-size character array.
 *
 * @note **This library provides no `sread` for a raw pointer (`TChar*`); `is >> ptr` does not
 *       compile.** This matches `std::istream` as of C++20 (P0487R1 removed
 *       `operator>>(basic_istream&, charT*)`, keeping only the array-reference form). The reason
 *       is memory safety: when the target is a raw pointer the library cannot know the buffer's
 *       capacity, and the rule through C++17 -- "`width == 0` means no bound" -- let `is >> ptr`
 *       write on until whitespace, an exploitable buffer overflow when the input is
 *       attacker-controlled.
 * @note What makes this overload safe is that the bound `N` comes from the **type** rather
 *       than from stream state: at most `width == 0 ? N - 1 : min(width, N) - 1` characters plus a
 *       terminator are stored. A `width` of 0 -- the default -- means "no width set" rather than
 *       "a width of zero", so the bound falls back to `N`; substituting 0 into the `min` would
 *       give a meaningless answer. `setw()` can therefore only **tighten** the bound here, never
 *       loosen it --
 *       even a stale `width` left over from an earlier operation (arithmetic extraction,
 *       `get_money` and `get_time` do not consume `width`, matching the standard) can never
 *       reach past `N`.
 * @note For a buffer whose capacity is only known at run time, extract into a
 *       `std::basic_string` (which grows on demand), or use the unformatted
 *       `istream::read(s, n)` / `get(s, n)`, both of which take the capacity explicitly.
 * @tparam TChar The stream's character type.
 * @tparam N The length of the target array, terminator included.
 * @endif
 */
template <typename TChar, std::size_t N>
struct io_traits<TChar, TChar[N]> // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): the key is the user's array type
{
    /**
     * @lang{ZH}
     * @brief 以 `N` 为容量调用 `istream_extract`。
     *
     * 与标准一致，`width()` 在这里被消费；它只能把上界从 `N` 收紧，不能放宽。
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param io 提供 `width()` 的流。
     * @param loc 提供 `ctype<TChar>` facet 的 locale。
     * @param c 目标缓冲区。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 locale 中没有 `ctype<TChar>` facet，或未提取到任何字符——
     *        `N == 1` 时必然如此，因为这个缓冲区只放得下终止符。**只要本函数被调用**，无论
     *        哪种情形终止符都已写入；哨兵失败时本函数不被调用，数组保持原样，详见
     *        `istream_extract` 的 `@warning`。
     * @endif
     *
     * @lang{EN}
     * @brief Calls `istream_extract` with `N` as the capacity.
     *
     * As in the standard, `width()` is consumed here; it can only tighten the bound from `N`,
     * never loosen it.
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param io The stream supplying `width()`.
     * @param loc The locale supplying the `ctype<TChar>` facet.
     * @param c The destination buffer.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If the locale carries no `ctype<TChar>` facet, or no characters were
     *        extracted -- which `N == 1` always is, that buffer having room for the terminator
     *        alone. The terminator is written either way **provided this function is called at
     *        all**; a failed sentry skips it and leaves the array untouched. See the `@warning`
     *        on `istream_extract`.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, TChar* c)
    {
        constexpr std::size_t n = N;
        return istream_extract(iter, iter_end, io, loc, c, n);
    }
};

/**
 * @lang{ZH}
 * @brief 把一个以空白分隔的词提取到 `char` 流上的 `unsigned char` 定长数组。
 *
 * 对应标准只给 `basic_istream<char>` 的 `operator>>(unsigned char (&)[N])`：按字节原样存入，
 * 不经任何转换，故 `reinterpret_cast` 成 `char*` 后与 `io_traits<TChar, TChar[N]>` 走同一条
 * 路。上界、终止符与哨兵失败时的行为全部同那里的说明，宽流上没有此特化。
 * @tparam N 目标数组的长度，含终止符。
 * @endif
 *
 * @lang{EN}
 * @brief Extracts one whitespace-delimited token into an `unsigned char` fixed-size array on a
 *        `char` stream.
 *
 * Matches the `operator>>(unsigned char (&)[N])` the standard gives only to
 * `basic_istream<char>`: the bytes are stored as they are with no conversion, so after a
 * `reinterpret_cast` to `char*` this takes the same path as `io_traits<TChar, TChar[N]>`. The
 * bound, the terminator and the behavior on a failed sentry are all as described there; wide
 * streams have no such specialization.
 * @tparam N The length of the target array, terminator included.
 * @endif
 */
template <std::size_t N>
struct io_traits<char, unsigned char[N]> // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): the key is the user's array type
{
    /**
     * @lang{ZH}
     * @brief `reinterpret_cast` 成 `char*` 后以 `N` 为容量调用 `istream_extract`。
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param io 提供 `width()` 的流。
     * @param loc 提供 `ctype<char>` facet 的 locale。
     * @param c 目标缓冲区。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 locale 中没有 `ctype<char>` facet，或未提取到任何字符
     *        （`N == 1` 时必然如此）。
     * @endif
     *
     * @lang{EN}
     * @brief Casts to `char*` (a `reinterpret_cast`) and calls `istream_extract` with `N` as
     *        the capacity.
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param io The stream supplying `width()`.
     * @param loc The locale supplying the `ctype<char>` facet.
     * @param c The destination buffer.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If the locale carries no `ctype<char>` facet, or no characters were
     *        extracted (which `N == 1` always is).
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, unsigned char* c)
    {
        constexpr std::size_t n = N;
        return istream_extract(iter, iter_end, io, loc, reinterpret_cast<char*>(c), n); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
};

/**
 * @lang{ZH}
 * @brief 把一个以空白分隔的词提取到 `char` 流上的 `signed char` 定长数组；与
 *        `io_traits<char, unsigned char[N]>` 相同，说明见那里。
 * @tparam N 目标数组的长度，含终止符。
 * @endif
 *
 * @lang{EN}
 * @brief Extracts one whitespace-delimited token into a `signed char` fixed-size array on a
 *        `char` stream; identical to `io_traits<char, unsigned char[N]>`, see the notes there.
 * @tparam N The length of the target array, terminator included.
 * @endif
 */
template <std::size_t N>
struct io_traits<char, signed char[N]> // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): the key is the user's array type
{
    /**
     * @lang{ZH}
     * @brief `reinterpret_cast` 成 `char*` 后以 `N` 为容量调用 `istream_extract`。
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param io 提供 `width()` 的流。
     * @param loc 提供 `ctype<char>` facet 的 locale。
     * @param c 目标缓冲区。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 locale 中没有 `ctype<char>` facet，或未提取到任何字符
     *        （`N == 1` 时必然如此）。
     * @endif
     *
     * @lang{EN}
     * @brief Casts to `char*` (a `reinterpret_cast`) and calls `istream_extract` with `N` as
     *        the capacity.
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param io The stream supplying `width()`.
     * @param loc The locale supplying the `ctype<char>` facet.
     * @param c The destination buffer.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If the locale carries no `ctype<char>` facet, or no characters were
     *        extracted (which `N == 1` always is).
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<char, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<char>& io, const locale<char>& loc, signed char* c)
    {
        constexpr std::size_t n = N;
        return istream_extract(iter, iter_end, io, loc, reinterpret_cast<char*>(c), n); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
};

/**
 * @lang{ZH}
 * @brief 写出 `std::basic_string` 的内容，或把一个以空白分隔的词提取进去。
 *
 * @note 提取**先清空目标**，且清空发生在任何可能抛出的操作之前。输入一侧抛出（迭代器、`ctype`
 *       facet、设备）时留下的是**已消费字符的完整前缀**：提取循环包在 `try` 内，暂存缓冲区在
 *       异常路径上照样追加进去；一个字符都没提取到时才是空串。**例外是追加本身失败**
 *       （`bad_alloc` / `length_error`）：那时暂存区里最多 128 个字符无处可放，`str` 会短于
 *       已消费的量。定长数组形式的前缀保证反而**更强**：那里已提取的字符直接写进调用方数组、
 *       没有暂存区，抛出时只补终止符，因此不存在上面这条例外。
 * @note 上界来自流状态而非类型：`width()` 非 0 时最多提取那么多字符，为 0 时只由空白或 EOF
 *       收尾。`width` 在这里被**消费并清零**，成功与失败都是如此。定长数组形式那条「需要运行期
 *       确定容量就提取到 `basic_string`」指的正是后一种情形。
 * @tparam TChar 流的字符类型，也是字符串的字符类型。
 * @tparam TTraits 字符串的 traits 类型；任意，不参与读写。
 * @tparam TAlloc 字符串的分配器类型；任意。
 * @endif
 *
 * @lang{EN}
 * @brief Writes the contents of a `std::basic_string`, or extracts one whitespace-delimited token
 *        into it.
 *
 * @note Extraction **clears the target first**, and does so before anything that can throw. When the
 *       throw comes from the input side -- the iterator, the `ctype` facet, the device -- what is
 *       left is **the complete prefix of the characters consumed**: the extraction loop sits in a
 *       `try` and the staging buffer is appended on the exception path too. It is empty only when
 *       no character was extracted at all. **The exception is a failing append**
 *       (`bad_alloc` / `length_error`): up to 128 staged characters then have nowhere to go and
 *       `str` ends up shorter than what was consumed. The fixed-size array form's prefix guarantee
 *       is **stronger**: the extracted characters go straight into the caller's array with no
 *       staging buffer, a throw only adds the terminator, and that exception does not arise.
 * @note The bound comes from stream state rather than from the type: a non-zero `width()` caps the
 *       number of characters, and a zero one lets whitespace or EOF decide. `width` is
 *       **consumed and reset** here, on success and on failure alike. This is the case the array
 *       form's "extract into a `basic_string` when the capacity is only known at run time" note
 *       points at.
 * @tparam TChar The stream's character type, which is also the string's.
 * @tparam TTraits The string's traits type; arbitrary, and not involved in reading or writing.
 * @tparam TAlloc The string's allocator type; arbitrary.
 * @endif
 */
template <typename TChar, typename TTraits, typename TAlloc>
struct io_traits<TChar, std::basic_string<TChar, TTraits, TAlloc>>
{
    /**
     * @lang{ZH}
     * @brief 把字符串的全部内容经 `ostream_insert` 写出，按需补齐到字段宽度。
     *
     * 内嵌的空字符照常写出——长度取自 `size()`，不找终止符。`width()` 由 `ostream_insert`
     * 消费。
     * @param iter 输出迭代器。
     * @param io 提供 `width()` / `fill()` / `adjustfield` 的流。
     * @param str 写出的源。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Writes the whole string through `ostream_insert`, padded to the field width where
     *        one is set.
     *
     * Embedded null characters are written like any other: the length is `size()`, and no
     * terminator is looked for. `ostream_insert` consumes `width()`.
     * @param iter The output iterator.
     * @param io The stream supplying `width()`, `fill()` and `adjustfield`.
     * @param str The source to write.
     * @return The output iterator past what was written.
     * @throw stream_error If the required fill count exceeds `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter iter, ios_base<TChar>& io, const locale<TChar>&, const std::basic_string<TChar, TTraits, TAlloc>& str)
    {
        return ostream_insert(iter, io, str.data(), str.size());
    }

    /**
     * @lang{ZH}
     * @brief 清空 @p str，再把一个以空白分隔的词提取进去。
     *
     * 字符先攒在 128 个字符的栈缓冲区里、攒满一批才 `append` 一次，减少对 @p str 的重分配。
     * 上界为 `width()`（非 0 时）或 `str.max_size()`；`width()` 在最前面就被消费。前缀保证
     * 与例外见类说明。
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param io 提供 `width()` 的流。
     * @param loc 提供 `ctype<TChar>` facet 的 locale。
     * @param str 提取的目标；进入时即被清空。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若一个字符都没提取到，或 locale 中没有 `ctype<TChar>` facet。
     * @throw std::bad_alloc 若对 @p str 的追加分配失败。
     * @throw std::length_error 若追加会超过 @p str 的 `max_size()`。
     * @endif
     *
     * @lang{EN}
     * @brief Clears @p str, then extracts one whitespace-delimited token into it.
     *
     * Characters are staged in a 128-character stack buffer and appended a batch at a time, to
     * cut down on reallocations of @p str. The bound is `width()` when non-zero, otherwise
     * `str.max_size()`; `width()` is consumed up front. See the class description for the
     * prefix guarantee and its exception.
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param io The stream supplying `width()`.
     * @param loc The locale supplying the `ctype<TChar>` facet.
     * @param str The target to extract into; it is cleared on entry.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If no characters were extracted, or the locale carries no
     *        `ctype<TChar>` facet.
     * @throw std::bad_alloc If an append to @p str fails to allocate.
     * @throw std::length_error If an append would exceed @p str's `max_size()`.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, std::basic_string<TChar, TTraits, TAlloc>& str)
    {
        str.erase();
        constexpr std::size_t buf_size = 128;
        std::array<TChar, buf_size> buf;
        std::size_t len = 0;
        const std::size_t w = io.width(0);
        const std::size_t n = w > 0 ? w : str.max_size();
        std::size_t extracted = 0;

        auto ct = loc.template get<ctype<TChar>>();
        if (!ct)
            throw stream_error("cannot get ctype facet");
        try
        {
            while (extracted < n && (iter != iter_end))
            {
                const TChar c = *iter;
                if (ct->is_any(base_ft<ctype>::space, c))
                    break;

                if (len == buf_size)
                {
                    str.append(buf.data(), buf_size);
                    len = 0;
                }
                buf[len++] = c;
                ++extracted;
                ++iter;
            }
            str.append(buf.data(), len);
        }
        catch (...)
        {
            // Retried rather than skipped: append is strongly exception-safe, so a failed one
            // left nothing behind and this is the only chance to keep the staged characters.
            try
            {
                str.append(buf.data(), len);
            }
            catch (...) {} // NOLINT(bugprone-empty-catch)
            throw;
        }

        if (extracted == 0)
            throw stream_error("istream extraction fail: no characters extracted");

        return iter;
    }
};
}
