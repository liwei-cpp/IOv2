// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <IOv2/facet/numeric.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/locale/locale.h>

#include <iterator>
#include <limits>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 算术值的数值格式读写。
 *
 * @note 排除的那五个类型是**字符**类型，归 `IOv2/io/traits/char_and_str.h` 管，不是数值。
 *       排除项的作用不止于选择重载，更在于让 `io_traits<char, wchar_t>` 之类**根本不存在**：
 *       类模板没有 `= delete`，这是表达标准所删重载的唯一手段。删掉任何一条，
 *       `os << L'x'` 都会静默打印出一个数字。
 * @note 名单里**没有** `signed char` / `unsigned char`，这是有意的：宽流上它们经整型提升
 *       落到数值插入，必须留给本特化；`char` 流上的两条由 `char_and_str.h` 里的全特化接管，
 *       全特化优先于任何偏特化。
 * @note 约束里那条 `numeric_limits` 判据管的是 C++23 的扩展浮点类型（`std::float16_t` /
 *       `bfloat16_t` / `float32_t` / `float64_t` / `float128_t`）：本特化只接纳**存在标准浮点
 *       类型能精确表示**的那些，与 C++23 `[ostream.inserters.arithmetic]` 一致。装不下的
 *       让特化不存在，因此本机（`long double` 为 x87 80 位）上 `os << std::float128_t{}`
 *       报「没有 `operator<<`」。
 * @endif
 *
 * @lang{EN}
 * @brief Numeric-format reading and writing of arithmetic values.
 *
 * @note The five excluded types are **character** types; they belong to
 *       `IOv2/io/traits/char_and_str.h`, not here. The exclusions do more than steer overload
 *       selection: they are what makes `io_traits<char, wchar_t>` and friends **not exist at
 *       all**. A class template has no `= delete`, so this is the only way to express the
 *       standard's deleted overloads; drop any one of them and `os << L'x'` silently prints a
 *       number.
 * @note `signed char` / `unsigned char` are deliberately **absent** from the list: on a wide
 *       stream they reach numeric insertion through integral promotion and must stay here, while
 *       the `char`-stream cases are taken by the explicit specializations in `char_and_str.h`,
 *       which outrank every partial specialization.
 * @note The `numeric_limits` clause in the constraint is about the C++23 extended
 *       floating-point types (`std::float16_t`, `bfloat16_t`, `float32_t`, `float64_t`,
 *       `float128_t`): this specialization admits only those for which **some standard
 *       floating-point type represents them exactly**, as C++23
 *       `[ostream.inserters.arithmetic]` prescribes. The ones that do not fit get no
 *       specialization at all, so here (`long double` being x87 80-bit)
 *       `os << std::float128_t{}` reports "no `operator<<`".
 * @endif
 */
template <typename TChar, typename TValue>
    requires (std::is_same_v<TValue, std::remove_cv_t<TValue>>
              && std::is_arithmetic_v<TValue>
              && (!std::is_floating_point_v<TValue>
                  || (std::numeric_limits<long double>::digits >= std::numeric_limits<TValue>::digits
                      && std::numeric_limits<long double>::max_exponent
                             >= std::numeric_limits<TValue>::max_exponent
                      && std::numeric_limits<long double>::min_exponent
                             <= std::numeric_limits<TValue>::min_exponent))
              && !std::is_same_v<TValue, char>
              && !std::is_same_v<TValue, wchar_t>
              && !std::is_same_v<TValue, char8_t>
              && !std::is_same_v<TValue, char16_t>
              && !std::is_same_v<TValue, char32_t>)
struct io_traits<TChar, TValue>
{
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, TValue value)
    {
        auto width_guard = io.width_guard();
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->put(s, io, value);
    }

    /**
     * @lang{ZH}
     * @brief 从流中解析一个算术值。
     *
     * @note 本成员比类模板的名单多排除 `signed char` 与 `unsigned char`，这处**不对称是有意
     *       的**。提取按引用传参，拿不到插入侧的整型提升，标准也只为 `char` 流定义了这两个
     *       提取器，因此宽流上的 `wis >> sc` 编译不过，而不是退回去按数值解析。`char` 流上的
     *       两条由 `char_and_str.h` 里的全特化承接。
     * @endif
     *
     * @lang{EN}
     * @brief Parses an arithmetic value from the stream.
     *
     * @note This member excludes `signed char` and `unsigned char` on top of the class
     *       template's list, and that **asymmetry is deliberate**. Extraction takes its target by
     *       reference and gets none of the integral promotion the insertion side enjoys, and the
     *       standard defines those two extractors for `char` streams only, so `wis >> sc` on a
     *       wide stream does not compile instead of falling back to parsing a number. The
     *       `char`-stream cases are taken by the explicit specializations in `char_and_str.h`.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, TValue& value)
        requires (!std::is_same_v<TValue, signed char>
                  && !std::is_same_v<TValue, unsigned char>)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->get(s, s_end, io, value);
    }
};

/**
 * @lang{ZH}
 * @brief 指针的地址格式读写。
 *
 * @note 排除的四个指向类型对应标准里 `= delete` 的字符串重载：`const wchar_t*`（窄流上）、
 *       `const char8_t*`、`const char16_t*`、`const char32_t*`。排除之后没有任何 `io_traits`
 *       匹配，`os << L"hi"` 编译不过，而不是静默打出一个地址、流还留在 `good()`。
 *       类模板没有 `= delete`，「根本没有 `io_traits`」是表达它的唯一手段。
 * @note 判的是 `remove_const_t` 而**不是** `remove_cv_t`：`const wchar_t*` 被标准删掉，
 *       `const volatile wchar_t*` 没有，后者按地址打印。指向 volatile 内存的字符串本就无法
 *       安全打印——找终止符与搬字符是两趟，两趟之间内容可以变。
 * @note 第一个约束判的是**键本身**的顶层 cv，与上一条无关：`is_pointer_v` 对顶层 cv 不敏感，
 *       而 `char_and_str.h` 里的字符串特化敏感，少了它 `char* volatile` 会被本特化接住打地址，
 *       而标准打的是内容。加上之后本特化拒绝，`operator<<` 改用 `decay_t` 重试，得到与无限定键
 *       一致的答案。只有 `volatile` 会这样漏进来——`operator<<` 的形参是 `const TValue&`。
 * @note 指向函数的指针也被排除，因此 `os << setw`（漏写实参列表）编译不过，而不是打出一个
 *       地址。用户自写的操纵符走 `operator<<(T&, void (*)(ios_base<TChar>&))`，不经过本特化。
 * @note `volatile int*` 之类照样打得出地址（`swrite` 会把 `volatile` 转掉），与 C++23 P1147R1
 *       给 `basic_ostream` 添加的 `operator<<(const volatile void*)` 一致。地址只被格式化，
 *       从不解引用。
 * @note 指向类型与流的 `char_type` 一致时（`wos << L"hi"`），`io_traits<TChar, const TChar*>`
 *       更特化，本特化本来就轮不到；`char` / `signed char` / `unsigned char` 的字符串重载
 *       同理由 `char_and_str.h` 承接，因此不在排除名单里。
 * @endif
 *
 * @lang{EN}
 * @brief Address-format reading and writing of pointers.
 *
 * @note The four excluded pointees correspond to the string overloads the standard `= delete`s:
 *       `const wchar_t*` (on a narrow stream), `const char8_t*`, `const char16_t*` and
 *       `const char32_t*`. With them excluded no `io_traits` matches, so `os << L"hi"` does not
 *       compile instead of silently printing an address while the stream stays `good()`. A class
 *       template has no `= delete`, so "there is no `io_traits` at all" is the only way to
 *       express it.
 * @note The list keys on `remove_const_t`, **not** `remove_cv_t`: the standard deletes
 *       `const wchar_t*` but not `const volatile wchar_t*`, which prints as an address. A string
 *       in volatile memory cannot be printed safely anyway -- finding the terminator and copying
 *       the characters are two passes, and the contents may change in between.
 * @note The first constraint is about top-level cv on the **key itself** and is unrelated to the
 *       note above: `is_pointer_v` ignores top-level cv while the string specializations in
 *       `char_and_str.h` do not, so without it `char* volatile` would be caught here and printed
 *       as an address where the standard prints the contents. With it this specialization
 *       declines, `operator<<` retries with `decay_t`, and the answer matches the unqualified
 *       key. Only `volatile` can leak in this way: `operator<<` takes `const TValue&`.
 * @note Pointers to functions are excluded too, so `os << setw` with the argument list left off
 *       does not compile instead of printing an address. A user-written manipulator goes through
 *       `operator<<(T&, void (*)(ios_base<TChar>&))` and never reaches this specialization.
 * @note `volatile int*` and friends still print as addresses (`swrite` casts the `volatile`
 *       away), matching the `operator<<(const volatile void*)` that C++23 P1147R1 added to
 *       `basic_ostream`. The address is only formatted, never dereferenced.
 * @note When the pointee matches the stream's `char_type` (`wos << L"hi"`),
 *       `io_traits<TChar, const TChar*>` is more specialized and this one was never in the
 *       running; the string overloads for `char` / `signed char` / `unsigned char` are taken by
 *       `char_and_str.h` for the same reason, which is why they are not on the exclusion list.
 * @endif
 */
template <typename TChar, typename TValue>
    requires (std::is_same_v<TValue, std::remove_cv_t<TValue>>
              && std::is_pointer_v<TValue>
              && !std::is_function_v<std::remove_pointer_t<TValue>>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, wchar_t>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, char8_t>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, char16_t>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, char32_t>)
struct io_traits<TChar, TValue>
{
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, TValue value)
    {
        auto width_guard = io.width_guard();
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->put(s, io, const_cast<const void*>(static_cast<const volatile void*>(value)));
    }

    /**
     * @lang{ZH}
     * @brief 从流中解析一个地址，写入 `void*`。
     *
     * @note 与 `swrite` 不同，`sread` **只接受 `void*`**，不接受任意指针类型，与标准一致——
     *       `std::istream` 只有 `operator>>(void*&)`。因此 `is >> intptr` / `is >> charptr`
     *       没有可行重载，编译不过，而不是把文本地址 `reinterpret_cast` 成野指针，
     *       也不会把字符缓冲区的提取悄悄变成地址解析。
     * @note 这处收窄由**两处**共同表达：`requires` 里的 `is_same_v<TValue, void*>` 钉住键，
     *       形参 `void*&` 钉住目标。走 `operator>>` 时任一处都够，显式限定调用则需要两处齐备，
     *       所以任何一处都不是冗余的。
     * @endif
     *
     * @lang{EN}
     * @brief Parses an address from the stream into a `void*`.
     *
     * @note Unlike `swrite`, `sread` accepts **`void*` only**, not an arbitrary pointer type,
     *       matching the standard, where `std::istream` provides only `operator>>(void*&)`.
     *       So `is >> intptr` / `is >> charptr` have no viable overload and do not compile,
     *       rather than `reinterpret_cast`ing a textual address into a wild pointer or silently
     *       turning character-buffer extraction into address parsing.
     * @note The narrowing is expressed in **two** places: `is_same_v<TValue, void*>` in the
     *       requires-clause pins the key, and the `void*&` parameter pins the target. Either one
     *       suffices for `operator>>`; an explicitly qualified call needs both, so neither is
     *       redundant.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type> &&
                  std::is_same_v<TValue, void*>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, void*& value)
    {
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->get(s, s_end, io, value);
    }
};
}
