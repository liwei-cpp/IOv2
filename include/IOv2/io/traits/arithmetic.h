// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file arithmetic.h
 * @lang{ZH}
 * 为算术类型与指针类型特化 `io_traits`，提供数值格式与地址格式的读写。
 *
 * 两个特化都把实际的格式化与解析交给 locale 里的 `numeric<TChar>` facet，自身只做三件事：
 * 取 facet、经 `width_guard` 兑现"字段宽度一次性"的约定，以及用 `requires` 子句把字符类型和
 * 标准 `= delete` 掉的重载排除在外——后一点是本文件大部分说明的主题。`signed char` /
 * `unsigned char` 及其指针在 `char` 流上的字符语义由 `IOv2/io/traits/char_and_str.h` 提供，
 * 本文件只在那份头缺席时让相应表达式**编译不过**，而不是静默按数值写出。
 * @endif
 *
 * @lang{EN}
 * Specializes `io_traits` for arithmetic and pointer types, providing numeric-format and
 * address-format reading and writing.
 *
 * Both specializations hand the actual formatting and parsing to the locale's `numeric<TChar>`
 * facet and do only three things themselves: fetch the facet, honor the "field width is one-shot"
 * contract through `width_guard`, and use their requires-clauses to keep character types and the
 * overloads the standard deletes out -- the latter is what most of the commentary in this file
 * is about. The character semantics of `signed char` / `unsigned char` and their pointers on a
 * `char` stream live in `IOv2/io/traits/char_and_str.h`; this file merely makes those expressions
 * **fail to compile** when that header is absent, rather than silently writing a number.
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
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
 *       <tt>os << L'x'</tt> 都会静默打印出一个数字。
 * @note `signed char` / `unsigned char` 的排除**挂着 `TChar == char` 这个条件**，因为这两个
 *       类型是二维的：`char` 流上它们是字符，归 `char_and_str.h`；宽流上标准没有它们的重载，
 *       它们经整型提升落到数值插入，必须留在本特化。条件排除让缺少 `char_and_str.h` 时
 *       `os << (signed char)` 成为**编译错误**，而不是静默按数值写出。
 * @warning **写出时本库不做整型提升**，按 `TValue` 自身的位宽格式化，因此宽流上负的
 *          `signed char` 在 `hex` / `oct` 下比标准短一半：`wos << hex << (signed char)-1` 这里
 *          写 `ff`，标准写 `ffffffff`（提升到 `int` 后按 LWG 23 重解释为 `unsigned int`）。
 *          `short` 等有自己重载的类型不受影响——标准按被选中重载的形参宽度重解释，恰与
 *          `make_unsigned_t<TValue>` 相同；`unsigned char` 提升后仍非负，数字也相同。
 *          这一格是刻意保留的分歧：读侧两边都不接受宽流上的 `signed char`，复刻标准换不到
 *          互操作性，而 8 位写法用 `int` 读得回来（`ff` → 255 → `-1`），32 位写法会溢出
 *          `int` 并置 `strfailbit`。
 * @note 约束里那条 `numeric_limits` 判据管的是 C++23 的扩展浮点类型（`std::float16_t` /
 *       `bfloat16_t` / `float32_t` / `float64_t` / `float128_t`）：本特化只接纳**存在标准浮点
 *       类型能精确表示**的那些，与 C++23 `[ostream.inserters.arithmetic]` 一致。装不下的
 *       让特化不存在，因此本机（`long double` 为 x87 80 位）上 `os << std::float128_t{}`
 *       报「没有 `operator<<`」。
 * @tparam TChar 流的字符类型。
 * @tparam TValue 算术类型；须不带顶层 cv 限定，且不是上述被排除的字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Numeric-format reading and writing of arithmetic values.
 *
 * @note The five excluded types are **character** types; they belong to
 *       `IOv2/io/traits/char_and_str.h`, not here. The exclusions do more than steer overload
 *       selection: they are what makes `io_traits<char, wchar_t>` and friends **not exist at
 *       all**. A class template has no `= delete`, so this is the only way to express the
 *       standard's deleted overloads; drop any one of them and <tt>os << L'x'</tt> silently prints a
 *       number.
 * @note The exclusion of `signed char` / `unsigned char` is **conditioned on `TChar == char`**,
 *       because those two types are two-dimensional: on a `char` stream they are characters and
 *       belong to `char_and_str.h`, while on a wide stream the standard has no overload for them,
 *       so they reach numeric insertion through integral promotion and must stay here. Making the
 *       exclusion conditional is what turns `os << (signed char)` without `char_and_str.h` into a
 *       **compile error** rather than a silent numeric write.
 * @warning **This library does not promote on the way out**; it formats at the width of `TValue`
 *          itself. A negative `signed char` on a wide stream is therefore half as wide as the
 *          standard's under `hex` / `oct`: `wos << hex << (signed char)-1` writes `ff` here and
 *          `ffffffff` in the standard, which promotes to `int` and reinterprets it as
 *          `unsigned int` per LWG 23. Types with an overload of their own, `short` among them, are
 *          unaffected -- the standard reinterprets at the width of the selected overload's
 *          parameter, which is exactly `make_unsigned_t<TValue>`; `unsigned char` stays
 *          non-negative under promotion and prints the same digits either way. The divergence is
 *          kept on purpose: neither side accepts `signed char` for extraction from a wide stream,
 *          so matching the standard buys no interoperability, while the 8-bit form reads back
 *          through `int` (`ff` -> 255 -> `-1`) where the 32-bit one overflows `int` and sets
 *          `strfailbit`.
 * @note The `numeric_limits` clause in the constraint is about the C++23 extended
 *       floating-point types (`std::float16_t`, `bfloat16_t`, `float32_t`, `float64_t`,
 *       `float128_t`): this specialization admits only those for which **some standard
 *       floating-point type represents them exactly**, as C++23
 *       `[ostream.inserters.arithmetic]` prescribes. The ones that do not fit get no
 *       specialization at all, so here (`long double` being x87 80-bit)
 *       `os << std::float128_t{}` reports that there is no `operator<<`.
 * @tparam TChar The stream's character type.
 * @tparam TValue An arithmetic type; must carry no top-level cv-qualifier and must not be one of
 *         the excluded character types above.
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
              && !std::is_same_v<TValue, char32_t>
              && !(std::is_same_v<TChar, char> && std::is_same_v<TValue, signed char>)
              && !(std::is_same_v<TChar, char> && std::is_same_v<TValue, unsigned char>))
struct io_traits<TChar, TValue>
{
    /**
     * @lang{ZH}
     * @brief 把一个算术值按数值格式写出。
     *
     * 基数、符号、精度、浮点记法、分组以及按 `width()` / `fill()` / `adjustfield` 补齐，
     * 全部由 locale 的 `numeric<TChar>` facet 依据 @p io 的状态完成；本函数只取 facet 并
     * 转交。`width()` 由 facet 消费，`width_guard` 保证取不到 facet 而抛出时也同样清零。
     *
     * @param s 输出迭代器。
     * @param io 提供格式标志、精度、宽度与填充字符的流。
     * @param loc 提供 `numeric<TChar>` facet 的 locale。
     * @param value 要写出的值，按 `TValue` 自身的位宽格式化（见类说明中的 `@warning`）。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 locale 中没有 `numeric<TChar>` facet。
     * @endif
     *
     * @lang{EN}
     * @brief Writes an arithmetic value in numeric format.
     *
     * Base, sign, precision, floating-point notation, grouping, and padding to `width()` with
     * `fill()` per `adjustfield` are all done by the locale's `numeric<TChar>` facet from the
     * state of @p io; this function only fetches the facet and delegates. The facet consumes
     * `width()`; `width_guard` makes sure it is zeroed as well when the facet is missing and
     * the function throws.
     *
     * @param s The output iterator.
     * @param io The stream supplying format flags, precision, width and fill character.
     * @param loc The locale supplying the `numeric<TChar>` facet.
     * @param value The value to write, formatted at the width of `TValue` itself (see the
     *              `@warning` in the class description).
     * @return The output iterator past what was written.
     * @throw stream_error If the locale carries no `numeric<TChar>` facet.
     * @endif
     */
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
     * 解析全部交给 locale 的 `numeric<TChar>` facet，它按 @p io 的 `basefield` 等标志识别
     * 记法、按 locale 规则处理分组，解析失败或溢出时抛出。与标准一致，本函数**不消费**
     * `width()`。
     *
     * @note 本成员比类模板的名单多排除 `signed char` 与 `unsigned char`，这处**不对称是有意
     *       的**。提取按引用传参，拿不到插入侧的整型提升，标准也只为 `char` 流定义了这两个
     *       提取器，因此宽流上的 `wis >> sc` 编译不过，而不是退回去按数值解析。`char` 流上的
     *       两条已由类模板的名单排除，归 `char_and_str.h`。
     * @param s 输入迭代器。
     * @param s_end 输入哨位。
     * @param io 提供 `basefield` 等解析标志的流。
     * @param loc 提供 `numeric<TChar>` facet 的 locale。
     * @param value 接收结果的变量；溢出时按 LWG 23 置为 `numeric_limits` 的极值后再抛出。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 locale 中没有 `numeric<TChar>` facet，或解析失败（含溢出）。
     * @endif
     *
     * @lang{EN}
     * @brief Parses an arithmetic value from the stream.
     *
     * Parsing is done entirely by the locale's `numeric<TChar>` facet, which recognizes the
     * notation from the `basefield` and related flags of @p io, applies the locale's grouping
     * rules, and throws on a parse failure or an overflow. As in the standard, this function
     * does **not** consume `width()`.
     *
     * @note This member excludes `signed char` and `unsigned char` on top of the class
     *       template's list, and that **asymmetry is deliberate**. Extraction takes its target by
     *       reference and gets none of the integral promotion the insertion side enjoys, and the
     *       standard defines those two extractors for `char` streams only, so `wis >> sc` on a
     *       wide stream does not compile instead of falling back to parsing a number. The
     *       `char`-stream cases are already off the class template's list and belong to
     *       `char_and_str.h`.
     * @param s The input iterator.
     * @param s_end The input sentinel.
     * @param io The stream supplying `basefield` and the other parsing flags.
     * @param loc The locale supplying the `numeric<TChar>` facet.
     * @param value Receives the result; on overflow it is set to the `numeric_limits` extreme
     *              per LWG 23 before the throw.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If the locale carries no `numeric<TChar>` facet, or parsing fails
     *        (overflow included).
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
 *       更特化，本特化本来就轮不到。`char` 被指类型**无条件排除**——`char_and_str.h` 对任何流
 *       都提供加宽特化，宽流上标准也写文本。`signed char` / `unsigned char` 被指类型只在
 *       `char` 流上排除：宽流上标准没有它们的字符串重载，落到 `operator<<(const void*)` 打地址，
 *       正是本特化该做的。三处排除都键在 `remove_const_t` 上，故 `volatile char*` 仍打地址。
 * @tparam TChar 流的字符类型。
 * @tparam TValue 对象指针类型；须不带顶层 cv 限定，且被指类型不在上述排除名单内。
 * @endif
 *
 * @lang{EN}
 * @brief Address-format reading and writing of pointers.
 *
 * @note The four excluded pointees correspond to the string overloads the standard deletes
 *       (`= delete`):
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
 *       running. A `char` pointee is excluded **unconditionally**: `char_and_str.h` supplies a
 *       widening specialization for every stream, and the standard writes text on a wide stream
 *       too. `signed char` / `unsigned char` pointees are excluded only on a `char` stream: on a
 *       wide one the standard has no string overload for them, so they fall to
 *       `operator<<(const void*)` and print an address, which is exactly this specialization's
 *       job. All three keys are `remove_const_t`, so `volatile char*` still prints an address.
 * @tparam TChar The stream's character type.
 * @tparam TValue An object pointer type; must carry no top-level cv-qualifier, and its pointee
 *         must not be on the exclusion list above.
 * @endif
 */
template <typename TChar, typename TValue>
    requires (std::is_same_v<TValue, std::remove_cv_t<TValue>>
              && std::is_pointer_v<TValue>
              && !std::is_function_v<std::remove_pointer_t<TValue>>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, char>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, wchar_t>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, char8_t>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, char16_t>
              && !std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, char32_t>
              && !(std::is_same_v<TChar, char>
                   && std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, signed char>)
              && !(std::is_same_v<TChar, char>
                   && std::is_same_v<std::remove_const_t<std::remove_pointer_t<TValue>>, unsigned char>))
struct io_traits<TChar, TValue>
{
    /**
     * @lang{ZH}
     * @brief 把一个指针的地址按十六进制写出。
     *
     * 指针先被转成 `const void*`（`volatile` 一并转掉）再交给 locale 的 `numeric<TChar>`
     * facet，补齐与 `width()` 的消费也由 facet 完成；`width_guard` 保证取不到 facet 而抛出时
     * 宽度同样清零。地址只被格式化，从不解引用，因此悬空指针也能安全写出。
     *
     * @param s 输出迭代器。
     * @param io 提供格式标志、宽度与填充字符的流。
     * @param loc 提供 `numeric<TChar>` facet 的 locale。
     * @param value 要写出地址的指针。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 locale 中没有 `numeric<TChar>` facet。
     * @endif
     *
     * @lang{EN}
     * @brief Writes a pointer's address in hexadecimal.
     *
     * The pointer is converted to `const void*` first (casting any `volatile` away) and handed
     * to the locale's `numeric<TChar>` facet, which also does the padding and consumes
     * `width()`; `width_guard` makes sure the width is zeroed as well when the facet is missing
     * and the function throws. The address is only formatted, never dereferenced, so a dangling
     * pointer is safe to write.
     *
     * @param s The output iterator.
     * @param io The stream supplying format flags, width and fill character.
     * @param loc The locale supplying the `numeric<TChar>` facet.
     * @param value The pointer whose address is written.
     * @return The output iterator past what was written.
     * @throw stream_error If the locale carries no `numeric<TChar>` facet.
     * @endif
     */
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
     * 解析交给 locale 的 `numeric<TChar>` facet，按十六进制整数读入并转成指针。与标准一致，
     * 本函数**不消费** `width()`。
     *
     * @note 与 `swrite` 不同，`sread` **只接受 `void*`**，不接受任意指针类型，与标准一致——
     *       `std::istream` 只有 `operator>>(void*&)`。因此 `is >> intptr` / `is >> charptr`
     *       没有可行重载，编译不过，而不是把文本地址 `reinterpret_cast` 成野指针，
     *       也不会把字符缓冲区的提取悄悄变成地址解析。
     * @note 这处收窄由**两处**共同表达：`requires` 里的 `is_same_v<TValue, void*>` 钉住键，
     *       形参 `void*&` 钉住目标。走 `operator>>` 时任一处都够，显式限定调用则需要两处齐备，
     *       所以任何一处都不是冗余的。
     * @param s 输入迭代器。
     * @param s_end 输入哨位。
     * @param io 提供解析标志的流。
     * @param loc 提供 `numeric<TChar>` facet 的 locale。
     * @param value 接收解析出的地址。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 locale 中没有 `numeric<TChar>` facet，或解析失败。
     * @endif
     *
     * @lang{EN}
     * @brief Parses an address from the stream into a `void*`.
     *
     * Parsing is done by the locale's `numeric<TChar>` facet, which reads a hexadecimal integer
     * and converts it to a pointer. As in the standard, this function does **not** consume
     * `width()`.
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
     * @param s The input iterator.
     * @param s_end The input sentinel.
     * @param io The stream supplying the parsing flags.
     * @param loc The locale supplying the `numeric<TChar>` facet.
     * @param value Receives the parsed address.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If the locale carries no `numeric<TChar>` facet, or parsing fails.
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
