// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

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
 * @throw stream_error 若 locale 中没有 `ctype<TChar>` facet。
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
 * @throw stream_error If the locale carries no `ctype<TChar>` facet.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, std::nullptr_t>
{
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
