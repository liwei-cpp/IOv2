// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <cstddef>
#include <string>

#include <type_traits>
#include <IOv2/facet/ctype.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/locale/locale.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 把空指针字面量写成文本。
 *
 * @note 输出经由 `ostream_insert`，而不是直接写进输出迭代器。这与标准一致：
 *       `std::basic_ostream::operator<<(nullptr_t)` 的规定是 `return *this << s;`
 *       （`s` 为实现定义的 NTBS），即转交给字符串插入器。由此继承到两条行为——按
 *       `width()` 和 `fill()` 补齐（遵守 `adjustfield`），以及**完成后把 `width()` 清零**。
 * @note 清零那条不是可有可无的收尾。`width` 是一次性的、跨操作可见的流状态；若某个
 *       `swrite` 不消费它，残留值会漏给**下一次**插入，让一个与 `nullptr` 无关的输出被
 *       莫名其妙地补齐。走 `ostream_insert` 是本库落实"格式化输出函数"这一契约的方式，
 *       但它只能覆盖真正走到它的那条路：取不到 facet 时本函数在抛出前自己清零，
 *       否则这条失败路径就成了契约的缺口。
 * @note 文本 `"nullptr"` 在标准里是实现定义的。它只用到基本源字符集，因此
 *       `ctype<TChar>::widen()` 对任何 `TChar` 都有定义良好的结果。
 * @throw stream_error 若 locale 中没有 `ctype<TChar>` facet。
 * @endif
 *
 * @lang{EN}
 * @brief Writes the null pointer literal as text.
 *
 * @note The output goes through `ostream_insert` rather than straight into the output
 *       iterator. This matches the standard: `std::basic_ostream::operator<<(nullptr_t)`
 *       is specified as `return *this << s;` (with `s` an implementation-defined NTBS),
 *       i.e. it delegates to the string inserter. Two behaviours are inherited from
 *       that -- padding to `width()` with `fill()` (honouring `adjustfield`), and
 *       **resetting `width()` to 0** on completion.
 * @note The reset is not optional housekeeping. `width` is one-shot stream state that is
 *       visible across operations; an `swrite` that fails to consume it leaks the leftover
 *       value into the **next** insertion, padding some unrelated output for no visible
 *       reason. Going through `ostream_insert` is how the "formatted output function" contract
 *       is honoured throughout this library, but it can only cover the path that actually
 *       reaches it: when the facet is missing this function spends the width itself before
 *       throwing, or that failure path would be a hole in the contract.
 * @note The text `"nullptr"` is implementation-defined per the standard. It uses only the
 *       basic source character set, so `ctype<TChar>::widen()` is well defined for it for
 *       any `TChar`.
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
        const char* c_buf = "nullptr";
        constexpr std::size_t n = 7;

        auto mp = loc.template get<ctype<TChar>>();
        if (!mp)
        {
            // This path never reaches ostream_insert, so it has to spend the width itself.
            io.width(0);
            throw stream_error("cannot get ctype facet");
        }

        TChar buf[n];
        mp->widen_seq(c_buf, c_buf + n, buf);

        return ostream_insert(s, io, buf, n);
    }
};
}
