/**
 * @file io_manip.h
 * @lang{ZH}
 * 定义了带参数的流操纵符：`resetiosflags` / `setiosflags` / `setbase` / `setfill` /
 * `setprecision` / `setw`（调整格式化状态），以及 `put_money` / `get_money` /
 * `put_time` / `get_time`（按 locale 格式化或解析货币与时间）。
 *
 * 每个操纵符由一个工厂和一个 `xxx_t` 载体类型构成，后者是**实现细节**、不应被直接构造；
 * 工厂是普通函数（如 `setw`）或函数模板（如 `setfill`），`put_money` / `get_money` 例外——
 * 它们是**函数对象**，为的是关掉 ADL，理由见 `put_money`；
 * 全都经由 `io_traits` 特化生效。载体只携带参数、不带 `operator()`：调整格式状态的那几个把
 * 逻辑写在**流形式**的 `swrite` / `sread` 里，`os << m` / `is >> m` 是仅有的入口（标准里
 * `setw(5)(os)` 那样的直接调用形式不存在）；货币与时间那几个提供**迭代器形式**，由通用的格式化
 * 插入/提取运算符负责加锁与哨兵。方向由**哪个成员存在**表达：只有 `swrite` 即只能插入，
 * 只有 `sread` 即只能提取，用反了那一侧的运算符不满足约束、没有可行重载，因此编译不过。
 *
 * @warning **操纵符与随后的 I/O 不构成同一个临界区。** `os << setw(5) << value` 里，
 *          `setw(5)` 走流形式，插入运算符**不为它加锁**——它只是对 `width` 做一次原子写；
 *          `<< value` 走迭代器形式，在自己的临界区里读取 `width` 并在用完后清零。两者之间
 *          没有任何互斥，另一个线程的 `setw` 可以落在中间，双方互相吃掉对方的宽度。这与
 *          标准库的行为一致，但本库在其余地方对"单次操作原子"的保证更强，容易让人误以为
 *          这里也是。需要整体原子时，用 `IOv2::sync` 把它们圈进同一个临界区。
 * @warning **往流形式的 `swrite` / `sread` 里放读-改-写逻辑，必须自己取 `io_mutex()`。**
 *          本文件这几个操纵符不取锁，只因为它们各自都是对 `ios_base` 上某个原子成员的单次
 *          读-改-写（`exchange` / `fetch_or` / CAS 循环之一）。像"先读 `flags()`
 *          再按结果 `setf()`"这样跨两次调用的逻辑不在此列，写在流形式里就是竞态；范例见
 *          `endl` 与 `ws`，两者都自己取锁并自己 `catch`（理由见 `traits/traits_base.h`）。
 * @warning **工厂函数返回的是临时对象，只应作为同一个完整表达式的一部分立即使用。**
 *          `put_money_t` / `get_money_t` 持有对实参的引用，`put_time_t` / `get_time_t` 持有
 *          裸指针。写成 `os << put_money(x)` 是安全的——临时量活到完整表达式结束；但
 *          `auto m = put_money(compute()); os << m;` 会悬垂，因为 `compute()` 的临时结果
 *          在第一条语句结束时就已销毁。此契约与 `std::put_money` 等同。
 * @note **本头文件只带来操纵符本身，不带来"往流里写一个数或一个字符串"的能力。** 它不包含
 *       `io/traits/arithmetic.h` 与 `io/traits/char_and_str.h`，因此只 `#include
 *       <io/io_manip.h>` 时 `os << 42`、`os << "abc"` 都编译不过，而 `os << setw(8)` 可以。
 *       这是按值类型选配的一贯做法（见 `io/traits/traits_base.h`）：要写哪类值就包哪个
 *       traits 头，本文件不替你决定。库内的测试文件全都显式补上这两个头。
 * @endif
 *
 * @lang{EN}
 * Defines the parameterized stream manipulators: `resetiosflags`, `setiosflags`, `setbase`,
 * `setfill`, `setprecision` and `setw` (which adjust formatting state), plus `put_money`,
 * `get_money`, `put_time` and `get_time` (which format or parse money and time under the
 * locale).
 *
 * Each manipulator consists of a factory and an `xxx_t` carrier type; the latter is an
 * **implementation detail** and should never be constructed directly. A factory is either a plain
 * function (`setw`) or a function template (`setfill`); `put_money` and `get_money` are the
 * exception -- they are **function objects**, in order to turn ADL off, for the reason given on
 * `put_money`. All of them take effect through an `io_traits` specialization. A carrier holds the
 * parameters only and has no `operator()`: the formatting-state ones keep their logic in the
 * **stream form** of `swrite` / `sread`, so `os << m` / `is >> m` is the only entry point (the
 * standard's direct-call form `setw(5)(os)` does not exist here); the money and time ones provide
 * the **iterator form**, and the generic formatted insertion/extraction operators own the lock and
 * the sentry. The direction is expressed by **which member exists**: `swrite` only means insertion
 * only, `sread` only means extraction only, and using one backwards leaves the operator on that
 * side unsatisfied, so there is no viable overload and it does not compile.
 *
 * @warning **A manipulator and the I/O that follows it are not one critical section.** In
 *          `os << setw(5) << value`, `setw(5)` goes through the stream form and the insertion
 *          operator **does not lock for it** -- it is just one atomic write to `width`;
 *          `<< value` goes through the iterator form and reads `width` in its own critical
 *          section, resetting it to zero once used. Nothing mutually excludes the two, so
 *          another thread's `setw` can land in between and the two steal each other's width.
 *          This matches the standard library, but this library's stronger "a single operation
 *          is atomic" guarantee elsewhere makes it easy to assume otherwise. To make a group
 *          atomic, wrap it in one critical section with `IOv2::sync`.
 * @warning **Read-modify-write logic in a stream-form `swrite` / `sread` must take
 *          `io_mutex()` itself.** The manipulators in this file get away without a lock only
 *          because each is a single read-modify-write on one atomic member of `ios_base`
 *          (one of `exchange`, `fetch_or`, a CAS loop). Logic spanning two calls -- say
 *          reading `flags()` and then calling `setf()` on the result -- is not, and is a race
 *          if written in the stream form; `endl` and `ws` are the examples to follow, taking
 *          the lock and catching for themselves (see `traits/traits_base.h` for why).
 * @warning **A factory returns a temporary, to be used only as part of the same full
 *          expression.** `put_money_t` / `get_money_t` hold a reference to the argument, and
 *          `put_time_t` / `get_time_t` hold raw pointers. `os << put_money(x)` is safe -- the
 *          temporary lives to the end of the full expression -- but
 *          `auto m = put_money(compute()); os << m;` dangles, because `compute()`'s temporary
 *          result is already destroyed at the end of the first statement. This contract is the
 *          same as `std::put_money`'s.
 * @note **This header brings in the manipulators, not the ability to write a number or a string
 *       to a stream.** It does not include `io/traits/arithmetic.h` or
 *       `io/traits/char_and_str.h`, so with `#include <io/io_manip.h>` alone `os << 42` and
 *       `os << "abc"` do not compile while `os << setw(8)` does. That is the library's usual
 *       opt-in-per-value-type arrangement (see `io/traits/traits_base.h`): include the traits
 *       header for the kind of value you mean to write; this file does not choose for you. Every
 *       test file in the library adds those two headers explicitly.
 * @endif
 */
#pragma once

#include <common/defs.h>
#include <common/metafunctions.h>
#include <common/streambuf_defs.h>
#include <facet/monetary.h>
#include <facet/timeio.h>
#include <io/io_base.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/tm.h>
#include <io/traits/traits_base.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>
#include <locale/locale.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <string>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief `resetiosflags` 操纵符的类型。
 *
 * 本操纵符只改格式标志，两个方向都合法，故 `io_traits<TChar, resetiosflags_t>` 同时提供
 * `swrite` 与 `sread`。本类型只携带参数，不带 `operator()`：逻辑全在扩展点里——一行写得下的就
 * 两个成员各写一遍，写不下的走一个私有静态辅助函数。这样 `os << m` / `is >> m` 是仅有的入口，
 * 异常统一由运算符转交 `handle_exception`，标准里 `setbase(8)(os)` 那样的直接调用形式不存在。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `resetiosflags` manipulator.
 *
 * It only changes format flags and is legal in both directions, so
 * `io_traits<TChar, resetiosflags_t>` provides both `swrite` and `sread`. This type carries the
 * parameter only and has no `operator()`: all the logic lives in the extension point -- spelled
 * out in both members when it fits on one line, and factored into a private static helper when it
 * does not. That leaves `os << m` / `is >> m` as the only entries, with exceptions handed to
 * `handle_exception` by the operator, and no direct-call form such as `setbase(8)(os)`.
 * @endif
 */
struct resetiosflags_t
{
    explicit resetiosflags_t(ios_defs::fmtflags mask) : m_mask(mask) {}

    ios_defs::fmtflags m_mask;
};

/**
 * @lang{ZH} @brief 构造清除 @p mask 中各标志的操纵符（其余标志不受影响）。 @endif
 * @lang{EN} @brief Builds the manipulator that clears the flags in @p mask, leaving the rest untouched. @endif
 */
inline resetiosflags_t resetiosflags(ios_defs::fmtflags mask) { return resetiosflags_t{mask}; }

/**
 * @lang{ZH}
 * @brief `resetiosflags` 的扩展点特化：两个方向都提供，都清除 `m_mask` 中的各标志，其余不受
 *        影响。
 *
 * 操纵符的方向由**成员的有无**表达：两个成员都在，于是 `os << resetiosflags(f)` 与
 * `is >> resetiosflags(f)` 都成立。改用约束是不够的——`iostream` 同时满足两个流概念，单向操纵符
 * 若只靠约束就挡不住反向用法，故方向一律落在这里。
 * @note 流类型不匹配时，成员在**声明**层面（`ostream_type` / `istream_type`）就不可行，而不是
 *       在函数体里硬报错——后者会让 `requires { os << x; }` 探测不出来。代价是诊断为通用的
 *       "没有匹配的 `operator<<` / `operator>>`"，不会点明原因。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `resetiosflags`: both directions are provided, and
 *        both clear the flags in `m_mask`, leaving the rest untouched.
 *
 * A manipulator's direction is expressed by **which member exists**: both are here, so
 * `os << resetiosflags(f)` and `is >> resetiosflags(f)` both work. A constraint would not be
 * enough -- `iostream` satisfies both stream concepts, so constraints alone could not stop a
 * one-way manipulator from being used backwards; direction therefore always lives here.
 * @note On a stream-type mismatch the members are non-viable at the **declaration** level
 *       (`ostream_type` / `istream_type`) rather than erroring inside the body, which would make
 *       `requires { os << x; }` unprobeable. The price is that the diagnostic is the generic
 *       "no match for `operator<<` / `operator>>`" and does not say why.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, resetiosflags_t>
{
    template <ostream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void swrite(T& s, const resetiosflags_t& f) { s.setf(ios_defs::fmtflags(0), f.m_mask); }

    template <istream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void sread(T& s, const resetiosflags_t& f) { s.setf(ios_defs::fmtflags(0), f.m_mask); }
};

/**
 * @lang{ZH}
 * @brief `setiosflags` 操纵符的类型。两个方向都合法；方向如何表达见
 *        `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setiosflags` manipulator. Legal in both directions; see
 *        `io_traits<TChar, resetiosflags_t>` for how the direction is expressed.
 * @endif
 */
struct setiosflags_t
{
    explicit setiosflags_t(ios_defs::fmtflags mask) : m_mask(mask) {}

    ios_defs::fmtflags m_mask;
};

/**
 * @lang{ZH} @brief 构造置位 @p mask 中各标志的操纵符（按位或，其余标志不受影响）。 @endif
 * @lang{EN} @brief Builds the manipulator that sets the flags in @p mask (bitwise-or; the rest are untouched). @endif
 */
inline setiosflags_t setiosflags(ios_defs::fmtflags mask) { return setiosflags_t{mask}; }

/**
 * @lang{ZH}
 * @brief `setiosflags` 的扩展点特化：两个方向都提供，都置位 `m_mask` 中的各标志（按位或），
 *        其余不受影响。见 `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `setiosflags`: both directions, each setting the
 *        flags in `m_mask` (bitwise-or) and leaving the rest untouched. See
 *        `io_traits<TChar, resetiosflags_t>`.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, setiosflags_t>
{
    template <ostream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void swrite(T& s, const setiosflags_t& f) { s.setf(f.m_mask); }

    template <istream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void sread(T& s, const setiosflags_t& f) { s.setf(f.m_mask); }
};

/**
 * @lang{ZH}
 * @brief `setbase` 操纵符的类型。两个方向都合法；方向如何表达见
 *        `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setbase` manipulator. Legal in both directions; see
 *        `io_traits<TChar, resetiosflags_t>` for how the direction is expressed.
 * @endif
 */
struct setbase_t
{
    explicit setbase_t(int base) : m_base(base) {}

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
inline setbase_t setbase(int base) { return setbase_t{base}; }

/**
 * @lang{ZH}
 * @brief `setbase` 的扩展点特化：两个方向都提供，都转调私有的 `apply`——按 `m_base` 设置
 *        `basefield`，取值的含义见 `setbase`。见 `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `setbase`: both directions, each forwarding to the
 *        private `apply`, which sets `basefield` from `m_base`; see `setbase` for what the values
 *        mean, and `io_traits<TChar, resetiosflags_t>` for the direction.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, setbase_t>
{
    template <ostream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void swrite(T& s, const setbase_t& f) { apply(s, f); }

    template <istream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void sread(T& s, const setbase_t& f) { apply(s, f); }

private:
    template <typename T>
    static void apply(T& s, const setbase_t& f)
    {
        s.setf(f.m_base ==  8 ? ios_defs::oct :
               f.m_base == 10 ? ios_defs::dec :
               f.m_base == 16 ? ios_defs::hex :
               ios_defs::fmtflags(0), ios_defs::basefield);
    }
};

/**
 * @lang{ZH}
 * @brief `setfill` 操纵符的类型。两个方向都合法；方向如何表达见
 *        `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setfill` manipulator. Legal in both directions; see
 *        `io_traits<TChar, resetiosflags_t>` for how the direction is expressed.
 * @endif
 */
template<typename TFill>
struct setfill_t
{
    explicit setfill_t(TFill c) : m_c(c) {}

    TFill m_c;
};

/**
 * @lang{ZH}
 * @brief 构造设置填充字符的操纵符；填充字符用于字段宽度大于内容时补齐。
 * @warning **填充不得改变这段文本读起来是几。** 格式化数值与货币时，若这次补齐要写出的
 *          字符会让读者读出另一个数，插入侧直接拒绝，置 `strfailbit`（仅当该位在异常掩码
 *          中时才抛 `stream_error`）。判据同时看字符与落点：
 *          - `'1'`–`'9'` 处处不可用；`'0'` 仅当补在数字正前方时可用，故
 *            `setfill('0') << setw(8) << internal << -42` 得到 `"-0000042"`，而同样的
 *            `right`（`"00000-42"`）与 `left`（`"42000000"`）都会失败；十六进制补零同理，
 *            只有 `internal` 把零放进 `0x` 与数字之间。
 *          - 小数点仅当补在数值之后时可用（`"42......"` 可以，`"......42"` 不行）。
 *          - 正负号仅当与该数值自身的符号一致时可用（`"++++42"` 可以，`"----42"` 不行），
 *            补在数值之后一律可用。
 *          - 其余字符（`'*'`、`' '`、千位分隔符、字母等）不受影响。
 *          `fill` 是粘性流状态，只在**真的要补齐**的那一次判断：宽度不大于内容时不补齐，
 *          也就不会失败。提取 `get_money` 时用同一判据把关，见 `get_money`。
 * @note `TFill` 由实参推导，且必须与目标流的 `char_type` **完全一致**——
 *       `io_traits<TChar, setfill_t<TFill>>` 的两个成员都要求二者 `same_as`，不存在隐式转换。
 *       因此对 `wchar_t` 流须写 `setfill(L'*')`，写成 `setfill('*')` 会编译失败而非静默转换。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the fill character, used to pad when the field width
 *        exceeds the content.
 * @warning **Padding must not change what number the text says.** When formatting a numeric or
 *          monetary value, a fill character that would make a reader read a different number is
 *          rejected outright: the stream sets `strfailbit` (and throws `stream_error` only when
 *          that bit is in the exception mask). The test looks at both the character and where it
 *          lands:
 *          - `'1'`–`'9'` are never usable; `'0'` is usable only directly in front of the digits,
 *            so `setfill('0') << setw(8) << internal << -42` gives `"-0000042"` while the same
 *            with `right` (`"00000-42"`) or `left` (`"42000000"`) fails. Zero-padded hex behaves
 *            the same way: only `internal` puts the zeros between the `0x` and the digits.
 *          - The decimal point is usable only after the value (`"42......"` yes,
 *            `"......42"` no).
 *          - A sign is usable only where it agrees with the value's own sign (`"++++42"` yes,
 *            `"----42"` no), and always usable after the value.
 *          - Every other character (`'*'`, `' '`, the thousands separator, letters, …) is
 *            unaffected.
 *          `fill` is sticky stream state and is only tested where padding is actually written:
 *          a width that does not exceed the content pads nothing and so cannot fail. `get_money`
 *          applies the same test on extraction; see `get_money`.
 * @note `TFill` is deduced from the argument and must match the target stream's `char_type`
 *       **exactly**: both members of `io_traits<TChar, setfill_t<TFill>>` require the two to be
 *       `same_as`, so no implicit conversion applies. A `wchar_t` stream therefore needs
 *       `setfill(L'*')`; `setfill('*')` fails to compile rather than converting silently.
 * @endif
 */
template<typename TFill>
inline setfill_t<TFill> setfill(TFill c) { return setfill_t<TFill>{c}; }

/**
 * @lang{ZH}
 * @brief `setfill` 的扩展点特化：两个方向都提供，都设置流的填充字符。见
 *        `io_traits<TChar, resetiosflags_t>`。
 * @note 两个成员都带 `same_as` 约束，要求 `TFill` 与流的 `char_type` **完全一致**，不接受
 *       隐式转换；理由见 `setfill`。约束写在这里而不是函数体里，字符类型不匹配的 `setfill`
 *       才能在**声明**层面就不可行；代价是诊断为通用的"没有匹配的 `operator<<`"，不会点名
 *       填充字符。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `setfill`: both directions, each setting the stream's
 *        fill character. See `io_traits<TChar, resetiosflags_t>`.
 * @note Both members carry a `same_as` constraint requiring `TFill` to match the stream's
 *       `char_type` **exactly**, with no implicit conversion; see `setfill` for why. Keeping it in
 *       the constraint rather than the body is what makes a mismatched `setfill` non-viable at the
 *       **declaration** level; the price is that the diagnostic is the generic "no match for
 *       `operator<<`" and does not name the fill character.
 * @endif
 */
template <typename TChar, typename TFill>
struct io_traits<TChar, setfill_t<TFill>>
{
    template <ostream_type T>
        requires (std::same_as<typename T::char_type, TFill> &&
                  std::same_as<typename T::char_type, TChar>)
    static void swrite(T& s, const setfill_t<TFill>& f) { s.fill(f.m_c); }

    template <istream_type T>
        requires (std::same_as<typename T::char_type, TFill> &&
                  std::same_as<typename T::char_type, TChar>)
    static void sread(T& s, const setfill_t<TFill>& f) { s.fill(f.m_c); }
};

/**
 * @lang{ZH}
 * @brief `setprecision` 操纵符的类型。两个方向都合法；方向如何表达见
 *        `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setprecision` manipulator. Legal in both directions; see
 *        `io_traits<TChar, resetiosflags_t>` for how the direction is expressed.
 * @endif
 */
struct setprecision_t
{
    explicit setprecision_t(std::ptrdiff_t n) : m_n(n) {}

    std::ptrdiff_t m_n;
};

/**
 * @lang{ZH}
 * @brief 构造设置浮点精度的操纵符。
 *
 * 精度以 `std::uint8_t` 存储（见 `ios_base::precision`），有效范围 0..255。参数取
 * `std::ptrdiff_t` 而非 `std::uint8_t`，是为了让越界值被**显式拒绝**而不是被静默窄化：
 * 若形参本身就是 `std::uint8_t`，`setprecision(300)` 会经隐式转换悄悄变成 44，
 * 而运行期变量连编译警告都不会有。
 * @note 范围检查是 `ios_base::precision` 自己的，在操纵符**作用于流时**才做，本函数只保存
 *       原值。届时抛出的 `stream_error` 由 `operator<<` / `operator>>` 交给 `handle_exception`，
 *       转成 `strfailbit` 并遵守流的异常掩码，与本库其余的失败一致；若改在构造时抛，异常会从
 *       `os << ...` 表达式里直接逃逸到调用方，流却仍报告 `good()`。
 * @param n 目标精度，必须落在 0..255。
 * @return 可用于 `os << setprecision(n)` / `is >> setprecision(n)` 的操纵符。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that sets the floating-point precision.
 *
 * The precision is stored as a `std::uint8_t` (see `ios_base::precision`), so the valid
 * range is 0..255. The parameter is a `std::ptrdiff_t` rather than a `std::uint8_t` so that an
 * out-of-range value is **rejected explicitly** instead of silently wrapping: with a
 * `std::uint8_t` parameter, `setprecision(300)` would quietly become 44 via the implicit
 * conversion, and a run-time argument would not even produce a compiler warning.
 * @note The range check is `ios_base::precision`'s own and runs when the manipulator is **applied
 *       to a stream**; this function only stores the value. The `stream_error` thrown there is
 *       handed to `handle_exception` by `operator<<` / `operator>>`, where it becomes a
 *       `strfailbit` and honors the stream's exception mask, like every other failure in this
 *       library. Thrown at construction it would instead escape an `os << ...` expression
 *       straight to the caller while the stream still reported `good()`.
 * @param n The target precision; must lie in 0..255.
 * @return A manipulator usable as `os << setprecision(n)` / `is >> setprecision(n)`.
 * @endif
 */
inline setprecision_t setprecision(std::ptrdiff_t n) { return setprecision_t{n}; }

/**
 * @lang{ZH}
 * @brief `setprecision` 的扩展点特化：两个方向都提供。见 `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `setprecision`: both directions. See
 *        `io_traits<TChar, resetiosflags_t>`.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, setprecision_t>
{
    template <ostream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void swrite(T& s, const setprecision_t& f) { apply(s, f); }

    template <istream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void sread(T& s, const setprecision_t& f) { apply(s, f); }

private:
    template <typename T>
    static void apply(T& s, const setprecision_t& f)
    {
        s.precision(f.m_n);
    }
};

/**
 * @lang{ZH}
 * @brief `setw` 操纵符的类型。两个方向都合法；方向如何表达见
 *        `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `setw` manipulator. Legal in both directions; see
 *        `io_traits<TChar, resetiosflags_t>` for how the direction is expressed.
 * @endif
 */
struct setw_t
{
    explicit setw_t(std::ptrdiff_t n) : m_n(n) {}

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
 *       时的窄化把回绕抵消回来、还原成负数并被拒。
 * @note 判负由 `ios_base::width` 自己完成，在操纵符**作用于流时**才做，本函数只保存原值。
 *       届时抛出的 `stream_error` 由 `operator<<` / `operator>>` 交给 `handle_exception`，
 *       转成 `strfailbit` 并遵守流的异常掩码，与本库其余的失败一致；若在构造时抛，异常会从
 *       `os << ...` 表达式里直接逃逸到调用方，流却仍报告 `good()`。
 * @note **提取端的长度安全不依赖本函数。** 目标缓冲区的上界始终来自类型本身：
 *       `io_traits<TChar, TChar[N]>` 以 `min(width, N)` 为界，`std::basic_string` 自动增长，
 *       而裸指针根本没有对应的 `sread`（`is >> ptr` 无法编译，与 C++20 起的 `std::istream`
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
 *       that wrap, restoring the negative value to be rejected.
 * @note The sign check is `ios_base::width`'s own and runs when the manipulator is **applied to
 *       a stream**; this function only stores the value. The `stream_error` thrown there is
 *       handed to `handle_exception` by `operator<<` / `operator>>`, where it becomes a
 *       `strfailbit` and honors the stream's exception mask, like every other failure in this
 *       library. Thrown at construction it would instead escape an `os << ...` expression
 *       straight to the caller while the stream still reported `good()`.
 * @note **Length safety on the extraction side does not depend on this function.** The bound
 *       on a destination buffer always comes from its type: `io_traits<TChar, TChar[N]>` bounds
 *       at `min(width, N)`, `std::basic_string` grows on demand, and a raw pointer has no
 *       `sread` at all (`is >> ptr` does not compile, matching `std::istream` as of C++20).
 *       `width` can therefore only **tighten** a bound, never cause an out-of-bounds write.
 * @note Matching the standard, `width` is consumed only by character-array and
 *       `std::basic_string` extraction; it survives arithmetic extraction, `get_money` and
 *       `get_time`.
 * @param n The target width; must not be negative.
 * @return A manipulator usable as `os << setw(n)` / `is >> setw(n)`.
 * @endif
 */
inline setw_t setw(std::ptrdiff_t n) { return setw_t{n}; }

/**
 * @lang{ZH}
 * @brief `setw` 的扩展点特化：两个方向都提供。见 `io_traits<TChar, resetiosflags_t>`。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `setw`: both directions. See
 *        `io_traits<TChar, resetiosflags_t>`.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, setw_t>
{
    template <ostream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void swrite(T& s, const setw_t& f) { apply(s, f); }

    template <istream_type T>
        requires std::same_as<typename T::char_type, TChar>
    static void sread(T& s, const setw_t& f) { apply(s, f); }

private:
    template <typename T>
    static void apply(T& s, const setw_t& f)
    {
        s.width(f.m_n);
    }
};

template<typename TMoney> struct put_money_t { const TMoney& m_mon; bool m_intl; };
/**
 * @lang{ZH}
 * @brief 构造按 locale 的货币格式写出 @p mon 的操纵符。
 * @param mon 货币值。可为**除 `bool` 外**的整型（以最小货币单位计，如"分"），或已是
 *            `char_type` 数字串的 `std::basic_string<char_type>`（**须为默认的 `char_traits`
 *            与默认分配器**）。**注意本库不接受浮点**，这一点与接受 `long double` 的
 *            `std::put_money` 不同（约束来自 `monetary::put`）。
 *            排除 `bool` 与 `monetary::put` 的约束一致；要求默认 traits/分配器则比本库的
 *            字符串插入运算符更严——`os << str` 能写的串，`os << put_money(str)` 未必能写。
 *            以上三类实参都会让重载不可行，诊断是通用的"没有匹配的 `operator<<`"，看不出
 *            具体原因，故在此列全。
 * @param intl `true` 用国际格式（如 `USD`），`false` 用本地格式（如 `$`）。
 * @warning 返回的对象**持有对 @p mon 的引用**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that writes @p mon in the locale's monetary format.
 * @param mon The monetary value: either an integral type **other than `bool`**, counted in the
 *            smallest currency unit (cents, say), or a `std::basic_string<char_type>` of digits
 *            (**with the default `char_traits` and the default allocator**). **Floating-point is
 *            not accepted here**, unlike `std::put_money` which takes a `long double`; the
 *            constraint comes from `monetary::put`.
 *            Excluding `bool` matches `monetary::put`; requiring the default traits and
 *            allocator is stricter than this library's own string insertion — a string
 *            `os << str` accepts is not necessarily one `os << put_money(str)` accepts.
 *            All three make the overload non-viable, and the diagnostic is the generic "no
 *            matching `operator<<`", which does not say why, hence the full list here.
 * @param intl `true` selects the international format (e.g. `USD`), `false` the national one
 *             (e.g. `$`).
 * @warning The returned object **holds a reference to @p mon** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @endif
 *
 * @lang{ZH}
 * @note **本工厂是个函数对象，不是函数模板**，这样写是为了关掉 ADL。`std::basic_string` 的
 *       关联命名空间是 `std`，若本工厂是函数模板，`using namespace IOv2;` 之后非限定调用
 *       `put_money(str)` 会被 ADL 拖进 `std::put_money` 而二义——而 `<iomanip>` 是本头文件
 *       传递性引入的，用户躲不掉。按 [basic.lookup.argdep]/1，非限定查找找到的若不是函数、
 *       也不是函数模板，就不执行 ADL，故函数对象没有这个问题。类型具名（而非匿名 struct）
 *       同样是必需的：匿名类无链接，会让这个 `inline` 变量在每个 TU 里各有一份。
 * @note 代价是不能再写显式模板实参 `put_money<int>(x)`（`std::put_money` 可以）。
 * @endif
 *
 * @lang{EN}
 * @note **This factory is a function object rather than a function template**, so as to turn
 *       ADL off. The associated namespace of a `std::basic_string` is `std`, so with a function
 *       template an unqualified `put_money(str)` after `using namespace IOv2;` would drag in
 *       `std::put_money` and be ambiguous -- and `<iomanip>` arrives transitively through this
 *       very header, so a user cannot avoid it. Per [basic.lookup.argdep]/1, ADL is not
 *       performed when unqualified lookup finds something that is neither a function nor a
 *       function template, which a function object is not. Naming the type (rather than using an
 *       anonymous struct) is equally necessary: an anonymous class has no linkage, which would
 *       give this `inline` variable a separate copy in every TU.
 * @note The cost is that explicit template arguments, `put_money<int>(x)`, are no longer
 *       available (`std::put_money` allows them).
 * @endif
 */
inline constexpr struct put_money_fn
{
    template <typename TMoney>
    put_money_t<TMoney> operator()(const TMoney& mon, bool intl = false) const
    { return { mon, intl }; }
} put_money{};

// remove_cv_t on the bool exclusion only: put() takes the integral by value, so cv is dropped
// there, but its string overload takes a plain const&, which a volatile string cannot bind to.
template <typename TChar, typename TMoney>
    requires ((std::integral<TMoney> && !std::same_as<std::remove_cv_t<TMoney>, bool>)
              || std::same_as<TMoney, std::basic_string<TChar>>)
struct io_traits<TChar, put_money_t<TMoney>>
{
    /**
     * @lang{ZH}
     * @brief 按 locale 的货币格式将 `f.m_mon` 写出。
     *
     * @note **缺少 `monetary` facet 不是编译期错误，而是运行期失败。** 这个特化只按 `TMoney`
     *       约束（整型且非 `bool`，或 `std::basic_string<TChar>`），流的 locale 里有没有
     *       `monetary<TChar>` 要到这里才知道。取不到就抛 `stream_error`，由 `operator<<` 接住、
     *       经 `handle_exception` 归为 `strfailbit`：什么都不写出，此后该流上的插入一律被哨兵
     *       挡下，直到显式 `clear()`。默认构造的 `locale<TChar>` 是带 `monetary` 的，这条主要
     *       出现在自行拼装 facet 的 locale 上。
     * @param f 待写出的货币值与 `intl` 标志；取值域见 `put_money`。
     * @return 指向最后一个写入位置之后的输出迭代器。
     * @throw stream_error 若 locale 中缺少 `monetary<TChar>` facet，或 `monetary::put` 自身失败
     *        （填充字符会改变这段文本读起来是几，见 `setfill`；或填充数超过
     *        `ios_defs::max_pad_count`）。
     * @endif
     *
     * @lang{EN}
     * @brief Writes `f.m_mon` in the locale's monetary format.
     *
     * @note **A missing `monetary` facet is a run-time failure, not a compile-time one.** This
     *       specialization is constrained on `TMoney` only (an integral other than `bool`, or a
     *       `std::basic_string<TChar>`); whether the stream's locale holds a `monetary<TChar>` is
     *       only known here. If it does not, a `stream_error` is thrown, caught by `operator<<`
     *       and categorized as `strfailbit` by `handle_exception`: nothing is written, and every
     *       later insertion on that stream is refused by the sentry until an explicit `clear()`.
     *       A default-constructed `locale<TChar>` does carry `monetary`, so this mainly concerns
     *       locales assembled facet by facet.
     * @param f The monetary value to write and the `intl` flag; for the accepted set see
     *          `put_money`.
     * @return An output iterator past the last written position.
     * @throw stream_error If the locale has no `monetary<TChar>` facet, or `monetary::put` itself
     *        fails (the fill character would change what number the text says, see `setfill`; or
     *        the fill count exceeds `ios_defs::max_pad_count`).
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, put_money_t<TMoney> f)
    {
        auto mp = loc.template get<monetary<TChar>>();
        if (!mp)
            throw stream_error("cannot get monetary facet");

        return mp->put(s, f.m_intl, io, f.m_mon);
    }
};

template<typename TMoney> struct get_money_t { TMoney& m_mon; bool m_intl; };
/**
 * @lang{ZH}
 * @brief 构造按 locale 的货币格式解析并写入 @p mon 的操纵符。
 * @param mon 接收解析结果的对象；可为**除 `bool` 外**的整型（以最小货币单位计），或
 *            `std::basic_string<char_type>`（**须为默认的 `char_traits` 与默认分配器**）。
 *            不接受浮点。取值域与 `put_money` 完全相同，理由与诊断说明见 `put_money`。
 * @param intl `true` 按国际格式解析，`false` 按本地格式解析。
 * @warning 返回的对象**持有对 @p mon 的引用**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @note **解析用流的 `fill()` 认填充**：货币 pattern 里的 `space` / `none` 段消耗的是
 *       `fill()`，而不是标准规定的空白。这样本库写出的带填充货币文本能被原样读回来
 *       （标准的做法只要 `fill` 不是空格就读不回自己刚写出的内容），代价是 `fill` 为默认
 *       空格时，`'\t'` 在 `space` 位置不被接受。
 * @warning 因此 `fill()` 同样受 `setfill` 那条判据约束：一旦真的吃掉了填充，而该填充字符
 *          是读者会当成金额一部分的（读 `"112345"` 时 `setfill('1')` 会把首位吃掉，只剩
 *          12345），本函数**响亮失败**（`strfailbit`），而不是静默给出被削掉的数。判据与
 *          写侧完全一致，见 `setfill`。没吃到填充就不判定——流上挂着任何 `fill` 都不影响
 *          正常解析。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that parses a monetary value in the locale's format into
 *        @p mon.
 * @param mon Receives the parsed result; either an integral type **other than `bool`**, counted
 *            in the smallest currency unit, or a `std::basic_string<char_type>` (**with the
 *            default `char_traits` and the default allocator**). Floating-point is not accepted.
 *            The accepted set is exactly that of `put_money`; see it for the reasons and for
 *            what the diagnostic looks like.
 * @param intl `true` parses the international format, `false` the national one.
 * @warning The returned object **holds a reference to @p mon** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @note **Parsing consumes the stream's `fill()`**: the `space` / `none` parts of the monetary
 *       pattern eat `fill()` rather than the whitespace the standard specifies. That is what
 *       lets padded monetary text written by this library be read straight back (the standard
 *       behavior cannot read back its own output once `fill` is anything but a space); the price
 *       is that with the default space fill a `'\t'` is not accepted at a `space` part.
 * @warning `fill()` is therefore subject to the same test as on the writing side: once a fill
 *          character is actually consumed and it is one a reader would have counted as part of
 *          the amount (reading `"112345"` with `setfill('1')` eats the leading digit and leaves
 *          12345), this function **fails loudly** (`strfailbit`) instead of silently handing back
 *          the truncated number. The test is identical to the writing side; see `setfill`. No
 *          fill consumed means no test — any `fill` on the stream leaves ordinary parsing alone.
 * @endif
 *
 * @lang{ZH}
 * @note 与 `put_money` 同理，本工厂是个**函数对象**而非函数模板，理由（关掉 ADL、以及类型为何
 *       必须具名）见 `put_money`。
 * @endif
 *
 * @lang{EN}
 * @note As with `put_money`, this factory is a **function object** rather than a function
 *       template; see `put_money` for why (turning ADL off, and why the type must be named).
 * @endif
 */
inline constexpr struct get_money_fn
{
    template <typename TMoney>
    get_money_t<TMoney> operator()(TMoney& mon, bool intl = false) const
    { return { mon, intl }; }
} get_money{};

template <typename TChar, typename TMoney>
    requires (std::same_as<TMoney, std::remove_cv_t<TMoney>>
              && ((std::integral<TMoney> && !std::same_as<TMoney, bool>)
                  || std::same_as<TMoney, std::basic_string<TChar>>))
struct io_traits<TChar, get_money_t<TMoney>>
{
    /**
     * @lang{ZH}
     * @brief 按 locale 的货币格式解析并写入 `f.m_mon`。
     *
     * @note 与 `put_money` 一侧同理，**缺少 `monetary` facet 到这里才发现**：抛 `stream_error`，
     *       由 `operator>>` 接住并归为 `strfailbit`，`f.m_mon` 不被改写。
     * @note 解析消耗的是流的 `fill()` 而不是空白，判据与写侧一致；详见 `get_money`。
     * @param f 接收解析结果的对象与 `intl` 标志；取值域见 `put_money`。
     * @return 指向最后一个已消耗字符之后的输入迭代器。
     * @throw stream_error 若 locale 中缺少 `monetary<TChar>` facet，或 `monetary::get` 解析失败
     *        （格式与 pattern 不符、中途 EOF、或吃掉的填充字符会改变读出来是几，见 `get_money`）。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a monetary value in the locale's format into `f.m_mon`.
     *
     * @note As on the `put_money` side, **a missing `monetary` facet is only discovered here**: a
     *       `stream_error` is thrown, caught by `operator>>` and categorized as `strfailbit`, and
     *       `f.m_mon` is left unmodified.
     * @note Parsing consumes the stream's `fill()` rather than whitespace, under the same test as
     *       the writing side; see `get_money`.
     * @param f The object receiving the result and the `intl` flag; for the accepted set see
     *          `put_money`.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If the locale has no `monetary<TChar>` facet, or `monetary::get` fails
     *        to parse (input not matching the pattern, EOF part-way through, or a consumed fill
     *        character that would change the number read out; see `get_money`).
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc,
                       const get_money_t<TMoney>& f)
    {
        auto mp = loc.template get<monetary<TChar>>();
        if (!mp)
            throw stream_error("cannot get monetary facet");

        return mp->get(s, s_end, f.m_intl, io, f.m_mon);
    }
};

template<typename TChar> struct put_time_t { const std::tm* tmb; const TChar* fmt; };
/**
 * @lang{ZH}
 * @brief 构造按 @p fmt 写出 `*tmb` 的操纵符。
 * @param tmb 要写出的时间；允许为空，见下。
 * @param fmt `strftime` 风格的格式串，但不支持 `%Z` / `%z`（见下）；允许为空，见下。
 * @note 两个指针都会在写出前校验，为空时置流的失败位而非解引用；详见
 *       `io_traits<TChar, put_time_t<TChar>>::swrite`。
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
 * @warning **`%Z` 与 `%z` 不受支持，且不会失败。** `std::tm` 没有可移植的时区字段，按
 *          `timeio` 的既定规则，值提供不了的说明符退化为字面量：这两个说明符被**原样写出**
 *          为 `%Z`、`%z`，流仍保持 `good()`。于是 `os << put_time(&t, "%H:%M %Z")` 得到
 *          `01:02 %Z`，而 `std::put_time` 会写出当前时区的真实名称。这是 `put_time` 唯一一处
 *          不置失败位的分歧，只能靠检查输出发现。退化本身与 `get_time` 对称——这样写出的内容
 *          仍能被同一格式串读回，详见 `timeio` 与 `get_time`。
 * @note **`tm_wday` 与 `tm_yday` 不被读取。** 星期由 y/m/d 重新推算，与调用方填的值无关。
 *       这与 `strftime` 的 `%a`/`%A`（读 `tm_wday`）、`%j`（读 `tm_yday`）不同：若调用方填入
 *       与日期不符的值，标准库按该值输出，本库按真实日期输出，双方都不报错。
 * @note 提取侧的契约**更宽松**：`get_time` 接受 `std::tm t{}`（`tm_mday == 0` 会被归一化成上月
 *       最后一天），`put_time` 不接受。两者刻意不对称，详见 `get_time`。
 * @warning **年份是唯一反过来的一处：这里比 `get_time` 宽。** `%Y` 按 `std::format` 的规矩写：
 *          负年份带 `-`，大于 9999 的年份写四位以上；而 `get_time` 的 `%Y` / `%G` / `%C`
 *          只收 0..9999 且不带符号（与 `std::chrono::from_stream("%Y")`、POSIX `strptime`
 *          一致）。于是 `put_time` 写出的年 10000 或 −44 用同一个格式串读不回来：`get_time`
 *          置 `strfailbit` 且 `*tmb` 完全不变。**只有年 0..9999 才保证往返。** 这是有意的
 *          取舍，见 `timeio`。
 * @warning 返回的对象**持有这两个裸指针**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that writes `*tmb` according to @p fmt.
 * @param tmb The time to write; may be null, see below.
 * @param fmt A `strftime`-style format string, except that `%Z` / `%z` are unsupported (see
 *            below); may be null, see below.
 * @note Both pointers are validated before the write, and a null one sets a failure bit on the
 *       stream rather than being dereferenced; see
 *       `io_traits<TChar, put_time_t<TChar>>::swrite`.
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
 * @warning **`%Z` and `%z` are unsupported, and do not fail.** `std::tm` has no portable
 *          time-zone field, and by `timeio`'s standing rule a specifier the value cannot supply
 *          degrades to a literal: these two are **written out verbatim** as `%Z` and `%z`, with
 *          the stream left `good()`. So `os << put_time(&t, "%H:%M %Z")` yields `01:02 %Z`,
 *          where `std::put_time` writes the real name of the current zone. This is the one
 *          `put_time` divergence that sets no failure bit, so it shows up only on inspecting
 *          the output. The degradation itself is symmetric with `get_time`: what is written
 *          still reads back through the same format string. See `timeio` and `get_time`.
 * @note **`tm_wday` and `tm_yday` are not read.** The weekday is recomputed from y/m/d,
 *       independently of whatever the caller stored. This differs from `strftime`, whose
 *       `%a`/`%A` read `tm_wday` and whose `%j` reads `tm_yday`: given a value inconsistent with
 *       the date, the standard library prints that value while this library prints the one
 *       implied by the date, neither reporting an error.
 * @note The extraction side is **more permissive**: `get_time` accepts a `std::tm t{}` (a
 *       `tm_mday` of 0 is normalized to the last day of the previous month) whereas `put_time`
 *       does not. The asymmetry is deliberate; see `get_time`.
 * @warning **The year is the one place where it runs the other way: this side is the permissive
 *          one.** `%Y` follows `std::format` here -- a leading `-` for a negative year, more than
 *          four digits past 9999 -- while `get_time`'s `%Y` / `%G` / `%C` accept only 0..9999,
 *          unsigned (matching `std::chrono::from_stream("%Y")` and POSIX `strptime`). A year of
 *          10000 or -44 written by `put_time` therefore does not read back through the same
 *          format string: `get_time` sets `strfailbit` and leaves `*tmb` completely unchanged.
 *          **Only years 0..9999 are guaranteed to round-trip.** The trade-off is deliberate; see
 *          `timeio`.
 * @warning The returned object **holds those two raw pointers** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @endif
 */
template<typename TChar>
inline put_time_t<TChar> put_time(const std::tm* tmb, const TChar* fmt) { return { tmb, fmt }; }

template <typename TChar>
struct io_traits<TChar, put_time_t<TChar>>
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
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, put_time_t<TChar> f)
    {
        if (f.tmb == nullptr || f.fmt == nullptr)
            throw stream_error("put_time fail: null tm or format pointer");

        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        return mp->put(s, *(f.tmb), f.fmt);
    }
};

template<typename TChar> struct get_time_t { std::tm* tmb; const TChar* fmt; };
/**
 * @lang{ZH}
 * @brief 构造按 @p fmt 解析时间并写入 `*tmb` 的操纵符。
 * @param tmb 接收解析结果的 `tm`；允许为空，见下。
 * @param fmt `strptime` 风格的格式串，但不支持 `%Z` / `%z`（见下）；允许为空，见下。
 * @note 两个指针都会在解析前校验，为空时置流的失败位而非解引用；详见
 *       `io_traits<TChar, get_time_t<TChar>>::sread`。
 * @note 格式串中未出现的字段保留 `*tmb` 原有的取值：解析上下文由 `*tmb` 铺好回退值，故
 *       `%H:%M` 这样只解析时间的格式串不会动到日期。`tm_wday` / `tm_yday` 总是由最终日期
 *       重新推算，`tm_isdst` 总是置为 -1——没有格式符携带夏令时信息，而沿用调用方的旧值会
 *       在日期被改写后变成错的；-1 表示"未知，交由 C 库判定"。
 * @note 上一条讲的是"格式串没提到的字段"，不是"格式符与 `tm` 成员一一对应"。周号说明符
 *       (`%U`/`%W`/`%V`) 解析出的是整整一周，即便格式串里没有 `%m`/`%d`，月与日也会被移到那
 *       一周里去——回退值只填格式串真正确定不了的字段。只有星期而没有周号时，日期取回退日期
 *       当天或之后第一个匹配的日子，与 `*tmb` 至多相差 6 天；
 *       见 `date_parse_helper::compute_ymd()`。
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
 *       日期，规则见 `io/traits/tm.h`）；沿用下来的日若在
 *       解析出的月份里不存在，则取该月最后一天，而不是让整次提取失败。因此
 *       `std::tm t{}`（`tm_mday` 为 0）配上只解析时间的格式串，得到的日期与
 *       `std::get_time` 之后再调用 `mktime()` 一致。
 * @note 插入侧不对称：`put_time` 要求 `*tmb` 的所有字段都在范围内、且日期组合真实存在，
 *       `std::tm t{}` 在那边会被拒绝并置 `strfailbit`。本函数接受它（见上一条）。详见
 *       `put_time`。
 * @warning **年份上不对称的方向是反的：这里比 `put_time` 严。** `%Y`（以及 `%G`、`%C`）
 *          **只接受 0..9999 且不带符号**，与 `std::chrono::from_stream("%Y")`、POSIX
 *          `strptime` 一致；而 `put_time` 会按 `std::format` 的规矩写出负年份和五位以上的
 *          年份。所以 `put_time` 写出的年 10000 或 −44，本函数读不回来：整次提取失败、置
 *          `strfailbit`，`*tmb` 一字节不改。**只有年 0..9999 才保证往返。** 这是本文档中
 *          "提取侧更宽松"的唯一例外，理由见 `timeio`。
 * @warning **`%Z` 与 `%z` 不受支持。** `std::tm` 没有可移植的时区字段，本函数使用的解析
 *          上下文（`time_parse_context<TChar, true, true, false>`，见 `io/traits/tm.h`）
 *          因此未激活时区字段组。按 `timeio` 的既定规则，上下文接不住的说明符不算错误，
 *          而是退化为字面量：`%Z` 要求输入里真的出现 `%Z` 这两个字符。于是
 *          `get_time(&t, "%H:%M %Z")` 读 `01:02 UTC` 会**整次提取失败**并置 `strfailbit`，
 *          `*tmb` 保持不变；而 `std::get_time` 会解析出一个时区名再丢弃，看上去是成功的。
 *          从 `std::istream` 迁移时，这类失败很容易被误判成输入数据有问题。详见 `timeio`。
 * @warning 返回的对象**持有这两个裸指针**，只应作为同一完整表达式的一部分立即使用；
 *          详见本文件顶部的说明。
 * @endif
 *
 * @lang{EN}
 * @brief Builds the manipulator that parses a time according to @p fmt into `*tmb`.
 * @param tmb The `tm` receiving the parsed result; may be null, see below.
 * @param fmt A `strptime`-style format string, except that `%Z` / `%z` are unsupported (see
 *            below); may be null, see below.
 * @note Both pointers are validated before parsing, and a null one sets a failure bit on the
 *       stream rather than being dereferenced; see `io_traits<TChar, get_time_t<TChar>>::sread`.
 * @note Fields absent from the format string keep the value they had in `*tmb`: the parse
 *       context is seeded with fallbacks from `*tmb`, so a time-only format string such as
 *       `%H:%M` does not disturb the date. `tm_wday` / `tm_yday` are always recomputed from the
 *       resulting date, and `tm_isdst` is always set to -1 -- no format specifier carries DST
 *       information, and carrying the caller's old value over would be wrong once the date has
 *       been rewritten; -1 means "unknown, let the C library work it out".
 * @note The previous note is about fields the format string says nothing about, not about a
 *       one-to-one mapping between specifiers and `tm` members. A week-number specifier
 *       (`%U`/`%W`/`%V`) parses a whole week, so it moves the month and day into that week
 *       even with no `%m`/`%d` in the format string -- a fallback only fills in what the
 *       format string genuinely cannot determine. A weekday with no week number moves the
 *       date to the first matching day on or after the one in `*tmb`, so at most six days;
 *       see `date_parse_helper::compute_ymd()`.
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
 *       borrows from the date; see `io/traits/tm.h` for the rules), and a day
 *       carried over this way that does not exist in the parsed month becomes the last day of
 *       that month rather than failing the extraction. A `std::tm t{}` (whose `tm_mday` is 0)
 *       with a time-only format string therefore yields the same date as `std::get_time`
 *       followed by `mktime()`.
 * @note The insertion side is not symmetric: `put_time` requires every field of `*tmb` to be in
 *       range and the date fields to form a day that exists, and rejects a `std::tm t{}` with
 *       `strfailbit`. This function accepts it (see the previous note). See `put_time`.
 * @warning **On the year the asymmetry points the other way: this side is the strict one.** `%Y`
 *          (and `%G`, `%C`) accept **only 0..9999, unsigned**, matching
 *          `std::chrono::from_stream("%Y")` and POSIX `strptime`, while `put_time` follows
 *          `std::format` and writes negative years and years of five or more digits. A year of
 *          10000 or -44 written by `put_time` therefore cannot be read back here: the whole
 *          extraction fails with `strfailbit` and `*tmb` is left completely unchanged. **Only
 *          years 0..9999 are guaranteed to round-trip.** This is the one exception to the "the
 *          extraction side is more permissive" rule stated throughout this documentation; see
 *          `timeio` for why.
 * @warning **`%Z` and `%z` are not supported.** `std::tm` has no portable time-zone field, so
 *          the parse context this function uses (`time_parse_context<TChar, true, true, false>`,
 *          see `io/traits/tm.h`) does not activate the time-zone field group. By `timeio`'s
 *          standing rule a specifier the context cannot hold is not an error but degrades to a
 *          literal: `%Z` requires the two characters `%Z` to actually appear in the input. So
 *          `get_time(&t, "%H:%M %Z")` reading `01:02 UTC` fails the **whole extraction** with
 *          `strfailbit` and leaves `*tmb` unchanged, where `std::get_time` parses a zone name,
 *          discards it, and appears to have succeeded. Migrating from `std::istream`, such a
 *          failure is easily misread as a problem with the input data. See `timeio`.
 * @warning The returned object **holds those two raw pointers** and should only be used as part
 *          of the same full expression; see the note at the top of this file.
 * @endif
 */
template<typename TChar>
inline get_time_t<TChar> get_time(std::tm* tmb, const TChar* fmt) { return { tmb, fmt }; }

template <typename TChar>
struct io_traits<TChar, get_time_t<TChar>>
{
    /**
     * @lang{ZH}
     * @brief 按 `fmt` 的格式解析时间并写入 `*tmb`。
     *
     * @note 与 `put_time` 同理，`get_time` 保存的两个裸指针在解引用前必须校验：`f.fmt`
     *       为空时向 `std::basic_string_view` 的隐式转换即为未定义行为，`f.tmb` 为空时
     *       回写 `*(f.tmb)` 是空指针写入。二者都绕过异常机制直接崩溃。
     * @param f 用于接收解析结果的 `tm` 与格式串。`*(f.tmb)` 的现有内容会作为格式串未解析字段
     *          的回退值，详见 `get_time`。`f.tmb` 或 `f.fmt` 为空指针时不解析：不回写
     *          `*(f.tmb)`，抛出的 `stream_error` 由流转为 `strfailbit`。校验在提取运算符
     *          构造哨兵**之后**才做，故 `skipws` 已跳过的前导空白不会退回。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 `f.tmb` 或 `f.fmt` 为空指针，或缺少 timeio facet，
     *        **或解析失败**——格式不匹配、字段越界、中途 EOF 都由 `timeio::get` 以
     *        `stream_error("timeio parse error")` 抛出，这是本函数最常见的失败方式；
     *        另有还原结果不是有效日历日时来自 `convert_to` 的同类抛出。三者一样由流转为
     *        `strfailbit`，仅当该位在异常掩码中时才传播到调用方。
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
     *          parse; see `get_time`. If `f.tmb` or `f.fmt` is null nothing is parsed:
     *          `*(f.tmb)` is not written and the `stream_error` thrown is turned into
     *          `strfailbit` by the stream. The check runs **after** the extraction operator has
     *          built its sentry, so leading whitespace already skipped by `skipws` is not put
     *          back.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If `f.tmb` or `f.fmt` is a null pointer, if the timeio facet is
     *        missing, **or if parsing fails** — a format mismatch, an out-of-range field or an
     *        EOF part-way through are all thrown by `timeio::get` as
     *        `stream_error("timeio parse error")`, which is this function's most common failure;
     *        `convert_to` throws the same way when the recovered result is not a calendar day
     *        that exists. All three are turned into `strfailbit` by the stream and reach the
     *        caller only when that bit is in the exception mask.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>
                  && (steppable_back<TIter> || is_istreambuf_iterator<TIter>))
    static TIter sread(TIter s, TSent s_end, ios_base<TChar>& io, const locale<TChar>& loc,
                       const get_time_t<TChar>& f)
    {
        if (f.tmb == nullptr || f.fmt == nullptr)
            throw stream_error("get_time fail: null tm or format pointer");

        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        // The context is date+time without a time zone, matching
        // parse_context_type<TChar, std::tm>; that is also the combination whose
        // convert_to(std::tm&) is available.
        auto tmp = parse_context_type<TChar, std::tm>::make_parse_context(*(f.tmb));
        auto res = mp->get(s, s_end, tmp, f.fmt);

        tmp.convert_to(*(f.tmb));

        return res;
    }
};
}
