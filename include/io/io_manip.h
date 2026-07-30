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
    explicit _Setprecision(std::uint8_t n) : m_n(n) {}

    /**
     * @lang{ZH} @brief 设置流的浮点精度。 @endif
     * @lang{EN} @brief Sets the stream's floating-point precision. @endif
     */
    template <typename T>
        requires (istream_type<T> || ostream_type<T>)
    void operator () (T& s) const
    {
        s.precision(m_n);
    }

    std::uint8_t m_n;
};

/**
 * @lang{ZH}
 * @brief 构造设置浮点精度的操纵符。
 *
 * 精度以 `std::uint8_t` 存储（见 `ios_base::precision`），有效范围 0..255。参数取
 * `size_t` 而非 `std::uint8_t`，是为了让越界值在此处**显式报错**而不是被静默回绕：
 * 若形参本身就是 `std::uint8_t`，`setprecision(300)` 会经隐式转换悄悄变成 44，
 * 而运行期变量连编译警告都不会有。
 * @param n 目标精度，必须落在 0..255。
 * @return 可用于 `os << setprecision(n)` / `is >> setprecision(n)` 的操纵符。
 * @throw stream_error 若 `n > 255`。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the floating-point precision.
 *
 * The precision is stored as a `std::uint8_t` (see `ios_base::precision`), so the valid
 * range is 0..255. The parameter is a `size_t` rather than a `std::uint8_t` so that an
 * out-of-range value is **reported explicitly** here instead of silently wrapping: with a
 * `std::uint8_t` parameter, `setprecision(300)` would quietly become 44 via the implicit
 * conversion, and a run-time argument would not even produce a compiler warning.
 * @param n The target precision; must lie in 0..255.
 * @return A manipulator usable as `os << setprecision(n)` / `is >> setprecision(n)`.
 * @throw stream_error If `n > 255`.
 * @endif
 */
inline _Setprecision setprecision(size_t n)
{
    if (n > std::numeric_limits<std::uint8_t>::max())
        throw stream_error("setprecision fail: precision out of range (0..255)");
    return _Setprecision{static_cast<std::uint8_t>(n)};
}

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
    explicit _Setw(std::uint8_t n) : m_n(n) {}

    /**
     * @lang{ZH} @brief 设置流的字段宽度。 @endif
     * @lang{EN} @brief Sets the stream's field width. @endif
     */
    template <typename T>
        requires (istream_type<T> || ostream_type<T>)
    void operator () (T& s) const
    {
        s.width(m_n);
    }

    std::uint8_t m_n;
};

/**
 * @lang{ZH}
 * @brief 构造设置字段宽度的操纵符。
 *
 * 宽度以 `std::uint8_t` 存储（见 `ios_base::width`），有效范围 0..255。参数取 `size_t`
 * 而非 `std::uint8_t`，是为了让越界值在此处**显式报错**而不是被静默回绕。
 *
 * @warning 越界值抛异常而非回绕，是为了让 `setw(256)`、`setw(512)` 这类误用**显式失败**，
 *          而不是静默折叠为 0 —— 后者会让"设了一个宽度"看起来生效，实际却等同于从未调用
 *          `setw`，从而给出与调用方意图完全无关的结果。
 * @note **提取端的长度安全不依赖本函数。** 目标缓冲区的上界始终来自类型本身：
 *       `reader<TChar, TChar[N]>` 以 `min(width, N)` 为界，`std::basic_string` 自动增长，
 *       而裸指针根本没有对应的 reader（`is >> ptr` 无法编译，与 C++20 起的 `std::istream`
 *       一致）。因此 `width` 只能把边界**收紧**，越界的 `setw` 至多是一次被拒绝的调用，
 *       不会造成越界写。
 * @note 与标准一致，`width` 只被字符数组与 `std::basic_string` 的提取消费；算术提取、
 *       `get_money`、`get_time` 之后 `width` 仍然保留。
 * @param n 目标宽度，必须落在 0..255。
 * @return 可用于 `os << setw(n)` / `is >> setw(n)` 的操纵符。
 * @throw stream_error 若 `n > 255`。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the field width.
 *
 * The width is stored as a `std::uint8_t` (see `ios_base::width`), so the valid range is
 * 0..255. The parameter is a `size_t` rather than a `std::uint8_t` so that an out-of-range
 * value is **reported explicitly** here instead of silently wrapping.
 *
 * @warning Throwing on an out-of-range value rather than wrapping makes misuse such as
 *          `setw(256)` or `setw(512)` **fail loudly** instead of silently folding to 0 --
 *          which would leave "a width was set" looking effective while behaving exactly as
 *          if `setw` had never been called, giving a result unrelated to the caller's intent.
 * @note **Length safety on the extraction side does not depend on this function.** The bound
 *       on a destination buffer always comes from its type: `reader<TChar, TChar[N]>` bounds
 *       at `min(width, N)`, `std::basic_string` grows on demand, and a raw pointer has no
 *       reader at all (`is >> ptr` does not compile, matching `std::istream` as of C++20).
 *       `width` can therefore only **tighten** a bound, and an out-of-range `setw` is at
 *       worst a rejected call, never an out-of-bounds write.
 * @note Matching the standard, `width` is consumed only by character-array and
 *       `std::basic_string` extraction; it survives arithmetic extraction, `get_money` and
 *       `get_time`.
 * @param n The target width; must lie in 0..255.
 * @return A manipulator usable as `os << setw(n)` / `is >> setw(n)`.
 * @throw stream_error If `n > 255`.
 * @endif
 */
inline _Setw setw(size_t n)
{
    if (n > std::numeric_limits<std::uint8_t>::max())
        throw stream_error("setw fail: width out of range (0..255)");
    return _Setw{static_cast<std::uint8_t>(n)};
}

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

template <typename TChar, typename TMoney>
    requires ((std::integral<TMoney> && !std::same_as<TMoney, bool>)
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
    requires ((std::integral<TMoney> && !std::same_as<TMoney, bool>)
              || std::same_as<TMoney, std::basic_string<TChar>>)
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

template<typename _CharT> struct _Put_time { const std::tm* tmb; const _CharT* fmt; };
/**
 * @lang{ZH}
 * @brief 构造按 @p fmt 写出 `*tmb` 的操纵符。
 * @param tmb 要写出的时间；不得为空。
 * @param fmt `strftime` 风格的格式串；不得为空。
 * @note 两个指针都会在写出前校验，为空时置流的失败位而非解引用；详见
 *       `writer<TChar, _Put_time<TChar>>::swrite`。
 * @note `put_time` 既不应用也不消耗 `io.width()`：填充与随后的 `width(0)` 由各自的 facet
 *       负责，而 `timeio` 的写出路径完全不涉及 width。因此 `os << setw(20) << put_time(...)`
 *       不会补齐到 20 列，且这个 width 会原样留给下一次插入。这与 `std::put_time` 的行为
 *       一致（`time_put` 同样不处理 width），但与 `put_money`、算术类型的插入不同——后两者
 *       经由 `monetary` / `numeric` facet，会消费掉 width。
 * @warning 返回的对象**持有这两个裸指针**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that writes `*tmb` according to @p fmt.
 * @param tmb The time to write; must not be null.
 * @param fmt A `strftime`-style format string; must not be null.
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
     * @param f 待写出的时间与格式串；`f.tmb` 与 `f.fmt` 均不得为空。
     * @return 指向最后一个写入位置之后的输出迭代器。
     * @throw stream_error 若 `f.tmb` 或 `f.fmt` 为空指针，或缺少 timeio facet。
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
     * @param f The time and format string to write; neither `f.tmb` nor `f.fmt` may be null.
     * @return An output iterator past the last written position.
     * @throw stream_error If `f.tmb` or `f.fmt` is a null pointer, or the timeio facet is
     *        missing.
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

template<typename _CharT> struct _Get_time { std::tm* tmb; const _CharT* fmt; };
/**
 * @lang{ZH}
 * @brief 构造按 @p fmt 解析时间并写入 `*tmb` 的操纵符。
 * @param tmb 接收解析结果的 `tm`；不得为空。
 * @param fmt `strptime` 风格的格式串；不得为空。
 * @note 两个指针都会在解析前校验，为空时置流的失败位而非解引用；详见
 *       `reader<TChar, _Get_time<TChar>>::sread`。
 * @note 格式串中未出现的字段保留 `*tmb` 原有的取值：解析上下文由 `*tmb` 铺好回退值，故
 *       `%H:%M` 这样只解析时间的格式串不会动到日期。`tm_wday` / `tm_yday` 总是由最终日期
 *       重新推算，`tm_isdst` 总是置为 -1——没有格式符携带夏令时信息，而沿用调用方的旧值会
 *       在日期被改写后变成错的；-1 表示"未知，交由 C 库判定"。
 * @note `*tmb` 中越界的字段在用作回退值前会先归一化（`tm_mday == 0` 取上月最后一天、
 *       `tm_mon == 12` 进位到次年 1 月等，规则见 `io/fp_defs/tm.h`）；沿用下来的日若在
 *       解析出的月份里不存在，则取该月最后一天，而不是让整次提取失败。因此
 *       `std::tm t{}`（`tm_mday` 为 0）配上只解析时间的格式串，得到的日期与
 *       `std::get_time` 之后再调用 `mktime()` 一致。
 * @warning 返回的对象**持有这两个裸指针**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that parses a time according to @p fmt into `*tmb`.
 * @param tmb The `tm` receiving the parsed result; must not be null.
 * @param fmt A `strptime`-style format string; must not be null.
 * @note Both pointers are validated before parsing, and a null one sets a failure bit on the
 *       stream rather than being dereferenced; see `reader<TChar, _Get_time<TChar>>::sread`.
 * @note Fields absent from the format string keep the value they had in `*tmb`: the parse
 *       context is seeded with fallbacks from `*tmb`, so a time-only format string such as
 *       `%H:%M` does not disturb the date. `tm_wday` / `tm_yday` are always recomputed from the
 *       resulting date, and `tm_isdst` is always set to -1 -- no format specifier carries DST
 *       information, and carrying the caller's old value over would be wrong once the date has
 *       been rewritten; -1 means "unknown, let the C library work it out".
 * @note Out-of-range fields of `*tmb` are normalized before they are used as fallbacks
 *       (`tm_mday == 0` is the last day of the previous month, `tm_mon == 12` carries into
 *       January of the next year, and so on; see `io/fp_defs/tm.h` for the rules), and a day
 *       carried over this way that does not exist in the parsed month becomes the last day of
 *       that month rather than failing the extraction. A `std::tm t{}` (whose `tm_mday` is 0)
 *       with a time-only format string therefore yields the same date as `std::get_time`
 *       followed by `mktime()`.
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
     * @param f 用于接收解析结果的 `tm` 与格式串；`f.tmb` 与 `f.fmt` 均不得为空。
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
     * @param f The `tm` receiving the parsed result and the format string; neither `f.tmb`
     *          nor `f.fmt` may be null.
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
    // Explicit template arguments for the same reason as in the _Get_money overload:
    // `is >> f` would recurse into this very function.
    return IOv2::operator>><T, _Get_time<TChar>>(is, f);
}
}