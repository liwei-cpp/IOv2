/**
 * @file io_manip.h
 * @lang{ZH}
 * 定义了带参数的流操纵符：`resetiosflags` / `setiosflags` / `setbase` / `setfill` /
 * `setprecision` / `setw`（调整格式化状态），以及 `put_money` / `get_money` /
 * `put_time` / `get_time`（按 locale 格式化或解析货币与时间）。
 *
 * 每个操纵符由一个工厂函数和一个 `_Xxx` 载体类型构成，后者是**实现细节**、不应被直接构造；
 * 调整格式状态的那几个各配一对 `operator<<` / `operator>>`，货币与时间那几个则走
 * `writer` / `reader` 特化，经通用的格式化插入/提取运算符生效。
 *
 * @warning **操纵符与随后的 I/O 不构成同一个临界区。** `os << setw(5) << value` 是两次
 *          各自加锁的操作：`setw(5)` 写入 `width`，`<< value` 在另一个临界区里读取并用完
 *          后把它清零。多线程共享同一个流时，另一个线程可能在两者之间插入自己的
 *          `setw`，双方互相吃掉对方的宽度。这与标准库的行为一致，但本库在其余地方对
 *          "单次操作原子"的保证更强，容易让人误以为这里也是。需要整体原子时，用
 *          `IOv2::sync` 把它们圈进同一个临界区。
 * @warning **工厂函数返回的是临时对象，只应作为同一个完整表达式的一部分立即使用。**
 *          `_Put_money` / `_Get_money` 持有对实参的引用，`_Put_time` / `_Get_time` 持有
 *          裸指针。写成 `os << put_money(x)` 是安全的——临时量活到完整表达式结束；但
 *          `auto m = put_money(compute()); os << m;` 会悬垂，因为 `compute()` 的临时结果
 *          在第一条语句结束时就已销毁。此契约与 `std::put_money` 等同。
 * @endif
 *
 * @lang{EN}
 * Defines the parameterized stream manipulators: `resetiosflags`, `setiosflags`, `setbase`,
 * `setfill`, `setprecision` and `setw` (which adjust formatting state), plus `put_money`,
 * `get_money`, `put_time` and `get_time` (which format or parse money and time under the
 * locale).
 *
 * Each manipulator consists of a factory function and a `_Xxx` carrier type; the latter is an
 * **implementation detail** and should never be constructed directly. The formatting-state
 * ones each come with a matching `operator<<` / `operator>>` pair, while the money and time
 * ones go through `writer` / `reader` specializations and take effect via the generic
 * formatted insertion/extraction operators.
 *
 * @warning **A manipulator and the I/O that follows it are not one critical section.**
 *          `os << setw(5) << value` is two separately-locked operations: `setw(5)` writes
 *          `width`, and `<< value` reads it in a different critical section and resets it to
 *          zero once used. With a stream shared between threads, another thread's `setw` can
 *          land in between and the two steal each other's width. This matches the standard
 *          library, but this library's stronger "a single operation is atomic" guarantee
 *          elsewhere makes it easy to assume otherwise. To make a group atomic, wrap it in one
 *          critical section with `IOv2::sync`.
 * @warning **A factory returns a temporary, to be used only as part of the same full
 *          expression.** `_Put_money` / `_Get_money` hold a reference to the argument, and
 *          `_Put_time` / `_Get_time` hold raw pointers. `os << put_money(x)` is safe -- the
 *          temporary lives to the end of the full expression -- but
 *          `auto m = put_money(compute()); os << m;` dangles, because `compute()`'s temporary
 *          result is already destroyed at the end of the first statement. This contract is the
 *          same as `std::put_money`'s.
 * @endif
 */
#pragma once

#include <io/fp_defs/tm.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/iostream.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief `resetiosflags` 操纵符的类型。
 *
 * 本操纵符只改格式标志，两个方向都合法，故同时派生自 `in_manip` 与 `out_manip`。方向为何
 * 必须编码进类型、以及"两个方向都合法者须同时派生两个基类"的理由，见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `resetiosflags` manipulator.
 *
 * It only changes format flags and is legal in both directions, so it derives from both
 * `in_manip` and `out_manip`. See `in_manip` for why the direction must live in the type, and
 * why something legal in both directions has to derive from both bases.
 * @endif
 */
struct _Resetiosflags : in_manip, out_manip
{
    explicit _Resetiosflags(ios_defs::fmtflags mask) : m_mask(mask) {}

    /**
     * @lang{ZH} @brief 清除 `m_mask` 中的各标志，其余不受影响。 @endif
     * @lang{EN} @brief Clears the flags in `m_mask`, leaving the rest untouched. @endif
     */
    template <typename T>
        requires (istream_type<T> || ostream_type<T>)
    void operator () (T& s) const
    {
        s.setf(ios_defs::fmtflags(0), m_mask);
    }

    ios_defs::fmtflags m_mask;
};

/**
 * @lang{ZH} @brief 构造清除 @p mask 中各标志的操纵符（其余标志不受影响）。 @endif
 * @lang{EN} @brief Builds the manipulator that clears the flags in @p mask, leaving the rest untouched. @endif
 */
inline _Resetiosflags resetiosflags(ios_defs::fmtflags mask) { return _Resetiosflags{mask}; }

/**
 * @lang{ZH}
 * @brief `setiosflags` 操纵符的类型。两个方向都合法，故同时派生两个方向标签；见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setiosflags` manipulator. Legal in both directions, so it derives
 *        from both direction tags; see `in_manip`.
 * @endif
 */
struct _Setiosflags : in_manip, out_manip
{
    explicit _Setiosflags(ios_defs::fmtflags mask) : m_mask(mask) {}

    /**
     * @lang{ZH} @brief 置位 `m_mask` 中的各标志（按位或），其余不受影响。 @endif
     * @lang{EN} @brief Sets the flags in `m_mask` (bitwise-or), leaving the rest untouched. @endif
     */
    template <typename T>
        requires (istream_type<T> || ostream_type<T>)
    void operator () (T& s) const
    {
        s.setf(m_mask);
    }

    ios_defs::fmtflags m_mask;
};

/**
 * @lang{ZH} @brief 构造置位 @p mask 中各标志的操纵符（按位或，其余标志不受影响）。 @endif
 * @lang{EN} @brief Builds the manipulator that sets the flags in @p mask (bitwise-or; the rest are untouched). @endif
 */
inline _Setiosflags setiosflags(ios_defs::fmtflags mask) { return _Setiosflags{mask}; }

/**
 * @lang{ZH}
 * @brief `setbase` 操纵符的类型。两个方向都合法，故同时派生两个方向标签；见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setbase` manipulator. Legal in both directions, so it derives from
 *        both direction tags; see `in_manip`.
 * @endif
 */
struct _Setbase : in_manip, out_manip
{
    explicit _Setbase(int base) : m_base(base) {}

    /**
     * @lang{ZH} @brief 按 `m_base` 设置 `basefield`；取值的含义见 `setbase`。 @endif
     * @lang{EN} @brief Sets `basefield` from `m_base`; see `setbase` for what the values mean. @endif
     */
    template <typename T>
        requires (istream_type<T> || ostream_type<T>)
    void operator () (T& s) const
    {
        s.setf(m_base ==  8 ? ios_defs::oct :
               m_base == 10 ? ios_defs::dec :
               m_base == 16 ? ios_defs::hex :
               ios_defs::fmtflags(0), ios_defs::basefield);
    }

    int m_base;
};

/**
 * @lang{ZH}
 * @brief 构造设置整数进制的操纵符。
 * @param base 目标进制：8、10、16 分别对应 `oct` / `dec` / `hex`。
 * @note 其它取值会把 `basefield` 整个清零，这不是错误：输出时按十进制处理，输入时按内容
 *       自动判定进制（`0x` 前缀为十六进制、前导 `0` 为八进制，否则十进制）。行为与
 *       `std::setbase` 一致。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that selects the integer base.
 * @param base The target base: 8, 10 and 16 map to `oct` / `dec` / `hex`.
 * @note Any other value clears `basefield` entirely, which is not an error: output is then
 *       decimal, and input determines the base from its content (a `0x` prefix means
 *       hexadecimal, a leading `0` octal, otherwise decimal). This matches `std::setbase`.
 * @endif
 */
inline _Setbase setbase(int base) { return _Setbase{base}; }

/**
 * @lang{ZH}
 * @brief `setfill` 操纵符的类型。两个方向都合法，故同时派生两个方向标签；见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setfill` manipulator. Legal in both directions, so it derives from
 *        both direction tags; see `in_manip`.
 * @endif
 */
template<typename _CharT>
struct _Setfill : in_manip, out_manip
{
    explicit _Setfill(_CharT c) : m_c(c) {}

    /**
     * @lang{ZH}
     * @brief 设置流的填充字符。
     * @note 约束要求 `_CharT` 与流的 `char_type` **完全一致**，不接受隐式转换；理由见
     *       `setfill`。这条约束此前由形参类型 `_Setfill<typename T::char_type>`（非推导
     *       语境）承担，现在由本 `requires` 承担，二者效果相同。
     * @endif
     *
     * @lang{EN}
     * @brief Sets the stream's fill character.
     * @note The constraint requires `_CharT` to match the stream's `char_type` **exactly**, with
     *       no implicit conversion; see `setfill` for why. That requirement used to be carried
     *       by the parameter type `_Setfill<typename T::char_type>`, a non-deduced context, and
     *       is now carried by this `requires` to the same effect.
     * @endif
     */
    template <typename T>
        requires ((istream_type<T> || ostream_type<T>)
                  && std::same_as<typename T::char_type, _CharT>)
    void operator () (T& s) const
    {
        s.fill(m_c);
    }

    _CharT m_c;
};

/**
 * @lang{ZH}
 * @brief 构造设置填充字符的操纵符；填充字符用于字段宽度大于内容时补齐。
 * @note `_CharT` 由实参推导，且必须与目标流的 `char_type` **完全一致**——`_Setfill` 的
 *       `operator()` 要求二者 `same_as`，不存在隐式转换。因此对 `wchar_t` 流须写
 *       `setfill(L'*')`，写成 `setfill('*')` 会编译失败而非静默转换。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the fill character, used to pad when the field width
 *        exceeds the content.
 * @note `_CharT` is deduced from the argument and must match the target stream's `char_type`
 *       **exactly**: `_Setfill`'s `operator()` requires the two to be `same_as`, so no implicit
 *       conversion applies. A `wchar_t` stream therefore needs `setfill(L'*')`; `setfill('*')`
 *       fails to compile rather than converting silently.
 * @endif
 */
template<typename _CharT>
inline _Setfill<_CharT> setfill(_CharT c) { return _Setfill<_CharT>{c}; }

/**
 * @lang{ZH}
 * @brief `setprecision` 操纵符的类型。两个方向都合法，故同时派生两个方向标签；见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setprecision` manipulator. Legal in both directions, so it derives
 *        from both direction tags; see `in_manip`.
 * @endif
 */
struct _Setprecision : in_manip, out_manip
{
    explicit _Setprecision(size_t n) : m_n(n) {}

    /**
     * @lang{ZH} @brief 设置流的浮点精度。
     *           @throw stream_error 若精度超出 0..255：置 `strfailbit`，并仅当该位在流的异常
     *           掩码中时才抛出。 @endif
     * @lang{EN} @brief Sets the stream's floating-point precision.
     *           @throw stream_error If the precision is outside 0..255: `strfailbit` is set, and
     *           it is thrown only if that bit is in the stream's exception mask. @endif
     */
    template <typename T>
        requires (istream_type<T> || ostream_type<T>)
    void operator () (T& s) const
    {
        try
        {
            if (m_n > std::numeric_limits<std::uint8_t>::max())
                throw stream_error("setprecision fail: precision out of range (0..255)");

            s.precision(static_cast<std::uint8_t>(m_n));
        }
        catch (...)
        {
            s.handle_exception(std::current_exception());
        }
    }

    size_t m_n;
};

/**
 * @lang{ZH}
 * @brief 构造设置浮点精度的操纵符。
 *
 * 精度以 `std::uint8_t` 存储（见 `ios_base::precision`），有效范围 0..255。参数取
 * `size_t` 而非 `std::uint8_t`，是为了让越界值被**显式拒绝**而不是被静默回绕：
 * 若形参本身就是 `std::uint8_t`，`setprecision(300)` 会经隐式转换悄悄变成 44，
 * 而运行期变量连编译警告都不会有。
 * @note 越界的检查在操纵符**作用于流时**才做，本函数只保存原值。`_Setprecision::operator()`
 *       就地把 `stream_error` 交给 `handle_exception`，转成 `strfailbit` 并遵守流的异常掩码，
 *       与本库其余的失败一致；若在构造时抛，异常会从 `os << ...` 表达式里直接逃逸到调用方，
 *       流却仍报告 `good()`。就地处理而非依赖 `operator<<` / `operator>>` 的兜底 catch，是为了
 *       让直接调用形式 `setprecision(n)(os)` 也走同一条错误路径。
 * @param n 目标精度，必须落在 0..255。
 * @return 可用于 `os << setprecision(n)` / `is >> setprecision(n)` 的操纵符。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the floating-point precision.
 *
 * The precision is stored as a `std::uint8_t` (see `ios_base::precision`), so the valid
 * range is 0..255. The parameter is a `size_t` rather than a `std::uint8_t` so that an
 * out-of-range value is **rejected explicitly** instead of silently wrapping: with a
 * `std::uint8_t` parameter, `setprecision(300)` would quietly become 44 via the implicit
 * conversion, and a run-time argument would not even produce a compiler warning.
 * @note The range check runs when the manipulator is **applied to a stream**; this function
 *       only stores the value. `_Setprecision::operator()` hands the `stream_error` to
 *       `handle_exception` in place, where it becomes a `strfailbit` and honours the stream's
 *       exception mask, like every other failure in this library. Thrown at construction it
 *       would instead escape an `os << ...` expression straight to the caller while the stream
 *       still reported `good()`. Handling it in place rather than leaning on the backstop
 *       `catch` in `operator<<` / `operator>>` is what puts the direct-call form
 *       `setprecision(n)(os)` on the same error path.
 * @param n The target precision; must lie in 0..255.
 * @return A manipulator usable as `os << setprecision(n)` / `is >> setprecision(n)`.
 * @endif
 */
inline _Setprecision setprecision(size_t n) { return _Setprecision{n}; }

/**
 * @lang{ZH}
 * @brief `setw` 操纵符的类型。两个方向都合法，故同时派生两个方向标签；见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setw` manipulator. Legal in both directions, so it derives from both
 *        direction tags; see `in_manip`.
 * @endif
 */
struct _Setw : in_manip, out_manip
{
    explicit _Setw(std::ptrdiff_t n) : m_n(n) {}

    /**
     * @lang{ZH} @brief 设置流的字段宽度。
     *           @throw stream_error 若宽度为负：置 `strfailbit`，并仅当该位在流的异常掩码中时
     *           才抛出。 @endif
     * @lang{EN} @brief Sets the stream's field width.
     *           @throw stream_error If the width is negative: `strfailbit` is set, and it is
     *           thrown only if that bit is in the stream's exception mask. @endif
     */
    template <typename T>
        requires (istream_type<T> || ostream_type<T>)
    void operator () (T& s) const
    {
        try
        {
            if (m_n < 0)
                throw stream_error("setw fail: negative width");

            s.width(static_cast<size_t>(m_n));
        }
        catch (...)
        {
            s.handle_exception(std::current_exception());
        }
    }

    std::ptrdiff_t m_n;
};

/**
 * @lang{ZH}
 * @brief 构造设置字段宽度的操纵符。
 *
 * 宽度以 `size_t` 存储（见 `ios_base::width`），不设上限。
 *
 * @note 形参有符号，是为了挡住负宽度：`setw(total - str.size())` 这类算式在
 *       `total < str.size()` 时按无符号回绕成接近 `2^64` 的巨值，取 `ptrdiff_t` 形参可让传参
 *       时的窄化把回绕抵消回来、还原成负数并被拒。判负后转 `size_t` 不会溢出。
 * @note 判负在操纵符**作用于流时**才做，本函数只保存原值。`_Setw::operator()` 就地把
 *       `stream_error` 交给 `handle_exception`，转成 `strfailbit` 并遵守流的异常掩码，与本库
 *       其余的失败一致；若在构造时抛，异常会从 `os << ...` 表达式里直接逃逸到调用方，流却仍
 *       报告 `good()`。就地处理而非依赖 `operator<<` / `operator>>` 的兜底 catch，是为了让直接
 *       调用形式 `setw(n)(os)` 也走同一条错误路径。
 * @note **提取端的长度安全不依赖本函数。** 目标缓冲区的上界始终来自类型本身：
 *       `reader<TChar, TChar[N]>` 以 `min(width, N)` 为界，`std::basic_string` 自动增长，
 *       而裸指针根本没有对应的 reader（`is >> ptr` 无法编译，与 C++20 起的 `std::istream`
 *       一致）。因此 `width` 只能把边界**收紧**，不会造成越界写。
 * @note 与标准一致，`width` 只被字符数组与 `std::basic_string` 的提取消费；算术提取、
 *       `get_money`、`get_time` 之后 `width` 仍然保留。
 * @param n 目标宽度，不得为负。
 * @return 可用于 `os << setw(n)` / `is >> setw(n)` 的操纵符。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the field width.
 *
 * The width is stored as a `size_t` (see `ios_base::width`), with no upper bound.
 *
 * @note The parameter is signed so that a negative width can be rejected: an expression such as
 *       `setw(total - str.size())` wraps as unsigned into a value near `2^64` when
 *       `total < str.size()`, and a `ptrdiff_t` parameter lets the narrowing at the call undo
 *       that wrap, restoring the negative value to be rejected. The conversion to `size_t`
 *       after the check cannot overflow.
 * @note The sign check runs when the manipulator is **applied to a stream**; this function only
 *       stores the value. `_Setw::operator()` hands the `stream_error` to `handle_exception` in
 *       place, where it becomes a `strfailbit` and honours the stream's exception mask, like
 *       every other failure in this library. Thrown at construction it would instead escape an
 *       `os << ...` expression straight to the caller while the stream still reported `good()`.
 *       Handling it in place rather than leaning on the backstop `catch` in `operator<<` /
 *       `operator>>` is what puts the direct-call form `setw(n)(os)` on the same error path.
 * @note **Length safety on the extraction side does not depend on this function.** The bound
 *       on a destination buffer always comes from its type: `reader<TChar, TChar[N]>` bounds
 *       at `min(width, N)`, `std::basic_string` grows on demand, and a raw pointer has no
 *       reader at all (`is >> ptr` does not compile, matching `std::istream` as of C++20).
 *       `width` can therefore only **tighten** a bound, never cause an out-of-bounds write.
 * @note Matching the standard, `width` is consumed only by character-array and
 *       `std::basic_string` extraction; it survives arithmetic extraction, `get_money` and
 *       `get_time`.
 * @param n The target width; must not be negative.
 * @return A manipulator usable as `os << setw(n)` / `is >> setw(n)`.
 * @endif
 */
inline _Setw setw(std::ptrdiff_t n) { return _Setw{n}; }

template<typename _MoneyT> struct _Put_money { const _MoneyT& m_mon; bool m_intl; };
/**
 * @lang{ZH}
 * @brief 构造按 locale 的货币格式写出 @p mon 的操纵符。
 * @param mon 货币值。可为整型（以最小货币单位计，如"分"），或已是 `char_type` 数字串的
 *            `std::basic_string`。**注意本库不接受浮点**，这一点与接受 `long double` 的
 *            `std::put_money` 不同（约束来自 `monetary::put`）。
 * @param intl `true` 用国际格式（如 `USD`），`false` 用本地格式（如 `$`）。
 * @warning 返回的对象**持有对 @p mon 的引用**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that writes @p mon in the locale's monetary format.
 * @param mon The monetary value: either an integral type counted in the smallest currency unit
 *            (cents, say), or a `std::basic_string` of `char_type` digits. **Floating-point is
 *            not accepted here**, unlike `std::put_money` which takes a `long double`; the
 *            constraint comes from `monetary::put`.
 * @param intl `true` selects the international format (e.g. `USD`), `false` the national one
 *             (e.g. `$`).
 * @warning The returned object **holds a reference to @p mon** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @endif
 */
template<typename _MoneyT>
inline _Put_money<_MoneyT> put_money(const _MoneyT& mon, bool intl = false) { return { mon, intl }; }

// remove_cv_t on the bool exclusion only: put() takes the integral by value, so cv is dropped
// there, but its string overload takes a plain const&, which a volatile string cannot bind to.
template <typename TChar, typename TMoney>
    requires ((std::integral<TMoney> && !std::same_as<std::remove_cv_t<TMoney>, bool>)
              || std::same_as<TMoney, std::basic_string<TChar>>)
struct writer<TChar, _Put_money<TMoney>>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, _Put_money<TMoney> f)
    {
        auto mp = loc.template get<monetary<TChar>>();
        if (!mp)
            throw stream_error("cannot get monetary facet");
        
        return mp->put(s, f.m_intl, io, f.m_mon);
    }
};

/**
 * @lang{ZH}
 * @brief 已删除：`put_money` 只能插入，不能提取。
 * @endif
 *
 * @lang{EN}
 * @brief Deleted: `put_money` inserts only; it cannot be extracted.
 * @endif
 */
template <istream_type T, typename TMoney>
T& operator>>(T& is, _Put_money<TMoney> f) = delete;

template<typename _MoneyT> struct _Get_money { _MoneyT& m_mon; bool m_intl; };
/**
 * @lang{ZH}
 * @brief 构造按 locale 的货币格式解析并写入 @p mon 的操纵符。
 * @param mon 接收解析结果的对象；可为整型（以最小货币单位计）或 `std::basic_string`。
 * @param intl `true` 按国际格式解析，`false` 按本地格式解析。
 * @warning 返回的对象**持有对 @p mon 的引用**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that parses a monetary value in the locale's format into
 *        @p mon.
 * @param mon Receives the parsed result; either an integral type counted in the smallest
 *            currency unit, or a `std::basic_string`.
 * @param intl `true` parses the international format, `false` the national one.
 * @warning The returned object **holds a reference to @p mon** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @endif
 */
template<typename _MoneyT>
inline _Get_money<_MoneyT> get_money(_MoneyT& mon, bool intl = false) { return { mon, intl }; }

template <typename TChar, typename TMoney>
    requires (std::same_as<TMoney, std::remove_cv_t<TMoney>>
              && ((std::integral<TMoney> && !std::same_as<TMoney, bool>)
                  || std::same_as<TMoney, std::basic_string<TChar>>))
struct reader<TChar, _Get_money<TMoney>>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, _Get_money<TMoney>& f)
    {
        auto mp = loc.template get<monetary<TChar>>();
        if (!mp)
            throw stream_error("cannot get monetary facet");

        return mp->get(s, s_end, f.m_intl, io, f.m_mon);
    }
};

/**
 * @lang{ZH}
 * @brief 提取操纵符：按当前 locale 的货币格式解析并写入 `get_money` 所引用的对象。
 *
 * @note 本重载**按值**接收操纵符对象，因此支持 `is >> get_money(x)` 这一惯用写法。
 *       泛型的格式化提取运算符取的是非常量左值引用 `TValue&`，而 `get_money(x)` 是
 *       纯右值，绑不上去；若没有本重载，调用方只能先落成具名左值再提取。按值接收不
 *       影响语义：`_Get_money` 的成员本身就是引用，拷贝聚合体只是重新绑定到同一个
 *       被引对象。
 * @param f 由 `get_money()` 构造的操纵符。
 * @return 流自身的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: parses a monetary value under the current locale and
 *        stores it into the object `get_money` refers to.
 *
 * @note This overload takes the manipulator **by value**, which is what makes the
 *       idiomatic `is >> get_money(x)` work. The generic formatted-extraction operator
 *       takes a non-const lvalue reference `TValue&`, and `get_money(x)` is a prvalue,
 *       which cannot bind to it; without this overload a caller would first have to
 *       materialize a named lvalue. Taking it by value changes nothing semantically:
 *       `_Get_money`'s members are references already, so copying the aggregate merely
 *       rebinds to the same referent.
 * @param f The manipulator produced by `get_money()`.
 * @return A reference to the stream itself.
 * @endif
 */
template <istream_type T, typename TMoney>
inline T& operator>>(T& is, _Get_money<TMoney> f)
{
    // Delegate to the generic formatted-extraction operator: it owns the sentry, the
    // skipws handling, the stream iterator (i_iter is private and befriends only that
    // template) and the eofbit bookkeeping. The explicit template argument list is
    // required rather than stylistic -- `is >> f` would re-select *this* overload,
    // because f is a named lvalue and the by-value candidate is the more specialized
    // one, and recurse forever.
    return IOv2::operator>><T, _Get_money<TMoney>>(is, f);
}

/**
 * @lang{ZH}
 * @brief 已删除：`get_money` 只能提取，不能插入。
 * @endif
 *
 * @lang{EN}
 * @brief Deleted: `get_money` extracts only; it cannot be inserted.
 * @endif
 */
template <ostream_type T, typename TMoney>
T& operator<<(T& os, _Get_money<TMoney> f) = delete;

template<typename _CharT> struct _Put_time { const std::tm* tmb; const _CharT* fmt; };
/**
 * @lang{ZH}
 * @brief 构造按 @p fmt 写出 `*tmb` 的操纵符。
 * @param tmb 要写出的时间；允许为空，见下。
 * @param fmt `strftime` 风格的格式串；允许为空，见下。
 * @note 两个指针都会在写出前校验，为空时置流的失败位而非解引用；详见
 *       `writer<TChar, _Put_time<TChar>>::swrite`。
 * @note `put_time` 既不应用也不消耗 `io.width()`：填充与随后的 `width(0)` 由各自的 facet
 *       负责，而 `timeio` 的写出路径完全不涉及 width。因此 `os << setw(20) << put_time(...)`
 *       不会补齐到 20 列，且这个 width 会原样留给下一次插入。这与 `std::put_time` 的行为
 *       一致（`time_put` 同样不处理 width），但与 `put_money`、算术类型的插入不同——后两者
 *       经由 `monetary` / `numeric` facet，会消费掉 width。
 * @warning **`*tmb` 必须描述一个完整且真实存在的时刻，而不只是 @p fmt 用到的那几个字段。**
 *          写出前 `timeio::put` 会校验全部字段：`tm_mon` 属于 [0,11]、`tm_mday` 属于 [1,31]、
 *          `tm_hour` 属于 [0,23]、`tm_min` 与 `tm_sec` 属于 [0,59]（**不接受闰秒 `tm_sec == 60`**）、
 *          年份在 `std::chrono::year` 的范围内，且三者组合必须是真实存在的日历日（2 月 30 日
 *          会被拒）。任一项不满足都会抛出，经 `handle_exception` 置 `strfailbit`：本次插入什么
 *          都不输出，且在 `clear()` 之前该流上后续的插入都会被 sentry 拒掉。
 * @note **与 `std::put_time` 的分歧。** 标准是 `strftime` 语义，逐说明符取字段（C11 7.27.3.5：
 *       每个说明符只读其描述中方括号列出的成员，且"若任一被用到的值超出正常范围，存入的字符
 *       未指定"——是 unspecified，不是错误）。因此 `std::put_time(&t, "%Y")` 配 `std::tm t{}`
 *       会正常输出 `1900`（`%Y` 不读 `tm_mday`），`%S` 也接受闰秒。本库则把 `*tmb` 整体转成
 *       `std::chrono` 类型再格式化，要求它整体自洽，上述用法在这里都会失败。从 `std::ostream`
 *       迁移"只格式化部分字段"的代码（如 `os << put_time(&t, "%H:%M")` 而 `t` 的日期未填）时
 *       需要注意这一点。
 * @note **`tm_wday` 与 `tm_yday` 不被读取。** 星期由 y/m/d 重新推算，与调用方填的值无关。
 *       这与 `strftime` 的 `%a`/`%A`（读 `tm_wday`）、`%j`（读 `tm_yday`）不同：若调用方填入
 *       与日期不符的值，标准库按该值输出，本库按真实日期输出，双方都不报错。
 * @note 提取侧的契约**更宽松**：`get_time` 接受 `std::tm t{}`（`tm_mday == 0` 会被归一化成上月
 *       最后一天），`put_time` 不接受。两者刻意不对称，详见 `get_time`。
 * @warning 返回的对象**持有这两个裸指针**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that writes `*tmb` according to @p fmt.
 * @param tmb The time to write; may be null, see below.
 * @param fmt A `strftime`-style format string; may be null, see below.
 * @note Both pointers are validated before the write, and a null one sets a failure bit on the
 *       stream rather than being dereferenced; see
 *       `writer<TChar, _Put_time<TChar>>::swrite`.
 * @note `put_time` neither applies nor consumes `io.width()`. Padding and the subsequent
 *       `width(0)` are the responsibility of the individual facets, and the `timeio` write path
 *       never looks at the width. So `os << setw(20) << put_time(...)` does not pad to 20
 *       columns, and that width is left in place for the next insertion. This matches
 *       `std::put_time` (`time_put` likewise ignores the width), but differs from `put_money`
 *       and from arithmetic insertion, both of which go through the `monetary` / `numeric`
 *       facets and do consume it.
 * @warning **`*tmb` must describe a complete instant that really exists, not merely the fields
 *          @p fmt happens to use.** Before writing, `timeio::put` validates every field:
 *          `tm_mon` in [0,11], `tm_mday` in [1,31], `tm_hour` in [0,23], `tm_min` and `tm_sec`
 *          in [0,59] (**a leap second, `tm_sec == 60`, is rejected**), the year within the range
 *          of `std::chrono::year`, and the three date fields together must form a calendar day
 *          that exists (February 30 is rejected). Anything else throws, and `handle_exception`
 *          turns that into `strfailbit`: the insertion writes nothing, and every later insertion
 *          on that stream is refused by the sentry until an explicit `clear()`.
 * @note **Divergence from `std::put_time`.** The standard has `strftime` semantics, taking
 *       fields per specifier (C11 7.27.3.5: a specifier reads only the members listed in
 *       brackets in its description, and "if any of the specified values is outside the normal
 *       range, the characters stored are unspecified" -- unspecified, not an error). So
 *       `std::put_time(&t, "%Y")` with a `std::tm t{}` prints `1900` (`%Y` never looks at
 *       `tm_mday`), and `%S` accepts a leap second. This library instead converts `*tmb` as a
 *       whole into `std::chrono` types before formatting, and so requires it to be internally
 *       consistent; every one of those uses fails here. Keep this in mind when migrating code
 *       from `std::ostream` that formats only some fields, such as
 *       `os << put_time(&t, "%H:%M")` with the date left unset.
 * @note **`tm_wday` and `tm_yday` are not read.** The weekday is recomputed from y/m/d,
 *       independently of whatever the caller stored. This differs from `strftime`, whose
 *       `%a`/`%A` read `tm_wday` and whose `%j` reads `tm_yday`: given a value inconsistent with
 *       the date, the standard library prints that value while this library prints the one
 *       implied by the date, neither reporting an error.
 * @note The extraction side is **more permissive**: `get_time` accepts a `std::tm t{}` (a
 *       `tm_mday` of 0 is normalized to the last day of the previous month) whereas `put_time`
 *       does not. The asymmetry is deliberate; see `get_time`.
 * @warning The returned object **holds those two raw pointers** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @endif
 */
template<typename _CharT>
inline _Put_time<_CharT> put_time(const std::tm* tmb, const _CharT* fmt) { return { tmb, fmt }; }

template <typename TChar>
struct writer<TChar, _Put_time<TChar>>
{
    /**
     * @lang{ZH}
     * @brief 按 `fmt` 的格式将 `*tmb` 写出。
     *
     * @note `put_time` 保存的是两个裸指针，两者都在此处校验后才解引用。空指针在这里是
     *       **未定义行为**而非可恢复的错误：`*(f.tmb)` 会把引用绑定到空指针，而 `f.fmt`
     *       会隐式转换成 `std::basic_string_view`，走 `char_traits::length(nullptr)`。
     *       二者都不是异常，`operator<<` 外层的 `catch` 接不住，直接崩溃。这也是为什么
     *       校验必须放在这里，而不是靠调用方自觉——`localtime()` 失败返回 `nullptr`、
     *       格式串来自配置或环境变量，都是很常见的真实路径。
     * @note 校验放在 `swrite` 而非 `put_time` 工厂里，是为了让异常落进 `operator<<` 的
     *       catch，经 `handle_exception` 归类为 `strfailbit`，与 `ostream::write` 等处的
     *       空指针处理保持一致的错误模型。
     * @param f 待写出的时间与格式串；`*(f.tmb)` 必须是完整有效的时刻（见 `put_time`）。
     *          `f.tmb` 或 `f.fmt` 为空指针时什么都不写出，抛出的 `stream_error` 由流转为
     *          `strfailbit`。
     * @return 指向最后一个写入位置之后的输出迭代器。
     * @throw stream_error 若 `f.tmb` 或 `f.fmt` 为空指针，或 `*(f.tmb)` 的字段越界（含闰秒
     *        `tm_sec == 60`）、或其日期组合不是真实存在的日历日，或缺少 timeio facet。
     * @endif
     *
     * @lang{EN}
     * @brief Writes `*tmb` formatted according to `fmt`.
     *
     * @note `put_time` stores two raw pointers; both are validated here before being
     *       dereferenced. A null pointer is **undefined behavior** here, not a recoverable
     *       error: `*(f.tmb)` binds a reference to a null pointer, and `f.fmt` converts
     *       implicitly to a `std::basic_string_view`, reaching
     *       `char_traits::length(nullptr)`. Neither is an exception, so the enclosing
     *       `catch` in `operator<<` cannot intercept it and the process crashes. That is why
     *       the check belongs here rather than being left to the caller -- a failing
     *       `localtime()` returning `nullptr`, or a format string coming from configuration
     *       or the environment, are entirely ordinary paths.
     * @note The check lives in `swrite` rather than in the `put_time` factory so that the
     *       exception lands in `operator<<`'s catch and is categorized as `strfailbit` by
     *       `handle_exception`, matching the error model of the null-pointer checks in
     *       `ostream::write` and friends.
     * @param f The time and format string to write; `*(f.tmb)` must be a complete, valid instant
     *          (see `put_time`). If `f.tmb` or `f.fmt` is null nothing is written, and the
     *          `stream_error` thrown is turned into `strfailbit` by the stream.
     * @return An output iterator past the last written position.
     * @throw stream_error If `f.tmb` or `f.fmt` is a null pointer, if a field of `*(f.tmb)` is
     *        out of range (including a leap second, `tm_sec == 60`) or its date fields do not
     *        form a calendar day that exists, or the timeio facet is missing.
     * @endif
     */
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, _Put_time<TChar> f)
    {
        if (f.tmb == nullptr || f.fmt == nullptr)
            throw stream_error("put_time fail: null tm or format pointer");

        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        return mp->put(s, *(f.tmb), f.fmt);
    }
};

/**
 * @lang{ZH}
 * @brief 已删除：`put_time` 只能插入，不能提取。
 * @endif
 *
 * @lang{EN}
 * @brief Deleted: `put_time` inserts only; it cannot be extracted.
 * @endif
 */
template <istream_type T, typename TChar>
T& operator>>(T& is, _Put_time<TChar> f) = delete;

template<typename _CharT> struct _Get_time { std::tm* tmb; const _CharT* fmt; };
/**
 * @lang{ZH}
 * @brief 构造按 @p fmt 解析时间并写入 `*tmb` 的操纵符。
 * @param tmb 接收解析结果的 `tm`；允许为空，见下。
 * @param fmt `strptime` 风格的格式串；允许为空，见下。
 * @note 两个指针都会在解析前校验，为空时置流的失败位而非解引用；详见
 *       `reader<TChar, _Get_time<TChar>>::sread`。
 * @note 格式串中未出现的字段保留 `*tmb` 原有的取值：解析上下文由 `*tmb` 铺好回退值，故
 *       `%H:%M` 这样只解析时间的格式串不会动到日期。`tm_wday` / `tm_yday` 总是由最终日期
 *       重新推算，`tm_isdst` 总是置为 -1——没有格式符携带夏令时信息，而沿用调用方的旧值会
 *       在日期被改写后变成错的；-1 表示"未知，交由 C 库判定"。
 * @note 被读作回退值的只有 `tm_year`、`tm_mon`、`tm_mday`、`tm_hour`、`tm_min`、`tm_sec` 六项。
 *       因此格式串若已解析出全部六项，结果与 `*tmb` 传入时的内容**无关**；只有格式串留下空缺
 *       时，传入的内容才会影响结果。
 * @warning **`*tmb` 必须是已初始化的对象**（例如 `std::tm t{}`）。上一条说的"无关"指的是
 *          结果，不是读取：那六项在解析开始前会被**无条件读取**一次以铺好回退值，与格式串
 *          之后是否覆盖它们无关。因此传入未初始化的 `std::tm` 即为未定义行为
 *          （[basic.indet]/2 —— 读取 indeterminate value），即使格式串填满了全部六项亦然。
 *          这一点与 `std::get_time` 不同：`std::time_get::get` 只写不读，配未初始化的
 *          `std::tm` 是良好定义的，迁移时需要注意。
 * @note 标准把"沿用还是整体覆盖 `*tmb` 的原有内容"列为 unspecified
 *       （[locale.time.get.virtuals]/15），本库明确选择沿用，并且更进一步：沿用下来的值会参与
 *       日期推导。例如 `*tmb` 为 1 月 31 日、格式串只有 `%m`、输入 `02` 时，本库得到 2 月
 *       28/29 日，而 `std::get_time` 只改写 `tm_mon`，留下并不存在的"2 月 31 日"。
 * @note `*tmb` 中越界的字段在用作回退值前会先归一化（`tm_mday == 0` 取上月最后一天、
 *       `tm_mon == 12` 进位到次年 1 月、时/分/秒超出 `[0, 24h)` 的部分按天进位或借位并入
 *       日期，规则见 `io/fp_defs/tm.h`）；沿用下来的日若在
 *       解析出的月份里不存在，则取该月最后一天，而不是让整次提取失败。因此
 *       `std::tm t{}`（`tm_mday` 为 0）配上只解析时间的格式串，得到的日期与
 *       `std::get_time` 之后再调用 `mktime()` 一致。
 * @note 插入侧不对称：`put_time` 要求 `*tmb` 的所有字段都在范围内、且日期组合真实存在，
 *       `std::tm t{}` 在那边会被拒绝并置 `strfailbit`。本函数接受它（见上一条）。详见
 *       `put_time`。
 * @warning 返回的对象**持有这两个裸指针**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that parses a time according to @p fmt into `*tmb`.
 * @param tmb The `tm` receiving the parsed result; may be null, see below.
 * @param fmt A `strptime`-style format string; may be null, see below.
 * @note Both pointers are validated before parsing, and a null one sets a failure bit on the
 *       stream rather than being dereferenced; see `reader<TChar, _Get_time<TChar>>::sread`.
 * @note Fields absent from the format string keep the value they had in `*tmb`: the parse
 *       context is seeded with fallbacks from `*tmb`, so a time-only format string such as
 *       `%H:%M` does not disturb the date. `tm_wday` / `tm_yday` are always recomputed from the
 *       resulting date, and `tm_isdst` is always set to -1 -- no format specifier carries DST
 *       information, and carrying the caller's old value over would be wrong once the date has
 *       been rewritten; -1 means "unknown, let the C library work it out".
 * @note Only `tm_year`, `tm_mon`, `tm_mday`, `tm_hour`, `tm_min` and `tm_sec` are read as
 *       fallbacks. A format string that parses all six therefore makes the result **independent**
 *       of what `*tmb` held on entry; the incoming contents matter only where the format string
 *       leaves a gap.
 * @warning **`*tmb` must be an initialized object** (a `std::tm t{}`, say). What the previous
 *          note calls independent is the result, not the read: those six fields are read
 *          **unconditionally**, before parsing starts, to seed the fallbacks -- whether the
 *          format string later overwrites them makes no difference. Passing an uninitialized
 *          `std::tm` is therefore undefined behavior ([basic.indet]/2, reading an indeterminate
 *          value) even when the format string fills in all six. This differs from
 *          `std::get_time`, where `std::time_get::get` only writes and never reads, so an
 *          uninitialized `std::tm` is well-defined; keep it in mind when migrating.
 * @note The standard leaves it unspecified whether the previous contents of `*tmb` are kept or
 *       simply overwritten ([locale.time.get.virtuals]/15). This library deliberately keeps them,
 *       and goes one step further: the values kept take part in deducing the date. With `*tmb`
 *       holding January 31, a format string of just `%m` and an input of `02`, this library
 *       yields February 28/29, whereas `std::get_time` rewrites only `tm_mon` and leaves behind a
 *       "February 31" that does not exist.
 * @note Out-of-range fields of `*tmb` are normalized before they are used as fallbacks
 *       (`tm_mday == 0` is the last day of the previous month, `tm_mon == 12` carries into
 *       January of the next year, and an hour/minute/second outside `[0, 24h)` carries into or
 *       borrows from the date; see `io/fp_defs/tm.h` for the rules), and a day
 *       carried over this way that does not exist in the parsed month becomes the last day of
 *       that month rather than failing the extraction. A `std::tm t{}` (whose `tm_mday` is 0)
 *       with a time-only format string therefore yields the same date as `std::get_time`
 *       followed by `mktime()`.
 * @note The insertion side is not symmetric: `put_time` requires every field of `*tmb` to be in
 *       range and the date fields to form a day that exists, and rejects a `std::tm t{}` with
 *       `strfailbit`. This function accepts it (see the previous note). See `put_time`.
 * @warning The returned object **holds those two raw pointers** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @endif
 */
template<typename _CharT>
inline _Get_time<_CharT> get_time(std::tm* tmb, const _CharT* fmt) { return { tmb, fmt }; }

template <typename TChar>
struct reader<TChar, _Get_time<TChar>>
{
    /**
     * @lang{ZH}
     * @brief 按 `fmt` 的格式解析时间并写入 `*tmb`。
     *
     * @note 与 `put_time` 同理，`get_time` 保存的两个裸指针在解引用前必须校验：`f.fmt`
     *       为空时向 `std::basic_string_view` 的隐式转换即为未定义行为，`f.tmb` 为空时
     *       回写 `*(f.tmb)` 是空指针写入。二者都绕过异常机制直接崩溃。
     * @param f 用于接收解析结果的 `tm` 与格式串。`*(f.tmb)` 的现有内容会作为格式串未解析字段
     *          的回退值，详见 `get_time`。`f.tmb` 或 `f.fmt` 为空指针时不解析：不消耗输入、
     *          不回写 `*(f.tmb)`，抛出的 `stream_error` 由流转为 `strfailbit`。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 `f.tmb` 或 `f.fmt` 为空指针，或缺少 timeio facet。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a time according to `fmt` and stores it into `*tmb`.
     *
     * @note As with `put_time`, the two raw pointers `get_time` stores must be validated
     *       before being dereferenced: a null `f.fmt` makes the implicit conversion to
     *       `std::basic_string_view` undefined behavior, and a null `f.tmb` makes the
     *       write-back through `*(f.tmb)` a null-pointer store. Both bypass the exception
     *       machinery and crash outright.
     * @param f The `tm` receiving the parsed result and the format string. The current contents
     *          of `*(f.tmb)` serve as the fallbacks for the fields the format string does not
     *          parse; see `get_time`. If `f.tmb` or `f.fmt` is null nothing is parsed: no input
     *          is consumed, `*(f.tmb)` is not written, and the `stream_error` thrown is turned
     *          into `strfailbit` by the stream.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If `f.tmb` or `f.fmt` is a null pointer, or the timeio facet is
     *        missing.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc, _Get_time<TChar>& f)
    {
        if (f.tmb == nullptr || f.fmt == nullptr)
            throw stream_error("get_time fail: null tm or format pointer");

        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        // The context is date+time without a time zone, matching
        // parse_context_type<TChar, std::tm>; that is also the combination whose
        // explicit operator std::tm() is available.
        auto tmp = parse_context_type<TChar, std::tm>::make_parse_context(*(f.tmb));
        auto res = mp->get(s, s_end, tmp, f.fmt);
        *(f.tmb) = static_cast<std::tm>(tmp);

        return res;
    }
};

/**
 * @lang{ZH}
 * @brief 提取操纵符：按 `fmt` 的格式解析时间并写入 `get_time` 所指的 `std::tm`。
 *
 * @note 与 `get_money` 的重载同理，本重载**按值**接收操纵符对象，使 `is >> get_time(&tm, fmt)`
 *       这一惯用写法可用；详见 `operator>>(T&, _Get_money<TMoney>)` 的说明。
 * @param f 由 `get_time()` 构造的操纵符。
 * @return 流自身的引用。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction manipulator: parses a time according to `fmt` and stores it into the
 *        `std::tm` that `get_time` points to.
 *
 * @note As with the `get_money` overload, this one takes the manipulator **by value** so
 *       that the idiomatic `is >> get_time(&tm, fmt)` works; see the note on
 *       `operator>>(T&, _Get_money<TMoney>)`.
 * @param f The manipulator produced by `get_time()`.
 * @return A reference to the stream itself.
 * @endif
 */
template <istream_type T, typename TChar>
    requires std::is_same_v<TChar, typename T::char_type>
inline T& operator>>(T& is, _Get_time<TChar> f)
{
    if (f.tmb == nullptr || f.fmt == nullptr)
    {
        std::lock_guard guard(is.io_mutex());
        is.handle_exception(
            std::make_exception_ptr(stream_error("get_time fail: null tm or format pointer")));
        return is;
    }

    // Explicit template arguments for the same reason as in the _Get_money overload:
    // `is >> f` would recurse into this very function.
    return IOv2::operator>><T, _Get_time<TChar>>(is, f);
}

/**
 * @lang{ZH}
 * @brief 已删除：`get_time` 只能提取，不能插入。
 * @endif
 *
 * @lang{EN}
 * @brief Deleted: `get_time` extracts only; it cannot be inserted.
 * @endif
 */
template <ostream_type T, typename TChar>
T& operator<<(T& os, _Get_time<TChar> f) = delete;
}