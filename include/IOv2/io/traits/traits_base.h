// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file traits_base.h
 * @lang{ZH}
 * 声明本库唯一的 I/O 扩展点 `io_traits<TChar, T>`，以及提取端的可选中转类型
 * `parse_context_type<TChar, T>`。
 *
 * 本文件不依赖任何其它头文件，只有声明、没有定义：`io_traits` 的主模板是**故意不定义**的，
 * 目的是让"某个类型不支持某个方向"这件事表达为「特化不存在」。类模板没有 `= delete`，这是
 * 唯一能把标准里那些被删除的重载（如向 `char` 流插入 `wchar_t`）如实表达出来的手段。
 *
 * ### 扩展点
 *
 * 给自己的类型 `T` 接上 `os << t` / `is >> t`，只需在命名空间 `IOv2` 里特化
 * `io_traits<TChar, T>`，并按需要提供下列成员。名字里的 `s` 是 **static** 的意思，与 sentry
 * 无关；`write` / `read` 这两个名字留给将来可能出现的非静态成员。
 *
 * **迭代器形式**（格式化 I/O 用这一档；加锁与哨兵由运算符负责，成员本身只管把字符搬进/搬出
 * 迭代器）：
 * ```cpp
 * template <typename TChar>
 * struct IOv2::io_traits<TChar, MyType>
 * {
 *     template <typename TIter>
 *         requires (char_sink_for<TIter, TChar>)
 *     static TIter swrite(TIter it, ios_base<TChar>& io, const locale<TChar>& loc,
 *                         const MyType& v);
 *
 *     template <typename TIter, std::sentinel_for<TIter> TSent>
 *         requires (std::is_same_v<TChar, typename TIter::value_type>)
 *     static TIter sread(TIter it, TSent end, ios_base<TChar>& io, const locale<TChar>& loc,
 *                        MyType& v);
 * };
 * ```
 *
 * **流形式**（操纵符用这一档；成员直接拿到流本身，**不加锁、不建哨兵**，需要就自己来——
 * `io_traits<TChar, ws_t>::sread` 就是自己加锁、自己构造哨兵、并自己 `catch` 的）：
 * ```cpp
 * template <ostream_type T>
 *     requires std::same_as<typename T::char_type, TChar>
 * static void swrite(T& s, const MyType& v);
 *
 * template <istream_type T>
 *     requires std::same_as<typename T::char_type, TChar>
 * static void sread (T& s, const MyType& v);
 * ```
 *
 * 那条 `requires` 不能省：流类型是模板形参，不写它就与键的 `TChar` 无关，显式限定调用便能把宽键
 * 配窄流。运算符走不到那里（它总是用 `T::char_type` 实例化），但手写调用够得着，于是本该编译期
 * 报错的事落到运行期（`strfailbit`）。迭代器形式靠那条迭代器约束达到同一效果：插入侧是本文件
 * 里的 `char_sink_for<TIter, TChar>`，提取侧是 `std::is_same_v<TChar, typename TIter::value_type>`。
 * 两侧不对称是有意的——输出迭代器连 `value_type` 这个 typedef 都不要求存在（C++20 起标准的
 * 输出适配器一律是 `void`），所以插入侧只能查可写性，能查到的字符类型仍照查；而提取侧实际
 * 传进来的只有 `istreambuf_iterator` 一族，成员 `value_type` 查得到，直接查它最省事。注意
 * 「查得到」不是标准给的保证：`std::input_iterator` 要求的是 `iter_value_t` 可求值，而不是
 * 有嵌套的 `value_type`，**裸指针**就没有（`char*` 满足 `std::input_iterator`，
 * `iter_value_t<char*>` 是 `char`，却没有 `char*::value_type`）。于是提取侧这条约束顺带把
 * 裸指针排除在外，插入侧的 `char_sink_for` 则收；这个差别落不到实处，因为 `operator>>` 永远
 * 只传 `istreambuf_iterator`。
 *
 * 两种形式靠**参数个数**区分，一个特化**只能提供其中一种**：两种都提供是编译错误，运算符会就地
 * `static_assert`。插入端还会把 `TValue` 衰退一次再试一遍（这样数组名能衰退成指针、函数名能衰退
 * 成函数指针），同一种形式内不衰退的 `TValue` 优先。
 *
 * 库内置的操纵符没有 `operator()`：单向的（`ws`、`endl`、`ends`、`flush`）是空标记类型，双向的
 * 只携带参数，逻辑一律写在本扩展点里——一行写得下的就两个成员各写一遍，写不下的走一个私有静态
 * 辅助函数。入口于是只剩 `os << m` / `is >> m` 一条，抛出的异常最终都由运算符接住转交
 * `handle_exception`，绝大多数操纵符因此不必自己兜。代价是标准的 `std::ws(is)` /
 * `std::endl(os)` 直接调用形式在本库不存在。
 *
 * @warning **自己加了锁的流形式成员，必须自己 `catch`：运算符那层的 `catch` 罩不住你的锁。**
 *          运算符对流形式不加锁，它的 `try` / `catch` 在你的 `lock_guard` 之外；异常一旦逃出去，
 *          栈展开会先析构你那个局部的锁守卫，`handle_exception` 的置位就落到**解锁之后**，失败
 *          路径与成功路径对同一把 `io_mutex()` 的可见性时序于是对不上。因此凡是取了
 *          `io_mutex()` 的流形式成员——库内是 `io_traits<TChar, ws_t>::sread` 与
 *          `io_traits<TChar, endl_t>::swrite`——都在锁内自己 `catch` 并调 `handle_exception`。
 *          掩码命中时异常仍会逃到运算符那层再处理一遍，这是无害的：`handle_exception` 是幂等的
 *          （见 `io_base` 上的说明）。反过来，压根不加锁的流形式成员（`setw`、`setfill` 那些）
 *          两条路径同样不加锁，一致，交给运算符即可。
 *
 * 唯一不经过本扩展点的是只取 `ios_base<TChar>&` 的**函数指针**操纵符：插入端与提取端各有一条
 * 专门的运算符重载，其形参类型必须是非推导语境——`os << IOv2::boolalpha` 里的操纵符是函数
 * 模板，形参不先定下来就推不出模板实参、取不到函数地址。只支持函数指针这一种形状；带状态的
 * 操纵符走本扩展点的流形式即可，它拿到的是真正的流，比 `ios_base&` 能做的更多。
 *
 * ### 方向
 *
 * 方向由**哪个成员存在**决定：只有 `swrite` 即只能插入，只有 `sread` 即只能提取，两个都有
 * 即两个方向都行。这一点不能改用**流类型**的约束来表达——`iostream` 同时满足 `istream_type`
 * 与 `ostream_type`，光靠那个挡不住反向用法；真正判定方向的是运算符的约束
 * `detail::insertable` / `detail::extractable`，它们探的就是对应的成员在不在。用反了因此不是
 * 报一句定制信息，而是这条运算符根本不参与重载决议，得到编译器通用的
 * "no match for `operator<<`"。
 *
 * 反过来说，这也让"能不能流式化"变成可以**探测**的：`requires { os << x; }` /
 * `requires { is >> x; }` 现在如实反映结果，泛型代码（日志、序列化、调试打印）可以直接用它分支，
 * 不必知道底下走的是哪一种形式。定制诊断与可探测性二者不可兼得——`static_assert` 要可达就得让
 * 运算符不加约束，而不加约束就无法探测；本库选了后者。
 *
 * ### 错误
 *
 * 成员**直接抛异常**即可，不要自己去动流的状态位：运算符会接住并交给
 * `handle_exception`，转成相应的状态位、并遵守流的异常掩码。本库自己抛的一律是
 * `stream_error`。
 *
 * ### 解析上下文
 *
 * 提取端还有一个可选的中转：若 `parse_context_type<TChar, T>::type` 不是 `T` 本身，运算符会
 * 先构造一个该类型的临时量、让 `io_traits<TChar, 上下文类型>::sread` 解析它，再
 * `static_cast` 回 `T`。若上下文类型还提供了静态成员
 * `make_parse_context(const T&)`，则临时量由它构造——这就是 `std::tm` 用旧值作为未解析字段
 * 回退值的做法，见 `IOv2/io/traits/tm.h`。主模板是恒等映射，不需要这一层就不用管它。
 * @endif
 *
 * @lang{EN}
 * Declares this library's single I/O extension point, `io_traits<TChar, T>`, together with the
 * optional relay type used on the extraction side, `parse_context_type<TChar, T>`.
 *
 * This file depends on nothing else and contains declarations only: the primary `io_traits`
 * template is **deliberately left undefined** so that "this type does not support this
 * direction" can be expressed as "the specialization does not exist". A class template has no
 * `= delete`, and this is the only way to faithfully express the overloads the standard deletes
 * (inserting a `wchar_t` into a `char` stream, for instance).
 *
 * ### The extension point
 *
 * To make `os << t` / `is >> t` work for your own type `T`, specialize
 * `io_traits<TChar, T>` in namespace `IOv2` and provide whichever members you need. The `s` in
 * the names means **static** and has nothing to do with the sentry; the names `write` and `read`
 * are reserved for possible non-static members later.
 *
 * **Iterator form** (used by formatted I/O; the operator owns the lock and the sentry, and the
 * member only moves characters through the iterator):
 * ```cpp
 * template <typename TChar>
 * struct IOv2::io_traits<TChar, MyType>
 * {
 *     template <typename TIter>
 *         requires (char_sink_for<TIter, TChar>)
 *     static TIter swrite(TIter it, ios_base<TChar>& io, const locale<TChar>& loc,
 *                         const MyType& v);
 *
 *     template <typename TIter, std::sentinel_for<TIter> TSent>
 *         requires (std::is_same_v<TChar, typename TIter::value_type>)
 *     static TIter sread(TIter it, TSent end, ios_base<TChar>& io, const locale<TChar>& loc,
 *                        MyType& v);
 * };
 * ```
 *
 * **Stream form** (used by manipulators; the member gets the stream itself and there is **no
 * lock and no sentry** -- do it yourself if you need one, as `io_traits<TChar, ws_t>::sread` does,
 * which takes the lock, builds the sentry and catches, all itself):
 * ```cpp
 * template <ostream_type T>
 *     requires std::same_as<typename T::char_type, TChar>
 * static void swrite(T& s, const MyType& v);
 *
 * template <istream_type T>
 *     requires std::same_as<typename T::char_type, TChar>
 * static void sread (T& s, const MyType& v);
 * ```
 *
 * That `requires` is not optional: the stream is a template parameter, so without it nothing ties
 * it to the key's `TChar` and an explicitly qualified call can pair a wide key with a narrow
 * stream. The operators never get there -- they always instantiate with `T::char_type` -- but a
 * hand-written call does, turning what should be a compile error into a run-time one
 * (`strfailbit`). The iterator form achieves the same through its constraint on the iterator:
 * `char_sink_for<TIter, TChar>` from this file on the insertion side, and
 * `std::is_same_v<TChar, typename TIter::value_type>` on the extraction side. The asymmetry is
 * deliberate -- an output iterator is not required to have a `value_type` typedef at all (since
 * C++20 the standard output adaptors uniformly use `void`), so the insertion side can only test
 * writability, while still checking any character type it does find; the extraction side, whose
 * only real argument is an `istreambuf_iterator`, does have the member and simply tests it. That
 * the member is there is not something the standard guarantees, though: `std::input_iterator`
 * requires `iter_value_t` to be well-formed, not a nested `value_type`, and a **raw pointer** has
 * none (`char*` satisfies `std::input_iterator` and `iter_value_t<char*>` is `char`, yet there is
 * no `char*::value_type`). The extraction-side constraint therefore also excludes raw pointers
 * where `char_sink_for` accepts them -- a difference with no practical reach, since `operator>>`
 * only ever passes an `istreambuf_iterator`.
 *
 * The two forms are told apart by **arity**, and a specialization may provide **only one of
 * them**: providing both is a compile error, diagnosed by a `static_assert` in the operator. The
 * insertion side also retries with `TValue` decayed once, which is what lets an array name decay
 * to a pointer and a function name to a function pointer; within one form the undecayed `TValue`
 * wins.
 *
 * The library's own manipulators have no `operator()`: the one-way ones (`ws`, `endl`, `ends`,
 * `flush`) are empty tag types, the two-way ones carry their parameters only, and the logic always
 * lives in this extension point -- spelled out in both members when it fits on one line, factored
 * into a private static helper when it does not. That leaves `os << m` / `is >> m` as the only
 * entry, so every exception ends up caught by the operator and handed to `handle_exception`, and
 * most manipulators need no `catch` of their own. The price is that the standard's direct-call
 * forms, `std::ws(is)` and `std::endl(os)`, do not exist here.
 *
 * @warning **A stream-form member that takes a lock must catch for itself: the operator's `catch`
 *          cannot cover your lock.** The operator never locks for the stream form, so its
 *          `try` / `catch` sits outside your `lock_guard`; once an exception escapes, unwinding
 *          destroys that local guard first and `handle_exception` sets the state bits **after the
 *          unlock**, leaving the failure path inconsistent with the success path with respect to
 *          the same `io_mutex()`. Every stream-form member that takes `io_mutex()` -- in this
 *          library, `io_traits<TChar, ws_t>::sread` and `io_traits<TChar, endl_t>::swrite` --
 *          therefore catches under its own lock and calls `handle_exception` there. On a masked
 *          rethrow the exception still reaches the operator and is handled once more, which is
 *          harmless: `handle_exception` is idempotent (see its description on `io_base`).
 *          Conversely, a stream-form member that takes no lock at all (`setw`, `setfill` and the
 *          rest) has both paths equally unlocked, is therefore consistent, and can leave the
 *          exception to the operator.
 *
 * The one thing that does not go through this extension point is a **function-pointer**
 * manipulator taking only `ios_base<TChar>&`: each of the insertion and extraction sides carries
 * one dedicated operator overload for it, whose parameter type must be a non-deduced context --
 * the manipulator in `os << IOv2::boolalpha` is a function template, and without a parameter type
 * fixed in advance its template arguments cannot be deduced nor its address taken. A function
 * pointer is the only shape supported; a manipulator that carries state belongs in the stream form
 * of this extension point, which gets the real stream and can do more than `ios_base&` allows.
 *
 * ### Direction
 *
 * The direction is decided by **which member exists**: `swrite` only means insertion only,
 * `sread` only means extraction only, and both means both. A constraint on the **stream type**
 * cannot express this -- an `iostream` satisfies `istream_type` and `ostream_type` alike, so that
 * alone cannot stop a backwards use. What actually decides the direction are the operators' own
 * constraints, `detail::insertable` and `detail::extractable`, which probe for exactly those
 * members. Using one backwards therefore does not produce a tailored message: the operator simply
 * drops out of overload resolution and the compiler reports its generic "no match for
 * `operator<<`".
 *
 * The flip side is that streamability becomes **detectable**: `requires { os << x; }` and
 * `requires { is >> x; }` now report the truth, so generic code (logging, serialization, debug
 * printing) can branch on them without having to know which of the two forms is underneath.
 * Tailored diagnostics and detectability cannot coexist -- a reachable `static_assert` requires
 * an unconstrained operator, and an unconstrained operator cannot be probed. This library picks
 * the latter.
 *
 * ### Errors
 *
 * Members should simply **throw**; do not touch the stream's state bits yourself. The operator
 * catches and hands the exception to `handle_exception`, which turns it into the matching state
 * bit and honours the stream's exception mask. Everything this library throws itself is a
 * `stream_error`.
 *
 * ### Parse contexts
 *
 * The extraction side has one optional relay: if `parse_context_type<TChar, T>::type` is not `T`
 * itself, the operator builds a temporary of that type, lets
 * `io_traits<TChar, context type>::sread` parse into it, and `static_cast`s the result back to
 * `T`. If the context type also provides a static `make_parse_context(const T&)`, that builds
 * the temporary -- which is how `std::tm` uses its previous contents as the fallbacks for the
 * fields the format string does not parse; see `IOv2/io/traits/tm.h`. The primary template is the
 * identity, so ignore this layer if you do not need it.
 * @endif
 */
#pragma once

#include <iterator>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief `TIter` 能否作为字符类型为 `TChar` 的**输出**迭代器使用——插入侧扩展点的迭代器约束。
 *
 * 本文件头部说明了这条约束为何不能省：迭代器是函数模板形参而不是类型的一部分，显式限定的
 * 手写调用能把宽键配窄汇，本概念把 `TIter` 拴回键的 `TChar`。提取侧的对应约束直接写作
 * `std::is_same_v<TChar, typename TIter::value_type>`；两侧不对称是有意的，理由见下。
 *
 * 第一个合取项照搬 `<format>`：标准的 `format_to` 一族用的正是
 * `std::output_iterator<Out, const charT&>`，查的是"能不能把一个 `TChar` 左值写进去"这一
 * 真正需要的性质。C++20 起 `std::back_insert_iterator`、`front_insert_iterator`、
 * `insert_iterator`、`ostream_iterator`、`std::ostreambuf_iterator` 的 `value_type` 一律是
 * `void`，`std::output_iterator` 概念也不要求该 typedef 存在，因此**不能**用它去查写入侧——
 * 那是可读侧的 trait。提取侧照查成员 `value_type` 则无妨，因为实际传进去的只有
 * `istreambuf_iterator` 一族。这不等于标准替输入迭代器保证了这个成员：`std::input_iterator`
 * 要求的是 `iter_value_t` 可求值，裸指针满足它却没有嵌套 `value_type`，因而会被提取侧那条
 * 约束一并挡掉（本概念反倒收裸指针）。这个差别落不到实处——`operator>>` 永远只传
 * `istreambuf_iterator`。
 *
 * 第二个合取项是本库在标准之上多加的一道守卫：迭代器**若**报得出字符类型，就必须与 `TChar`
 * 一致。它查的是 `std::iter_value_t<TIter>` 而不是成员 `TIter::value_type`，两者对本库自己的
 * 迭代器结果相同，但对**裸指针**不同——裸指针没有成员 `value_type`，其字符类型来自
 * `iterator_traits`。用 `iter_value_t` 才能拦住 `wchar_t buf[64]` 配 `TChar == char` 这种手写汇，
 * 而这正是本守卫存在的意义；`std::format_to` 在同一位置是放行的，此处比标准严。
 *
 * @note 三种情形实测（`-std=c++23`，libstdc++ 15）：`iter_value_t<wchar_t*>` 为 `wchar_t`
 *       （配窄 `TChar` 被拒）、`iter_value_t<ostreambuf_iterator<...>>` 为 `char_type`
 *       （守卫照常生效）、而对 `std::back_insert_iterator` 是 **ill-formed**（不是 `void`），
 *       由第一个析取项的 `!requires` 兜住而放行。`is_void_v` 那一项覆盖的是
 *       `iterator_traits` well-formed 且把 `value_type` 定成 `void` 的输出适配器。
 *
 * @warning 对**报不出**字符类型的汇（`iter_value_t` ill-formed 或为 `void`），无从可查，
 *          只剩可写性这一关。于是宽窄错配会经隐式转换静默通过：窄 facet 写进 `std::wstring`
 *          得到的是"把字节当字符"的伪宽串，宽 facet 写进 `std::string` 则逐码元截断。两者都
 *          不是 UB、不越界、不崩溃，只是字符损坏，且 ASCII 部分看着正常。这与标准的行为一致
 *          （`std::format_to`、`std::copy` 到 `back_inserter` 皆然），也与本库 facet 层一致
 *          （`IOv2/facet/` 下的 `put`/`get` 完全无迭代器约束）。
 *
 * @tparam TIter 待检测的输出迭代器类型
 * @tparam TChar 流的字符类型
 * @endif
 *
 * @lang{EN}
 * @brief Whether `TIter` is usable as an **output** iterator over character type `TChar` -- the
 *        iterator constraint used by insertion-side extension points.
 *
 * The top of this file explains why the constraint cannot be dropped: the iterator is a
 * function-template parameter rather than part of a type, so an explicitly qualified
 * hand-written call could pair a wide key with a narrow sink, and this concept ties `TIter` back
 * to the key's `TChar`. The extraction-side counterpart is spelled directly as
 * `std::is_same_v<TChar, typename TIter::value_type>`; the asymmetry is deliberate, for the
 * reason below.
 *
 * The first conjunct is taken straight from `<format>`: the standard `format_to` family uses
 * exactly `std::output_iterator<Out, const charT&>`, testing the property actually needed --
 * that a `TChar` lvalue can be written through the iterator. Since C++20 the `value_type` of
 * `std::back_insert_iterator`, `front_insert_iterator`, `insert_iterator`, `ostream_iterator`
 * and `std::ostreambuf_iterator` is uniformly `void`, and `std::output_iterator` does not
 * require that typedef to exist at all, so it must **not** be used to check the write side -- it
 * is a readable-side trait. The extraction side may keep testing the member `value_type` because
 * the only thing ever passed there is an `istreambuf_iterator`. That is not the same as the
 * standard guaranteeing the member for input iterators: `std::input_iterator` requires
 * `iter_value_t` to be well-formed, and a raw pointer satisfies it while having no nested
 * `value_type`, so the extraction-side constraint rejects raw pointers (which this concept, in
 * contrast, accepts). The difference has no practical reach -- `operator>>` only ever passes an
 * `istreambuf_iterator`.
 *
 * The second conjunct is one guard this library adds on top of the standard: **if** the iterator
 * can name a character type at all, it has to agree with `TChar`. It tests
 * `std::iter_value_t<TIter>` rather than the member `TIter::value_type`; the two agree for this
 * library's own iterators but differ for **raw pointers**, which have no member `value_type` and
 * get their character type from `iterator_traits`. Only `iter_value_t` catches a hand-written
 * sink such as `wchar_t buf[64]` paired with `TChar == char`, which is precisely what this guard
 * exists for; `std::format_to` accepts that pairing, so here the library is stricter than the
 * standard.
 *
 * @note All three cases measured (`-std=c++23`, libstdc++ 15): `iter_value_t<wchar_t*>` is
 *       `wchar_t` (rejected against a narrow `TChar`), `iter_value_t<ostreambuf_iterator<...>>`
 *       is `char_type` (guard applies as usual), and for `std::back_insert_iterator` it is
 *       **ill-formed** (not `void`), which the leading `!requires` disjunct absorbs so the
 *       iterator is accepted. The `is_void_v` disjunct covers output adaptors whose
 *       `iterator_traits` is well-formed and names `value_type` as `void`.
 *
 * @warning For a sink that **cannot** name a character type (`iter_value_t` ill-formed or
 *          `void`) there is nothing to check, and only writability remains. A width mismatch
 *          then passes silently through an implicit conversion: a narrow facet writing into a
 *          `std::wstring` yields a pseudo-wide string of bytes-as-characters, while a wide facet
 *          writing into a `std::string` truncates code unit by code unit. Neither is UB, out of
 *          bounds, or a crash -- just character corruption, and the ASCII part still looks
 *          correct. This matches the standard (`std::format_to` and `std::copy` into a
 *          `back_inserter` behave the same) and matches this library's own facet layer, where
 *          the `put`/`get` templates under `IOv2/facet/` are wholly unconstrained.
 *
 * @tparam TIter The output iterator type under inspection
 * @tparam TChar The stream's character type
 * @endif
 */
template <typename TIter, typename TChar>
concept char_sink_for =
    std::output_iterator<TIter, const TChar&>
    && (!requires { typename std::iter_value_t<TIter>; }
        || std::is_void_v<std::iter_value_t<TIter>>
        || std::is_same_v<TChar, std::iter_value_t<TIter>>);

/**
 * @lang{ZH}
 * @brief 把 `TChar` 流的读写逻辑接到类型 `T` 上的扩展点。主模板故意不定义；见本文件头部。
 * @endif
 *
 * @lang{EN}
 * @brief The extension point that attaches read/write logic for a `TChar` stream to a type `T`.
 *        The primary template is deliberately undefined; see the top of this file.
 * @endif
 */
template <typename TChar, typename T>
struct io_traits;

/**
 * @lang{ZH}
 * @brief 提取 `T` 时实际解析进的中转类型；主模板为恒等映射。见本文件头部"解析上下文"。
 * @endif
 *
 * @lang{EN}
 * @brief The relay type actually parsed into when extracting a `T`; the primary template is the
 *        identity. See "Parse contexts" at the top of this file.
 * @endif
 */
template <typename TChar, typename T>
struct parse_context_type
{
    using type = T;
};

}
