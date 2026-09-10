// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file traits_base.h
 * @lang{ZH}
 * 声明本库唯一的 I/O 扩展点 `io_traits<TChar, T>`，以及提取端的可选中转类型
 * `parse_context_type<TChar, T>`。
 *
 * `io_traits` 的主模板是**故意不定义**的，目的是让"某个类型不支持某个方向"这件事表达为
 * 「特化不存在」。类模板没有 `= delete`，这是唯一能把标准里那些被删除的重载
 * （如向 `char` 流插入 `wchar_t`）如实表达出来的手段。
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
 * 配窄流，本该编译期报错的事于是落到运行期（`strfailbit`）。迭代器形式靠那条迭代器约束达到同一
 * 效果：插入侧是本文件里的 `char_sink_for<TIter, TChar>`，提取侧是
 * `std::is_same_v<TChar, typename TIter::value_type>`。两侧不对称是有意的——输出迭代器连
 * `value_type` 这个 typedef 都不要求存在，所以插入侧只能查可写性；而提取侧实际传进来的只有
 * `istreambuf_iterator` 一族，成员 `value_type` 查得到，直接查它最省事。
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
 * `requires { is >> x; }` 如实反映当前 TU 的结果，不必知道底下走的是哪一种形式。定制诊断与
 * 可探测性二者不可兼得——`static_assert` 要可达就得让运算符不加约束，而不加约束就无法探测；
 * 本库选了后者。
 * @warning 该判据**只对当前 TU 有效**：答案取决于本 TU 包含了 `traits/` 下的哪些头。在会被多个 TU
 *          发射的实体（函数模板、`inline` 函数）里拿它分支是 ODR 违反，不要求诊断，结果随优化
 *          级别与链接顺序变。分支要么关在单个 TU 内（`static`、匿名 namespace），要么保证相关
 *          TU 包含同一套 traits 头。自定义特化请与类型定义放在同一个头里，随类型进入每个 TU。
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
 * 先构造一个该类型的临时量、让 `io_traits<TChar, 上下文类型>::sread` 解析它，再调用上下文的
 * `convert_to(T&)` 写回目标。临时量由 `parse_context_type<TChar, T>` 的
 * `make_parse_context` 构造——这就是 `std::tm` 用旧值作为未解析字段回退值的做法，见
 * `IOv2/io/traits/tm.h`；没有该成员时默认构造。主模板是恒等映射，不需要这一层就不用管它。
 *
 * 该成员必须**恰好**声明成 `static type make_parse_context(const T&)`，且是**单一的、非模板、
 * 非重载、公开、未删除**的成员：它只读取目标作种子，不得改动它，故按 `const` 引用收；`type` 按值
 * 返回，不接受可转换到它的中间类型。可以带 `noexcept`，也可以是从基类继承来的。凡是**声明了**
 * 这个名字却不合上述形状的，一律是 `static_assert`，不会静默退回默认构造——因此
 * `requires { is >> x; }` 对这种写坏的特化答**真**，真正用它时才撞上断言；这是有意的，
 * 目的是让写坏的特化响亮地失败，而不是被泛型代码静默绕开。
 * @warning 唯一的例外是把特化写成 `final`：那样本库探测不到形状不符的成员，`make_parse_context`
 *          会被当作不存在而默认构造上下文，种子被静默丢弃。特化不要写 `final`。
 * @endif
 *
 * @lang{EN}
 * Declares this library's single I/O extension point, `io_traits<TChar, T>`, together with the
 * optional relay type used on the extraction side, `parse_context_type<TChar, T>`.
 *
 * The primary `io_traits` template is **deliberately left undefined** so that "this type does
 * not support this direction" can be expressed as "the specialization does not exist". A class
 * template has no `= delete`, and this is the only way to faithfully express the overloads the
 * standard deletes (inserting a `wchar_t` into a `char` stream, for instance).
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
 * stream, turning what should be a compile error into a run-time one (`strfailbit`). The
 * iterator form achieves the same through its constraint on the iterator:
 * `char_sink_for<TIter, TChar>` from this file on the insertion side, and
 * `std::is_same_v<TChar, typename TIter::value_type>` on the extraction side. The asymmetry is
 * deliberate: an output iterator is not required to have a `value_type` typedef at all, so the
 * insertion side can only test writability, while the extraction side, whose only real argument
 * is an `istreambuf_iterator`, does have the member and simply tests it.
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
 * `requires { is >> x; }` report the truth for the current TU, without your having to know which of
 * the two forms is underneath. Tailored diagnostics and detectability cannot coexist -- a reachable
 * `static_assert` requires an unconstrained operator, and an unconstrained operator cannot be
 * probed. This library picks the latter.
 * @warning The test holds **for the current TU only**: the answer depends on which headers under
 *          `traits/` this TU included. Branching on it inside an entity emitted by several TUs (a
 *          function template, an `inline` function) is an ODR violation, no diagnostic required,
 *          and the outcome shifts with optimization level and link order. Keep branches inside a
 *          single TU (`static`, an unnamed namespace), or have every TU involved include the same
 *          traits headers. Put your own specializations in the header that defines the type, so
 *          they travel with it into every TU.
 *
 * ### Errors
 *
 * Members should simply **throw**; do not touch the stream's state bits yourself. The operator
 * catches and hands the exception to `handle_exception`, which turns it into the matching state
 * bit and honors the stream's exception mask. Everything this library throws itself is a
 * `stream_error`.
 *
 * ### Parse contexts
 *
 * The extraction side has one optional relay: if `parse_context_type<TChar, T>::type` is not `T`
 * itself, the operator builds a temporary of that type, lets
 * `io_traits<TChar, context type>::sread` parse into it, and calls the context's
 * `convert_to(T&)` to write the result back. The temporary comes from
 * `parse_context_type<TChar, T>`'s `make_parse_context` -- this is how `std::tm` uses its previous
 * contents as the fallbacks for the fields the format string does not parse; see
 * `IOv2/io/traits/tm.h`. It is default constructed when there is no such member. The primary
 * template is the identity, so ignore this layer if you do not need it.
 *
 * That member must be declared **exactly** `static type make_parse_context(const T&)`, as a
 * **single, non-template, non-overloaded, public, non-deleted** member: it only reads the target as
 * a seed and must not modify it, hence the `const` reference; `type` is returned by value, and an
 * intermediate type convertible to it is not accepted. It may be `noexcept`, and it may be
 * inherited from a base. Anything that **declares** that name without matching the shape is a
 * `static_assert`, never a silent fallback to default construction -- so `requires { is >> x; }`
 * answers **true** for such a mis-written specialization and the assertion fires only when it is
 * actually used. That is deliberate: a mis-written specialization should fail loudly rather than be
 * silently routed around by generic code.
 * @warning The one exception is writing the specialization `final`: a member of the wrong shape is
 *          then undetectable, `make_parse_context` is taken to be absent, the context is default
 *          constructed and the seed is silently dropped. Do not mark the specialization `final`.
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
 * 手写调用能把宽键配窄汇，本概念把 `TIter` 拴回键的 `TChar`。
 *
 * 第一个合取项照搬 `<format>`：标准的 `format_to` 一族用的正是
 * `std::output_iterator<Out, const charT&>`，查的是"能不能把一个 `TChar` 左值写进去"这一
 * 真正需要的性质。它有意不查成员 `value_type`——C++20 起标准的输出适配器一律把它定成 `void`，
 * 概念本身也不要求它存在。
 *
 * 第二个合取项是本库在标准之上多加的一道守卫：迭代器**若**报得出字符类型，就必须与 `TChar`
 * 一致。它查的是 `std::iter_value_t<TIter>` 而不是成员 `TIter::value_type`，这样才拦得住
 * `wchar_t buf[64]` 配 `TChar == char` 这种手写汇；`std::format_to` 在同一位置是放行的，
 * 此处比标准严。
 *
 * @note 三个析取项里的前两个都需要，但它们各自覆盖的形状与直觉相反。标准的输出适配器
 *       （`back_insert_iterator` / `front_insert_iterator` / `insert_iterator` /
 *       `ostream_iterator` / `ostreambuf_iterator`）虽然都声明了**成员** `value_type = void`，
 *       它们的 `std::iter_value_t` 却是 **ill-formed** 而不是 `void`，因此**全部**由
 *       `!requires` 那一项兜住——这也正是本合取项查 `iter_value_t` 而不查成员的用处所在。
 *       `is_void_v` 那一项**够不到任何标准适配器**，它覆盖的是显式特化了
 *       `std::iterator_traits<I>` 并把其 `value_type` 定成 `void` 的类型；这种形状实测可达，
 *       故该析取项不是死代码。
 * @warning 对**报不出**字符类型的汇（`iter_value_t` ill-formed 或为 `void`），无从可查，
 *          只剩可写性这一关。于是宽窄错配会经隐式转换静默通过：窄 facet 写进 `std::wstring`
 *          得到的是"把字节当字符"的伪宽串，宽 facet 写进 `std::string` 则逐码元截断。两者都
 *          不是 UB、不越界、不崩溃，只是字符损坏，且 ASCII 部分看着正常。这与标准的行为一致
 *          （`std::format_to`、`std::copy` 到 `back_inserter` 皆然）。
 * @warning 本概念**不检查容量**，插入侧整条链路也不带哨位：写出多少由 `width()` 决定，而不由汇
 *          决定。因此把固定容量的汇（例如 `char buf[8]`）配上大于它的 `width()`，写出会越过
 *          缓冲区末尾——**容量由调用方保证**。这与标准的输出迭代器约定一致
 *          （`std::format_to`、`std::num_put::put` 同样越界；`<format>` 另给 `format_to_n`
 *          作为有界形式）。与提取侧拒绝裸指针（见 `io_traits<TChar, TChar[N]>` 的说明）**不矛盾**：
 *          那里的长度来自输入数据，可被攻击者左右；这里的长度来自调用方自己设的流状态。
 *
 * @tparam TIter 待检测的输出迭代器类型
 * @tparam TChar 流的字符类型
 * @endif
 *
 * @lang{EN}
 * @brief Whether `TIter` is usable as an **output** iterator over character type `TChar` -- the
 *        iterator constraint used by insertion-side extension points.
 *
 * Why this constraint cannot be omitted is explained at the top of this file: the iterator is a
 * function-template parameter rather than part of the type, so an explicitly qualified
 * hand-written call could pair a wide key with a narrow sink; this concept ties `TIter` back to
 * the key's `TChar`.
 *
 * The first conjunct is taken straight from `<format>`: the standard `format_to` family uses
 * exactly `std::output_iterator<Out, const charT&>`, testing the property actually needed --
 * that a `TChar` lvalue can be written through the iterator. It deliberately does not test the
 * member `value_type`, which since C++20 is uniformly `void` on the standard output adaptors and
 * is not required to exist at all.
 *
 * The second conjunct is one guard this library adds on top of the standard: **if** the iterator
 * can name a character type at all, it has to agree with `TChar`. It tests
 * `std::iter_value_t<TIter>` rather than the member `TIter::value_type`, which is what catches a
 * hand-written sink such as `wchar_t buf[64]` paired with `TChar == char`; `std::format_to`
 * accepts that pairing, so here the library is stricter than the standard.
 *
 * @note The first two of the three disjuncts are both needed, but what each one covers is the
 *       opposite of what one would guess. The standard output adaptors
 *       (`back_insert_iterator`, `front_insert_iterator`, `insert_iterator`, `ostream_iterator`,
 *       `ostreambuf_iterator`) all declare a **member** `value_type` of `void`, yet their
 *       `std::iter_value_t` is **ill-formed** rather than `void`, so the leading `!requires`
 *       absorbs **all** of them -- which is precisely what testing `iter_value_t` instead of the
 *       member buys. The `is_void_v` disjunct reaches **no standard adaptor at all**; it covers
 *       types that explicitly specialize `std::iterator_traits<I>` with a `value_type` of `void`.
 *       That shape is reachable in practice, so the disjunct is not dead code.
 * @warning For a sink that **cannot** name a character type (`iter_value_t` ill-formed or
 *          `void`) there is nothing to check, and only writability remains. A width mismatch
 *          then passes silently through an implicit conversion: a narrow facet writing into a
 *          `std::wstring` yields a pseudo-wide string of bytes-as-characters, while a wide facet
 *          writing into a `std::string` truncates code unit by code unit. Neither is UB, out of
 *          bounds, or a crash -- just character corruption, and the ASCII part still looks
 *          correct. This matches the standard (`std::format_to` and `std::copy` into a
 *          `back_inserter` behave the same).
 * @warning This concept does **not** check capacity, and nothing on the insertion side carries a
 *          sentinel: how much is written is decided by `width()`, not by the sink. Pairing a
 *          fixed-capacity sink (`char buf[8]`, say) with a larger `width()` therefore writes past
 *          the end of the buffer -- **capacity is the caller's guarantee**. This follows the
 *          standard's output-iterator convention (`std::format_to` and `std::num_put::put` overrun
 *          alike; `<format>` offers `format_to_n` as the bounded form). It does **not** contradict
 *          the extraction side's refusal of raw pointers (see `io_traits<TChar, TChar[N]>`): there
 *          the length comes from the input and an attacker can steer it, here it comes from stream
 *          state the caller set.
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
