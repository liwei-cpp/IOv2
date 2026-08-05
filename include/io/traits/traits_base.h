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
 *         requires (std::is_same_v<TChar, typename TIter::value_type>)
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
 * `io_traits<TChar, _Ws>::sread` 就是自己加锁并构造哨兵的）：
 * ```cpp
 * template <ostream_type T> static void swrite(T& s, const MyType& v);
 * template <istream_type T> static void sread (T& s, const MyType& v);
 * ```
 *
 * 两种形式靠**参数个数**区分，一个特化**只应提供其中一种**——两种都提供时，运算符选中哪一种
 * 不作保证。插入端还会把 `TValue` 衰退一次再试一遍（这样数组名能衰退成指针、函数名能衰退成
 * 函数指针），同一种形式内不衰退的 `TValue` 优先。
 *
 * 库内置的操纵符没有 `operator()`：单向的（`ws`、`endl`、`ends`、`flush`）是空标记类型，双向的
 * 只携带参数，逻辑一律写在本扩展点里——一行写得下的就两个成员各写一遍，写不下的走一个私有静态
 * 辅助函数。入口于是只剩 `os << m` / `is >> m` 一条，异常统一由运算符接住转交
 * `handle_exception`，操纵符自身不必各兜一遍。代价是标准的 `std::ws(is)` / `std::endl(os)`
 * 直接调用形式在本库不存在。
 *
 * 唯一不经过本扩展点的是只取 `ios_base<TChar>&` 的**函数指针**操纵符：插入端与提取端各有一条
 * 专门的运算符重载，其形参类型必须是非推导语境——`os << IOv2::boolalpha` 里的操纵符是函数
 * 模板，形参不先定下来就推不出模板实参、取不到函数地址。只支持函数指针这一种形状；带状态的
 * 操纵符走本扩展点的流形式即可，它拿到的是真正的流，比 `ios_base&` 能做的更多。
 *
 * ### 方向
 *
 * 方向由**哪个成员存在**决定：只有 `swrite` 即只能插入，只有 `sread` 即只能提取，两个都有
 * 即两个方向都行。这一点不能改用约束来表达——`iostream` 同时满足 `istream_type` 与
 * `ostream_type`，光靠约束挡不住反向用法。用反了会撞上运算符链末尾的 `static_assert`，它会
 * 区分"根本没有 `io_traits`"和"有 `io_traits`，但这个方向没有能用的成员"。
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
 * 回退值的做法，见 `io/traits/tm.h`。主模板是恒等映射，不需要这一层就不用管它。
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
 *         requires (std::is_same_v<TChar, typename TIter::value_type>)
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
 * lock and no sentry** -- do it yourself if you need one, as `io_traits<TChar, _Ws>::sread`
 * does):
 * ```cpp
 * template <ostream_type T> static void swrite(T& s, const MyType& v);
 * template <istream_type T> static void sread (T& s, const MyType& v);
 * ```
 *
 * The two forms are told apart by **arity**, and a specialization should provide **only one of
 * them** -- when both are present, which one the operator picks is unspecified. The insertion side
 * also retries with `TValue` decayed once, which is what lets an array name decay to a pointer and
 * a function name to a function pointer; within one form the undecayed `TValue` wins.
 *
 * The library's own manipulators have no `operator()`: the one-way ones (`ws`, `endl`, `ends`,
 * `flush`) are empty tag types, the two-way ones carry their parameters only, and the logic always
 * lives in this extension point -- spelled out in both members when it fits on one line, factored
 * into a private static helper when it does not. That leaves `os << m` / `is >> m` as the only
 * entry, so exceptions are caught by the operator and handed to `handle_exception` in one place
 * instead of in every manipulator. The price is that the standard's direct-call forms,
 * `std::ws(is)` and `std::endl(os)`, do not exist here.
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
 * `sread` only means extraction only, and both means both. Constraints cannot express this --
 * an `iostream` satisfies `istream_type` and `ostream_type` alike, so a constraint alone cannot
 * stop a backwards use. Using one backwards hits the `static_assert` at the end of the
 * operator's chain, which distinguishes "no `io_traits` at all" from "an `io_traits` exists, but
 * offers no member usable in this direction".
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
 * fields the format string does not parse; see `io/traits/tm.h`. The primary template is the
 * identity, so ignore this layer if you do not need it.
 * @endif
 */
#pragma once

namespace IOv2
{
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

/**
 * @lang{ZH}
 * @brief `io_traits<TChar, T>` 是否有定义（即是否存在特化），不问方向。
 * @note 运算符用它把"根本没有扩展点"与"有扩展点、但这个方向没有能用的成员"分成两条不同的
 *       诊断。方向本身不能靠它判断——一个 `io_traits` 可以只提供其中一个方向。
 * @endif
 *
 * @lang{EN}
 * @brief Whether `io_traits<TChar, T>` is defined (that is, whether a specialization exists),
 *        regardless of direction.
 * @note The operators use it to split "no extension point at all" from "an extension point
 *       exists, but has no member usable in this direction" into two distinct diagnostics. It
 *       says nothing about direction -- an `io_traits` may provide only one of the two.
 * @endif
 */
template <typename TChar, typename T>
concept has_io_traits = requires(TChar, T)
{
    sizeof(io_traits<TChar, T>);
};
}
