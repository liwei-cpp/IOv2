// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <iterator>
#include <type_traits>
#include <IOv2/io/io_base.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/facet/numeric.h>
#include <IOv2/locale/locale.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 算术值的数值格式读写。
 *
 * @note 排除的那五个类型是**字符**类型，它们归 `IOv2/io/traits/char_and_str.h` 管，不是数值。
 *       依据是 C++23 `[ostream.inserters.character]` 的重载集：`char` 由对 `charT` 模板化的
 *       `operator<<(basic_ostream<charT>&, char)` 承接，会被加宽后写入任意流；`wchar_t`、
 *       `char8_t`、`char16_t`、`char32_t` 要么与流的 `char_type` 一致（由
 *       `io_traits<TChar, TChar>` 承接），要么在标准里是 `= delete` 的重载。两种情况都轮不到
 *       这里。
 * @note 名单里**没有** `signed char` / `unsigned char`，这是有意的。标准只为 `char` 流提供
 *       这两个字符重载；在宽流上它们经由整型提升落到 `operator<<(int)`，就是数值插入，
 *       必须留给本特化。`char` 流上的那两条由 `char_and_str.h` 里的**全特化**
 *       `io_traits<char, signed char>` / `io_traits<char, unsigned char>` 接管——全特化优先于
 *       任何偏特化，所以这里不需要、也不应该再写一条 `TChar == char` 的排除项。
 * @note 排除项的作用不止于选择重载，更在于让 `io_traits<char, wchar_t>` 之类**根本不存在**。
 *       类模板没有 `= delete`，把标准删除的重载表达为"根本没有 `io_traits`"是唯一的手段；
 *       删掉任何一条，`os << L'x'` 都会退回去静默打印一个数字。
 * @endif
 *
 * @lang{EN}
 * @brief Numeric-format reading and writing of arithmetic values.
 *
 * @note The five excluded types are **character** types; they belong to
 *       `IOv2/io/traits/char_and_str.h`, not here. The basis is the overload set in C++23
 *       `[ostream.inserters.character]`: `char` is taken by
 *       `operator<<(basic_ostream<charT>&, char)`, which is templated on `charT` and widens
 *       into a stream of any type; `wchar_t`, `char8_t`, `char16_t` and `char32_t` either
 *       match the stream's `char_type` (and are taken by `io_traits<TChar, TChar>`) or have a
 *       `= delete`d overload in the standard. Neither case leaves work for this specialization.
 * @note `signed char` / `unsigned char` are deliberately **absent** from the list. The
 *       standard provides those two character overloads for `char` streams only; on a wide
 *       stream they reach `operator<<(int)` through integral promotion, which is numeric
 *       insertion and must stay here. The `char`-stream cases are taken by the **explicit
 *       specializations** `io_traits<char, signed char>` / `io_traits<char, unsigned char>` in
 *       `char_and_str.h` -- an explicit specialization outranks every partial one, so no
 *       `TChar == char` exclusion is needed here, and none should be added.
 * @note The exclusions do more than steer overload selection: they are what makes
 *       `io_traits<char, wchar_t>` and friends **not exist at all**. A class template has no
 *       `= delete`, so "there is no `io_traits` at all" is the only way to express the
 *       standard's deleted overloads; drop any one of them and `os << L'x'` silently falls
 *       back to printing a number.
 * @endif
 */
template <typename TChar, typename TValue>
    requires (std::is_same_v<TValue, std::remove_cv_t<TValue>>
              && std::is_arithmetic_v<TValue>
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
     *       的**，不要"顺手改齐"。原因是提取按引用传参，因而拿不到插入侧的整型提升：
     *       `is >> sc` 里的 `signed char&` 绑不到任何 `operator>>(int&)`，标准也只为 `char` 流
     *       定义了 `operator>>(basic_istream<char>&, signed char&)` 与 `unsigned char&` 两个
     *       提取器（见 `[istream.extractors]`）。于是宽流上 `wis >> sc` 在标准里是非良构的，
     *       本库据此让它没有 `sread` 可匹配；`char` 流上的两条则由 `char_and_str.h` 里的全特化
     *       `io_traits<char, signed char>` / `io_traits<char, unsigned char>` 承接。
     * @note 把这两条从约束里去掉，`wis >> sc` 会退回去按数值解析——一个标准里根本不存在的
     *       操作，且不会有任何诊断。
     * @endif
     *
     * @lang{EN}
     * @brief Parses an arithmetic value from the stream.
     *
     * @note This member excludes `signed char` and `unsigned char` on top of the class
     *       template's list. That **asymmetry is deliberate**; do not "tidy it up" into a
     *       uniform list. Extraction takes its target by reference and so gets none of the
     *       integral promotion the insertion side enjoys: the `signed char&` in `is >> sc`
     *       binds to no `operator>>(int&)`, and the standard defines only
     *       `operator>>(basic_istream<char>&, signed char&)` and its `unsigned char&`
     *       companion, for `char` streams alone (see `[istream.extractors]`). `wis >> sc` on a
     *       wide stream is therefore ill-formed by the standard, and this library matches that
     *       by leaving no `sread` to match it; the two `char`-stream cases are taken by the
     *       explicit specializations `io_traits<char, signed char>` /
     *       `io_traits<char, unsigned char>` in `char_and_str.h`.
     * @note Drop those two entries from the constraint and `wis >> sc` silently falls back to
     *       parsing a number -- an operation the standard does not have, with no diagnostic.
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
 *       `const char8_t*`、`const char16_t*`、`const char32_t*`。它们**不该退化成打印地址**——
 *       `os << L"hi"` 打出 `0x...` 是个静默的错误答案，而流还是 `good()`。排除之后没有任何
 *       `io_traits` 匹配，`operator<<` 的约束不被满足、没有可行重载，于是编译不过——这是类模板
 *       能表达 `= delete` 的唯一方式。
 * @note 名单判的是 `remove_const_t` 而**不是** `remove_cv_t`：const 不影响一个指针算不算字符串
 *       指针，volatile 影响。标准删掉的是 `const wchar_t*`，**没有**删 `const volatile wchar_t*`，
 *       后者落到 `operator<<(const volatile void*)` 打地址。这个结论撇开标准也成立：打字符串至少
 *       要两趟——先找终止符，再逐字符搬出——而两趟之间 volatile 内存可以变，先前量到的长度不再
 *       作数，于是截断或越界；`setw` 又要求先知道宽度再输出，这两趟压不成一趟。指针值本身不带
 *       `volatile`，打它一次 volatile 读都不需要，是唯一安全的答案。标量只读一次，不受此限，
 *       `volatile wchar_t` 仍按字符走。
 * @note 上一条判的是**指向类型**的 cv，第一个约束判的是**键本身**的顶层 cv，两者互不相干。
 *       `is_pointer_v` 对顶层 cv 不敏感，而 `char_and_str.h` 里的字符串特化敏感，于是
 *       `char* volatile` 会被本特化接住打地址，而标准打的是内容——顶层 volatile 在按值传递时
 *       本就被丢掉，`os << cpv` 走的还是 `operator<<(const char*)`。加上这一条之后本特化拒绝，
 *       `operator<<` 改用 `decay_t` 重试、重跑一遍偏序，得到与无限定键完全一致的答案。
 *       只有 `volatile` 会这样漏进来：`operator<<` 的形参是 `const TValue&`，顶层 `const`
 *       到不了键上。
 * @note `swrite` 把实参显式转成 `const void*` 再交给 `numeric::put`，两步缺一不可：`static_cast`
 *       只能**添加** cv 限定，去掉 `volatile` 只有 `const_cast` 做得到。这样 `volatile int*` 之类
 *       才打得出地址，而不是因为到不了 `put(const void*)` 而经布尔转换打出 `1`。与 C++23 P1147R1
 *       给 `basic_ostream` 添加的**非删除**重载 `operator<<(const volatile void*)` 一致。丢弃
 *       `volatile` 在这里是安全的：地址只被格式化，`numeric::put` 从不解引用它。
 * @note 指向函数的指针同样被排除，理由是同一条，但机理与上面四条不同。`swrite` 的实参要先过
 *       `static_cast<const volatile void*>`，而函数指针与对象指针之间没有这个转换——去掉这条排除项，
 *       `os << setw`（漏写实参列表）就变成**函数体里的硬错误**，而不是 SFINAE 看得见的失败，
 *       `operator<<` 的约束也就挡不住它。排除之后没有 `io_traits` 匹配，诊断退回到"没有可行重载"，
 *       与上面四条一致。用户自写的操纵符走
 *       `operator<<(T&, void (*)(ios_base<TChar>&))` 那个专用重载，本来就不经过本特化。
 * @note 指向类型与流的 `char_type` 一致时（`wos << L"hi"`），`io_traits<TChar, const TChar*>`
 *       更特化，本特化本来就轮不到；这里的排除项只影响不一致的那些组合。
 * @note 名单里**没有** `char` / `signed char` / `unsigned char`，这不是遗漏。这三者的字符串
 *       重载在 `char_and_str.h` 里以更特化的偏特化或全特化给出，按偏序规则一定压过本特化，
 *       因此无需在此重复排除。留在本特化约束内的只有它们**该**走地址路径的组合，
 *       例如宽流上的 `wos << (const unsigned char*)s`——标准同样没有这个重载，会经隐式转换
 *       落到 `operator<<(const void*)` 打印地址。
 * @endif
 *
 * @lang{EN}
 * @brief Address-format reading and writing of pointers.
 *
 * @note The four excluded pointees correspond to the string overloads the standard
 *       `= delete`s: `const wchar_t*` (on a narrow stream), `const char8_t*`,
 *       `const char16_t*` and `const char32_t*`. They must **not** degrade into printing an
 *       address -- `os << L"hi"` yielding `0x...` is a silently wrong answer while the stream
 *       stays `good()`. With them excluded no `io_traits` matches at all, `operator<<`'s
 *       constraint is not satisfied and no overload is viable, so it does not compile -- which is
 *       the only way a class template can express `= delete`.
 * @note The list keys on `remove_const_t` and **not** `remove_cv_t`: `const` does not change
 *       whether a pointer counts as a string pointer, `volatile` does. The standard deletes
 *       `const wchar_t*` and does **not** delete `const volatile wchar_t*`, which lands on
 *       `operator<<(const volatile void*)` and prints an address. That conclusion holds
 *       independently of the standard: printing a string takes at least two passes -- find the
 *       terminator, then copy the characters out -- and volatile memory may change in between, so
 *       the length just measured no longer holds and the write truncates or runs off the end;
 *       `setw` needs the width before anything is emitted, so the two passes cannot be folded into
 *       one either. The pointer value itself is not `volatile`, and printing it needs no volatile
 *       read at all, which makes it the only safe answer. A scalar is a single read and is
 *       unaffected, so `volatile wchar_t` still goes down the character path.
 * @note The note above is about cv on the **pointee**; the first constraint is about top-level cv on
 *       the **key itself**, and the two are unrelated. `is_pointer_v` ignores top-level cv while the
 *       string specializations in `char_and_str.h` do not, so `char* volatile` would be caught here
 *       and printed as an address where the standard prints the contents -- top-level volatile is
 *       dropped when passing by value anyway, and `os << cpv` still resolves to
 *       `operator<<(const char*)`. With the constraint in place this specialization declines,
 *       `operator<<` retries with `decay_t`, the partial ordering runs again and the answer matches
 *       the unqualified key exactly. Only `volatile` can leak in this way: `operator<<` takes
 *       `const TValue&`, so a top-level `const` never reaches the key.
 * @note `swrite` casts its argument to `const void*` explicitly before handing it to
 *       `numeric::put`, and both steps are needed: `static_cast` can only **add** cv-qualifiers,
 *       and stripping the `volatile` takes a `const_cast`. That is what makes `volatile int*` and
 *       friends print an address, instead of failing to reach `put(const void*)` and printing `1`
 *       through a boolean conversion. It matches the **non-deleted**
 *       `operator<<(const volatile void*)` that C++23 P1147R1 added to `basic_ostream`. Casting
 *       the `volatile` away is safe here: the address is only formatted, never dereferenced by
 *       `numeric::put`.
 * @note Pointers to functions are excluded for the same reason, but by a different mechanism than
 *       the four above. `swrite`'s argument first goes through `static_cast<const volatile void*>`,
 *       and no such conversion exists between function pointers and object pointers -- drop this
 *       exclusion and `os << setw` with the argument list left off becomes a **hard error inside
 *       the function body** rather than a SFINAE-visible failure, which `operator<<`'s constraint
 *       cannot keep out. With the exclusion in place no `io_traits` matches and the diagnosis falls
 *       back to "no viable overload", the same as the four above. A user-written manipulator goes
 *       through the dedicated `operator<<(T&, void (*)(ios_base<TChar>&))` overload and never
 *       reaches this specialization.
 * @note When the pointee matches the stream's `char_type` (`wos << L"hi"`),
 *       `io_traits<TChar, const TChar*>` is more specialized and this specialization was never
 *       in the running; the exclusions only affect the mismatched combinations.
 * @note `char` / `signed char` / `unsigned char` are **absent** from the list, and that is
 *       not an oversight. Their string overloads live in `char_and_str.h` as more specialized
 *       partial or explicit specializations, which outrank this one by the partial ordering
 *       rules, so repeating the exclusion here would be redundant. What stays within this
 *       specialization's constraint is exactly the combinations that *should* take the address
 *       path, e.g. `wos << (const unsigned char*)s` on a wide stream -- the standard has no
 *       such overload either, and falls through the implicit conversion to
 *       `operator<<(const void*)`.
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
        auto mp = loc.template get<numeric<TChar>>();
        if (!mp)
            throw stream_error("cannot get numeric facet");

        return mp->put(s, io, const_cast<const void*>(static_cast<const volatile void*>(value)));
    }

    /**
     * @lang{ZH}
     * @brief 从流中解析一个地址，写入 `void*`。
     * @note 与 `swrite` 不同，`sread` **只接受 `void*`**，不接受任意指针类型。插入侧放宽是安全
     *       的，也与标准一致——`std::ostream` 的 `operator<<(const void*)` 本来就会通过隐式转换
     *       接住任意对象指针。但提取侧没有这样的转换：`std::istream` 只有 `operator>>(void*&)`，
     *       写 `is >> ip`（`ip` 为 `int*`）在标准里是非良构的。
     * @note 成员级的这处收窄是**有意**的，不要把它去掉。放宽会带来两个后果：一是把文本里的
     *       地址 `reinterpret_cast` 进任意类型的指针，产生野指针而流仍为 `good`；二是会静默
     *       接管 `char*`，使字符缓冲区的提取变成地址解析。收窄之后，`is >> charptr` /
     *       `is >> intptr` 没有 `sread` 可匹配，`operator>>` 没有可行重载，编译不过。
     * @note 收窄由**两处**共同表达：`requires` 里的 `is_same_v<TValue, void*>` 钉住**键**，形参
     *       `void*&` 钉住**目标**。只走 `operator>>` 的话任一处都够——键是从目标类型推出来的，
     *       两者不可能不一致；但显式限定调用能把它们拆开（`io_traits<char, int*>::sread(...)`
     *       配一个 `void*` 目标），那时只有两处齐备才挡得住。所以删掉任何一处都不会改变
     *       `operator>>` 的行为，别据此以为它是冗余的。
     * @endif
     *
     * @lang{EN}
     * @brief Parses an address from the stream into a `void*`.
     * @note Unlike `swrite`, `sread` accepts **`void*` only**, not an arbitrary pointer type.
     *       Being permissive on the insertion side is safe and matches the standard -- `std::ostream`'s
     *       `operator<<(const void*)` already accepts any object pointer through an implicit
     *       conversion. The extraction side has no such conversion: `std::istream` only provides
     *       `operator>>(void*&)`, and `is >> ip` (with `ip` an `int*`) is ill-formed by the standard.
     * @note This member-level narrowing is **deliberate**; do not drop it. Widening has
     *       two consequences: it `reinterpret_cast`s a textual address into a pointer of arbitrary
     *       type, yielding a wild pointer while the stream stays `good`; and it silently captures
     *       `char*`, turning character buffer extraction into address parsing. As narrowed,
     *       `is >> charptr` / `is >> intptr` have no `sread` to match, so `operator>>` has no
     *       viable overload and they do not compile.
     * @note The narrowing is expressed in **two** places: `is_same_v<TValue, void*>` in the
     *       requires-clause pins the **key**, and the `void*&` parameter pins the **target**. Either
     *       one alone suffices for `operator>>`, where the key is deduced from the target and the two
     *       cannot disagree; an explicitly qualified call can pull them apart, though
     *       (`io_traits<char, int*>::sread(...)` handed a `void*` target), and only both together
     *       keep that out. So dropping either one leaves `operator>>` unchanged -- do not read that
     *       as evidence it is redundant.
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
