// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file nullptr.h
 * @lang{ZH}
 * 为 `std::nullptr_t` 特化 `io_traits`，把空指针字面量写成文本 `nullptr`。
 *
 * 只有插入方向：标准的 `basic_ostream` 自 C++17 起有 `operator<<(nullptr_t)`，而
 * `basic_istream` 没有对应的提取器，故本特化不提供 `sread`，`is >> nullptr` 编译不过。
 * 之所以要单列一个特化，是因为 `std::nullptr_t` 既不是算术类型也不是指针类型，
 * `IOv2/io/traits/arithmetic.h` 的两个特化都接不住它。
 * @endif
 *
 * @lang{EN}
 * Specializes `io_traits` for `std::nullptr_t`, writing the null pointer literal as the text
 * `nullptr`.
 *
 * Insertion only: the standard's `basic_ostream` has had `operator<<(nullptr_t)` since C++17,
 * while `basic_istream` has no matching extractor, so this specialization provides no `sread`
 * and `is >> nullptr` does not compile. A separate specialization is needed because
 * `std::nullptr_t` is neither an arithmetic nor a pointer type, so neither specialization in
 * `IOv2/io/traits/arithmetic.h` can take it.
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/facet/ctype.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/locale/locale.h>

#include <cstddef>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 把空指针字面量写成文本。
 *
 * @note 输出经由 `ostream_insert`，与标准把 `operator<<(nullptr_t)` 规定为转交字符串插入器
 *       一致。由此得到两条行为：按 `width()` 和 `fill()` 补齐（遵守 `adjustfield`），
 *       以及**完成后把 `width()` 清零**。取不到 facet 而抛出时同样清零，不把宽度漏给下一次插入。
 * @note 文本 `"nullptr"` 在标准里是实现定义的，本库固定用这七个字符；它们都属于基本源字符集，
 *       因此 `ctype<TChar>::widen()` 对任何 `TChar` 都有定义良好的结果。
 * @tparam TChar 流的字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Writes the null pointer literal as text.
 *
 * @note The output goes through `ostream_insert`, matching the standard's specification of
 *       `operator<<(nullptr_t)` as a delegation to the string inserter. Two behaviors follow:
 *       padding to `width()` with `fill()` (honoring `adjustfield`), and **resetting `width()`
 *       to 0** on completion. A throw from a missing facet resets it too, so the width is never
 *       leaked into the next insertion.
 * @note The text `"nullptr"` is implementation-defined per the standard; this library always
 *       uses those seven characters. They lie in the basic source character set, so
 *       `ctype<TChar>::widen()` is well defined for them for any `TChar`.
 * @tparam TChar The stream's character type.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, std::nullptr_t>
{
    /**
     * @lang{ZH}
     * @brief 把 `nullptr` 七个字符加宽后经 `ostream_insert` 写出。
     *
     * 加宽用 locale 的 `ctype<TChar>` facet 一次完成（`widen_seq`），落到栈上的定长缓冲区，
     * 再交给 `ostream_insert` 按 `width()` / `fill()` / `adjustfield` 补齐。`width()` 由
     * `ostream_insert` 消费，`width_guard` 保证取不到 facet 而抛出时也同样清零。
     *
     * @param s 输出迭代器。
     * @param io 提供宽度、填充字符与对齐标志的流。
     * @param loc 提供 `ctype<TChar>` facet 的 locale。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 locale 中没有 `ctype<TChar>` facet，或所需填充量超过
     *        `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Widens the seven characters of `nullptr` and writes them through `ostream_insert`.
     *
     * Widening is done in one call to the locale's `ctype<TChar>` facet (`widen_seq`) into a
     * fixed-size buffer on the stack, which is then handed to `ostream_insert` for padding to
     * `width()` with `fill()` per `adjustfield`. `ostream_insert` consumes `width()`;
     * `width_guard` makes sure it is zeroed as well when the facet is missing and the function
     * throws.
     *
     * @param s The output iterator.
     * @param io The stream supplying width, fill character and adjustment flags.
     * @param loc The locale supplying the `ctype<TChar>` facet.
     * @return The output iterator past what was written.
     * @throw stream_error If the locale carries no `ctype<TChar>` facet, or the required fill
     *        count exceeds `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, std::nullptr_t)
    {
        auto width_guard = io.width_guard();
        const char* c_buf = "nullptr";
        constexpr std::size_t n = 7;

        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("cannot get ctype facet");

        TChar buf[n];
        mp->widen_seq(c_buf, c_buf + n, buf);

        return ostream_insert(s, io, buf, n);
    }
};
}
