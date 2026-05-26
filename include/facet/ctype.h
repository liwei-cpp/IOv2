/**
 * @file ctype.h
 * @lang{ZH}
 * `ctype` facet：字符分类、大小写转换及宽窄字符映射。
 *
 * 本文件提供以下核心组件：
 * - `detail::ctype_ops<Derived>`：CRTP 混入类，基于派生类的五个单字符原语
 *   （`is`、`toupper`、`tolower`、`widen`、`narrow`）提供序列操作变体及
 *   带回退的 `narrow(c, def)` 重载。
 * - `ctype<CharT>`（`sizeof(CharT) == 1`，即 `char`、`char8_t`）：单字节特化；
 *   构造时将全部 256 个值的五张原语表一次性快照到数组中，后续查询为零虚调用的 O(1)
 *   数组读取，且不持有 `ctype_conf<CharT>` 指针。
 * - `ctype<CharT>`（`sizeof(CharT) > 1`，即 `wchar_t`、`char32_t`）：多字节特化；
 *   对 `[0, s_len)` 区间同样预建快照表，对超出区间的值则在每次调用时直接转发到
 *   底层 `ctype_conf<CharT>`，无缓存、无锁，满足多线程安全契约。
 * - CTAD 推导指引：从 `shared_ptr<ctype_conf<CharT>>` 自动推导 `ctype<CharT>`。
 * @endif
 *
 * @lang{EN}
 * `ctype` facet: character classification, case conversion, and widen/narrow mapping.
 *
 * This file provides the following core components:
 * - `detail::ctype_ops<Derived>`: CRTP mixin class that provides sequence-operation
 *   variants and a fallback-taking `narrow(c, def)` overload, derived from the five
 *   single-character primitives of the `Derived` class (`is`, `toupper`, `tolower`,
 *   `widen`, `narrow`).
 * - `ctype<CharT>` (`sizeof(CharT) == 1`, i.e. `char`, `char8_t`): Single-byte
 *   specialization; at construction all five primitive tables are snapshotted for all
 *   256 values into plain arrays. Later lookups are O(1) array reads with no virtual
 *   dispatch and no `ctype_conf<CharT>` pointer retained.
 * - `ctype<CharT>` (`sizeof(CharT) > 1`, i.e. `wchar_t`, `char32_t`): Multi-byte
 *   specialization; values in `[0, s_len)` are served from precomputed snapshot tables;
 *   values beyond that range are forwarded directly to the underlying `ctype_conf<CharT>`
 *   on each call, lock-free, satisfying the multi-thread safety contract.
 * - CTAD deduction guide: deduces `ctype<CharT>` from `shared_ptr<ctype_conf<CharT>>`.
 * @endif
 */

#pragma once

#include <common/metafunctions.h>
#include <facet/ctype_details.h>
#include <facet/facet_common.h>

#include <array>
#include <concepts>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * `ctype` facet 模板的前向声明。
 * @endif
 *
 * @lang{EN}
 * Forward declaration of the `ctype` facet template.
 * @endif
 */
template <typename CharT>
class ctype;

namespace detail
{
/**
 * @lang{ZH}
 * CRTP 混入类：基于派生类的五个单字符原语提供序列操作及带回退的 `narrow` 重载。
 *
 * `ctype<CharT>` 的两个特化均从本类继承，以避免重复定义序列方法。
 *
 * 参数类型依赖 `Derived::char_type` 或 `Derived::mask` 的方法被声明为受约束的函数模板：
 * `requires` 子句将这些依赖名称的求值推迟到调用时（此时 `Derived` 已是完整类型），
 * 从而避免类作用域类型别名在 `Derived` 不完整时引发的错误。
 *
 * @par 区间前置条件
 * 所有接受迭代器对的方法（`is_seq`、`scan_is_any`、`scan_not_any`、`toupper_seq`、
 * `tolower_seq`、`widen_seq`、`narrow_seq`）均要求 `[beg, end)` 为合法区间，即
 * 从 `beg` 经反复递增可到达 `end`。传入不满足此条件的迭代器（如随机访问区间上
 * 顺序颠倒的参数、不匹配的迭代器对、超出合法区间末尾的 `end` 等）属于未定义行为。
 * 与等效的 `std::ctype` 契约一致；在热路径上不做断言。
 * @endif
 *
 * @lang{EN}
 * CRTP mixin class supplying sequence operations and the `narrow(c, default)` overload
 * in terms of the `Derived` class's five single-character primitives: `is()`,
 * `toupper()`, `tolower()`, `widen()`, and `narrow()`. Both `ctype<CharT>`
 * specializations inherit from this to avoid duplicating these definitions.
 *
 * Methods whose parameter types depend on `Derived::char_type` or `Derived::mask`
 * are declared as constrained function templates: the `requires` clause defers
 * evaluation of those dependent names to call time (when `Derived` is complete),
 * avoiding the incomplete-type error that class-scope type aliases would cause.
 *
 * @par Range precondition
 * Every method taking iterator pairs (`is_seq`, `scan_is_any`, `scan_not_any`,
 * `toupper_seq`, `tolower_seq`, `widen_seq`, `narrow_seq`) requires `[beg, end)` to
 * be a valid range, i.e. `end` is reachable from `beg` via repeated `++`. Passing
 * iterators that do not satisfy this (swapped arguments on a random-access range,
 * mismatched iterator pairs, an `end` past a valid range, etc.) is undefined behaviour.
 * Matches the equivalent `std::ctype` contract; not asserted on the hot path.
 * @endif
 *
 * @tparam Derived
 * @lang{ZH} CRTP 派生类自身的类型，须提供 `is`、`toupper`、`tolower`、`widen`、`narrow` 五个原语。 @endif
 * @lang{EN} The CRTP derived class type, which must provide the five primitives: `is`,
 * `toupper`, `tolower`, `widen`, and `narrow`. @endif
 */
template <typename Derived>
class ctype_ops
{
    /** @lang{ZH} 返回对 CRTP 派生类实例的常量引用。 @endif
     *  @lang{EN} Returns a const reference to the CRTP derived class instance. @endif */
    const Derived& self() const { return static_cast<const Derived&>(*this); }

public:
    /**
     * @lang{ZH}
     * 测试字符 `c` 是否具有掩码 `m` 中的任意分类属性。
     * @endif
     *
     * @lang{EN}
     * Tests whether character `c` has any of the classification properties in mask `m`.
     * @endif
     *
     * @param m
     * @lang{ZH} 要检测的分类属性位掩码。 @endif
     * @lang{EN} The classification property bitmask to test against. @endif
     *
     * @param c
     * @lang{ZH} 待检测的字符。 @endif
     * @lang{EN} The character to test. @endif
     *
     * @return
     * @lang{ZH} 若 `c` 具有 `m` 中至少一个分类属性则返回 `true`；否则返回 `false`。 @endif
     * @lang{EN} `true` if `c` has at least one of the classification properties in `m`;
     * `false` otherwise. @endif
     */
    template <typename TM, typename TC>
        requires std::convertible_to<TM, typename Derived::mask> &&
                 std::convertible_to<TC, typename Derived::char_type>
    bool is_any(TM m, TC c) const
    {
        return self().is(c) & m;
    }

    /**
     * @lang{ZH}
     * 对范围 `[low, high)` 内的每个字符调用 `is()`，将结果依次写入 `vec`。
     * @endif
     *
     * @lang{EN}
     * Calls `is()` on each character in `[low, high)` and writes the results to `vec`.
     * @endif
     *
     * @param low
     * @lang{ZH} 输入范围的起始迭代器。 @endif
     * @lang{EN} Start iterator of the input range. @endif
     *
     * @param high
     * @lang{ZH} 输入范围的末尾迭代器（独占上界）。 @endif
     * @lang{EN} End iterator of the input range (exclusive). @endif
     *
     * @param vec
     * @lang{ZH} 接收分类掩码结果的输出迭代器。 @endif
     * @lang{EN} Output iterator to receive the classification mask results. @endif
     *
     * @return
     * @lang{ZH} 写入最后一个元素之后的输出迭代器位置。 @endif
     * @lang{EN} Output iterator pointing one past the last element written. @endif
     */
    template <typename InIt, typename OutIt>
    OutIt is_seq(InIt low, InIt high, OutIt vec) const
    {
        while (low != high)
            *vec++ = self().is(*low++);
        return vec;
    }

    /**
     * @lang{ZH}
     * 在范围 `[beg, end)` 中前向扫描，返回第一个具有掩码 `m` 中任意属性的字符位置。
     * 若无此字符则返回 `end`。
     * @endif
     *
     * @lang{EN}
     * Scans forward in `[beg, end)` and returns the first iterator whose character
     * has any of the properties in mask `m`. Returns `end` if no such character exists.
     * @endif
     *
     * @param m
     * @lang{ZH} 要搜索的分类属性位掩码。 @endif
     * @lang{EN} The classification property bitmask to search for. @endif
     *
     * @param beg
     * @lang{ZH} 扫描范围的起始迭代器。 @endif
     * @lang{EN} Start iterator of the scan range. @endif
     *
     * @param end
     * @lang{ZH} 扫描范围的末尾迭代器（独占上界）。 @endif
     * @lang{EN} End iterator of the scan range (exclusive). @endif
     *
     * @return
     * @lang{ZH} 第一个具有 `m` 中任意属性的字符所在的迭代器；若不存在则返回 `end`。 @endif
     * @lang{EN} Iterator to the first character with any property in `m`; `end` if none. @endif
     */
    template <typename TM, typename InIt>
        requires std::convertible_to<TM, typename Derived::mask>
    InIt scan_is_any(TM m, InIt beg, InIt end) const
    {
        while ((beg != end) && (!(self().is(*beg) & m)))
            ++beg;
        return beg;
    }

    /**
     * @lang{ZH}
     * 在范围 `[beg, end)` 中前向扫描，返回第一个**不**具有掩码 `m` 中任意属性的字符位置。
     * 若所有字符均具有 `m` 中至少一个属性则返回 `end`。
     * @endif
     *
     * @lang{EN}
     * Scans forward in `[beg, end)` and returns the first iterator whose character has
     * **none** of the properties in mask `m`. Returns `end` if every character has at
     * least one property in `m`.
     * @endif
     *
     * @param m
     * @lang{ZH} 要跳过的分类属性位掩码。 @endif
     * @lang{EN} The classification property bitmask to skip over. @endif
     *
     * @param beg
     * @lang{ZH} 扫描范围的起始迭代器。 @endif
     * @lang{EN} Start iterator of the scan range. @endif
     *
     * @param end
     * @lang{ZH} 扫描范围的末尾迭代器（独占上界）。 @endif
     * @lang{EN} End iterator of the scan range (exclusive). @endif
     *
     * @return
     * @lang{ZH} 第一个不具有 `m` 中任意属性的字符所在的迭代器；若不存在则返回 `end`。 @endif
     * @lang{EN} Iterator to the first character with no property in `m`; `end` if none. @endif
     */
    template <typename TM, typename InIt>
        requires std::convertible_to<TM, typename Derived::mask>
    InIt scan_not_any(TM m, InIt beg, InIt end) const
    {
        while ((beg != end) && (self().is(*beg) & m))
            ++beg;
        return beg;
    }

    /**
     * @lang{ZH}
     * 对范围 `[beg, end)` 内的每个字符调用 `toupper()`，将结果依次写入 `dst`。
     * @endif
     *
     * @lang{EN}
     * Calls `toupper()` on each character in `[beg, end)` and writes the results to `dst`.
     * @endif
     *
     * @param beg
     * @lang{ZH} 输入范围的起始迭代器。 @endif
     * @lang{EN} Start iterator of the input range. @endif
     *
     * @param end
     * @lang{ZH} 输入范围的末尾迭代器（独占上界）。 @endif
     * @lang{EN} End iterator of the input range (exclusive). @endif
     *
     * @param dst
     * @lang{ZH} 接收大写转换结果的输出迭代器。 @endif
     * @lang{EN} Output iterator to receive the uppercase results. @endif
     *
     * @return
     * @lang{ZH} 写入最后一个元素之后的输出迭代器位置。 @endif
     * @lang{EN} Output iterator pointing one past the last element written. @endif
     */
    template <typename InIt, typename OutIt>
    OutIt toupper_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().toupper(*beg++);
        return dst;
    }

    /**
     * @lang{ZH}
     * 对范围 `[beg, end)` 内的每个字符调用 `tolower()`，将结果依次写入 `dst`。
     * @endif
     *
     * @lang{EN}
     * Calls `tolower()` on each character in `[beg, end)` and writes the results to `dst`.
     * @endif
     *
     * @param beg
     * @lang{ZH} 输入范围的起始迭代器。 @endif
     * @lang{EN} Start iterator of the input range. @endif
     *
     * @param end
     * @lang{ZH} 输入范围的末尾迭代器（独占上界）。 @endif
     * @lang{EN} End iterator of the input range (exclusive). @endif
     *
     * @param dst
     * @lang{ZH} 接收小写转换结果的输出迭代器。 @endif
     * @lang{EN} Output iterator to receive the lowercase results. @endif
     *
     * @return
     * @lang{ZH} 写入最后一个元素之后的输出迭代器位置。 @endif
     * @lang{EN} Output iterator pointing one past the last element written. @endif
     */
    template <typename InIt, typename OutIt>
    OutIt tolower_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().tolower(*beg++);
        return dst;
    }

    /**
     * @lang{ZH}
     * 对范围 `[beg, end)` 内的每个窄字符调用 `widen()`，将结果依次写入 `dst`。
     * @endif
     *
     * @lang{EN}
     * Calls `widen()` on each narrow character in `[beg, end)` and writes the results
     * to `dst`.
     * @endif
     *
     * @param beg
     * @lang{ZH} 输入范围的起始迭代器。 @endif
     * @lang{EN} Start iterator of the input range. @endif
     *
     * @param end
     * @lang{ZH} 输入范围的末尾迭代器（独占上界）。 @endif
     * @lang{EN} End iterator of the input range (exclusive). @endif
     *
     * @param dst
     * @lang{ZH} 接收拓宽结果的输出迭代器。 @endif
     * @lang{EN} Output iterator to receive the widened results. @endif
     *
     * @return
     * @lang{ZH} 写入最后一个元素之后的输出迭代器位置。 @endif
     * @lang{EN} Output iterator pointing one past the last element written. @endif
     */
    template <typename InIt, typename OutIt>
    OutIt widen_seq(InIt beg, InIt end, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().widen(*beg++);
        return dst;
    }

    /**
     * @lang{ZH}
     * 将字符 `c` 窄化为 `char`，若无对应单字节表示则返回回退字符 `def`。
     *
     * @par `def` 前置条件
     * 当 `self().narrow(c)` 不返回值时，`def` 原样返回；本函数**不**验证 `def` 在目标
     * locale 中是否可表示。按照惯例（与 `std::ctype::narrow` 一致），调用方应传入基本源
     * 字符集中的成员——通常是可打印的 ASCII 字节，如 `'?'`——以确保回退字符在任何编码
     * 下均有意义。传入任意字节（如 `'\xFF'`）会被静默接受，但可能产生在 locale 编码中
     * 不是合法独立字符的输出。
     * @endif
     *
     * @lang{EN}
     * Narrows character `c` to `char`, returning the fallback character `def` if no
     * single-byte representation exists.
     *
     * @par `def` precondition
     * `def` is returned verbatim when `self().narrow(c)` yields no value; this function
     * does NOT validate that `def` is itself representable in the target locale. By
     * convention (matching `std::ctype::narrow`) callers should pass a member of the
     * basic source character set — typically a printable ASCII byte such as `'?'` —
     * so the fallback is meaningful in any encoding. Passing an arbitrary byte (e.g.
     * `'\xFF'`) is accepted silently and may yield output that is not a valid standalone
     * character in the locale's encoding.
     * @endif
     *
     * @param c
     * @lang{ZH} 待窄化的字符。 @endif
     * @lang{EN} The character to narrow. @endif
     *
     * @param def
     * @lang{ZH} 当 `c` 无对应单字节表示时返回的回退字符。 @endif
     * @lang{EN} The fallback character returned when `c` has no single-byte representation. @endif
     *
     * @return
     * @lang{ZH} `c` 的单字节窄化结果；若无对应表示则返回 `def`。 @endif
     * @lang{EN} The single-byte narrowed result for `c`; `def` if no representation exists. @endif
     */
    template <typename TC>
        requires std::convertible_to<TC, typename Derived::char_type>
    char narrow(TC c, char def) const
    {
        auto res = self().narrow(c);
        return res ? *res : def;
    }

    /**
     * @lang{ZH}
     * 对范围 `[beg, end)` 内的每个字符调用 `narrow(c, dflt)`，将结果依次写入 `dst`。
     * @endif
     *
     * @lang{EN}
     * Calls `narrow(c, dflt)` on each character in `[beg, end)` and writes the results
     * to `dst`.
     * @endif
     *
     * @param beg
     * @lang{ZH} 输入范围的起始迭代器。 @endif
     * @lang{EN} Start iterator of the input range. @endif
     *
     * @param end
     * @lang{ZH} 输入范围的末尾迭代器（独占上界）。 @endif
     * @lang{EN} End iterator of the input range (exclusive). @endif
     *
     * @param dflt
     * @lang{ZH} 无对应单字节表示时使用的回退字符。 @endif
     * @lang{EN} The fallback character to use when no single-byte representation exists. @endif
     *
     * @param dst
     * @lang{ZH} 接收窄化结果的输出迭代器。 @endif
     * @lang{EN} Output iterator to receive the narrowed results. @endif
     *
     * @return
     * @lang{ZH} 写入最后一个元素之后的输出迭代器位置。 @endif
     * @lang{EN} Output iterator pointing one past the last element written. @endif
     */
    template <typename InIt, typename OutIt>
    OutIt narrow_seq(InIt beg, InIt end, char dflt, OutIt dst) const
    {
        while (beg != end)
            *dst++ = self().narrow(*beg++, dflt);
        return dst;
    }
};
} // namespace detail

/**
 * @lang{ZH}
 * 单字节字符类型（`char`、`char8_t`）的 `ctype` facet 特化。
 *
 * 输入空间 `[0, s_len)` 在构造时完全可枚举，因此本特化将所有五张原语表（`is`/`toupper`/
 * `tolower`/`widen`/`narrow`）一次性快照到平坦数组中，之后丢弃 `ctype_conf<CharT>`
 * 指针。所有后续查询均为零虚调用、零 `shared_ptr` 间接的 O(1) 数组读取。
 *
 * @par 与 `sizeof(CharT) > 1` 特化的不对称性
 * `sizeof > 1` 特化保留 `m_obj` 指针，因为其输入空间过大无法枚举，对超出表范围的值
 * 在调用时转发给 conf。因此，对用户派生的 `ctype_conf<CharT>` 中 `is`/`toupper`/
 * `tolower`/`widen`/`narrow` 的运行时修改，在 `sizeof > 1` 路径（超出表范围的分支）
 * 上会即时生效，但在本特化上**不会**生效：本特化在构造时捕获了一次性快照，之后永远
 * 不再查询 conf。需要动态变化单字节行为的调用方应在每次修改后重建一个新的 `ctype<CharT>`，
 * 或提供一个不做快照的自定义 `ctype<CharT>`。
 *
 * @tparam CharT
 * @lang{ZH} 单字节字符类型，须满足 `sizeof(CharT) == 1`。 @endif
 * @lang{EN} The single-byte character type; must satisfy `sizeof(CharT) == 1`. @endif
 *
 * @lang{EN}
 * `ctype` facet specialization for single-byte character types (`char`, `char8_t`).
 *
 * The full `[0, s_len)` input space is enumerable at construction, so this
 * specialization materializes all five primitive tables (`is`/`toupper`/`tolower`/
 * `widen`/`narrow`) into a one-shot snapshot and discards the `ctype_conf<CharT>`
 * pointer afterwards. All later lookups are pure O(1) array reads with no virtual
 * dispatch and no `shared_ptr` indirection.
 *
 * @par Asymmetry with the `sizeof(CharT) > 1` specialization
 * The `sizeof > 1` specialization retains `m_obj` because its input space is too large
 * to enumerate and forwards out-of-table values to the conf at call time. As a
 * consequence, runtime mutations to a user-derived `ctype_conf<CharT>`'s `is`/`toupper`/
 * `tolower`/`widen`/`narrow` take effect for the `sizeof > 1` path (on the out-of-table
 * branch) but NOT for this specialization: it captures a one-shot snapshot at
 * construction and never re-queries the conf. Callers who need dynamically-changing
 * single-byte behaviour should either rebuild a new `ctype<CharT>` after each change,
 * or supply a custom `ctype<CharT>` that does not snapshot.
 * @endif
 */
template <typename CharT>
    requires (sizeof(CharT) == 1)
class ctype<CharT> : public detail::ctype_ops<ctype<CharT>>
{
    // Note: we have to use unsigned char here to avoid negative value.
    // Sizing is in units of "values representable by unsigned char", not a
    // hard-coded 256. The CHAR_BIT == 8 assumption is enforced by a
    // static_assert in ctype_details.h, so on every supported platform
    // s_len == 256.
    /** @lang{ZH} 查找表的元素数量，等于 `unsigned char` 可表示的不同值的个数（所有支持平台上均为 256）。 @endif
     *  @lang{EN} Number of entries in each lookup table, equal to the number of distinct
     *  values representable by `unsigned char` (256 on all supported platforms). @endif */
    constexpr static unsigned s_len = std::numeric_limits<unsigned char>::max() + 1;

public:
    /** @lang{ZH} locale 批量 facet 构造机制所用的构造规则，指定构造本 facet 所需的 `ctype_conf<CharT>`。 @endif
     *  @lang{EN} Construction rule for the locale's batch facet-construction mechanism,
     *  specifying that `ctype_conf<CharT>` is needed to construct this facet. @endif */
    using create_rules = facet_create_rule<ctype_conf<CharT>>;

    /** @lang{ZH} 此 facet 操作的字符类型。 @endif
     *  @lang{EN} The character type operated on by this facet. @endif */
    using char_type = CharT;

    /** @lang{ZH} 字符分类属性的位掩码类型，与 `base_ft<ctype>::mask` 一致。 @endif
     *  @lang{EN} Bitmask type for character-classification properties, consistent with
     *  `base_ft<ctype>::mask`. @endif */
    using mask = typename ctype_conf<CharT>::mask;

    /**
     * @lang{ZH}
     * 构造函数。从 `p_obj` 提供的 `ctype_conf<CharT>` 实例为全部 256 个字节值预建
     * 五张快照表，之后不再持有 `p_obj`。
     * @endif
     *
     * @lang{EN}
     * Constructor. Builds five snapshot tables for all 256 byte values from the
     * `ctype_conf<CharT>` instance provided by `p_obj`; the pointer is not retained
     * afterwards.
     * @endif
     *
     * @param p_obj
     * @lang{ZH} 指向 `ctype_conf<CharT>` 实例的共享指针，不得为空。 @endif
     * @lang{EN} Shared pointer to the `ctype_conf<CharT>` instance; must not be empty. @endif
     *
     * @throws std::runtime_error
     * @lang{ZH} 若 `p_obj` 为空指针。 @endif
     * @lang{EN} If `p_obj` is an empty pointer. @endif
     */
    template <shared_ptr_to<ctype_conf<CharT>> TConfPtr>
    ctype(TConfPtr p_obj)
    {
        if (!p_obj) throw std::runtime_error("shared_ptr is empty");
        for (unsigned i = 0; i < s_len; ++i)
        {
            m_table[i] = p_obj->is(static_cast<CharT>(i));
            m_toupper[i] = p_obj->toupper(static_cast<CharT>(i));
            m_tolower[i] = p_obj->tolower(static_cast<CharT>(i));
            m_widen[i] = p_obj->widen(static_cast<CharT>(i));
            m_narrow[i] = p_obj->narrow(static_cast<CharT>(i));
        }
    }

    ~ctype() = default;
    ctype(const ctype&) = delete;
    ctype& operator=(const ctype&) = delete;
    ctype(ctype&&) = delete;
    ctype& operator=(ctype&&) = delete;

public:
    /**
     * @lang{ZH}
     * 返回字符 `c` 的分类掩码（O(1) 快照表查询）。
     * @endif
     *
     * @lang{EN}
     * Returns the classification mask for character `c` (O(1) snapshot table lookup).
     * @endif
     *
     * @param c @lang{ZH} 待分类的字符。 @endif @lang{EN} The character to classify. @endif
     * @return @lang{ZH} `c` 的分类掩码。 @endif @lang{EN} The classification mask for `c`. @endif
     */
    mask is(CharT c) const
    {
        return m_table[static_cast<unsigned char>(c)];
    }

    /**
     * @lang{ZH}
     * 返回字符 `c` 的大写形式（O(1) 快照表查询）。
     * @endif
     *
     * @lang{EN}
     * Returns the uppercase form of character `c` (O(1) snapshot table lookup).
     * @endif
     *
     * @param c @lang{ZH} 待转换的字符。 @endif @lang{EN} The character to convert. @endif
     * @return @lang{ZH} `c` 的大写形式。 @endif @lang{EN} The uppercase form of `c`. @endif
     */
    CharT toupper(CharT c) const
    {
        return m_toupper[static_cast<unsigned char>(c)];
    }

    /**
     * @lang{ZH}
     * 返回字符 `c` 的小写形式（O(1) 快照表查询）。
     * @endif
     *
     * @lang{EN}
     * Returns the lowercase form of character `c` (O(1) snapshot table lookup).
     * @endif
     *
     * @param c @lang{ZH} 待转换的字符。 @endif @lang{EN} The character to convert. @endif
     * @return @lang{ZH} `c` 的小写形式。 @endif @lang{EN} The lowercase form of `c`. @endif
     */
    CharT tolower(CharT c) const
    {
        return m_tolower[static_cast<unsigned char>(c)];
    }

    /**
     * @lang{ZH}
     * 将窄字符 `c` 拓宽为 `CharT`（O(1) 快照表查询）。
     * @endif
     *
     * @lang{EN}
     * Widens the narrow character `c` to `CharT` (O(1) snapshot table lookup).
     * @endif
     *
     * @param c @lang{ZH} 待拓宽的窄字符。 @endif @lang{EN} The narrow character to widen. @endif
     * @return @lang{ZH} `c` 对应的 `CharT` 值。 @endif @lang{EN} The `CharT` value corresponding to `c`. @endif
     */
    CharT widen(char c) const
    {
        return m_widen[static_cast<unsigned char>(c)];
    }

    /**
     * @lang{ZH}
     * 将字符 `c` 窄化为 `char`（O(1) 快照表查询）。
     * 若构造时 `ctype_conf<CharT>::narrow` 对该值返回 `nullopt`，则此处同样返回 `nullopt`。
     * @endif
     *
     * @lang{EN}
     * Narrows character `c` to `char` (O(1) snapshot table lookup).
     * Returns `nullopt` if `ctype_conf<CharT>::narrow` returned `nullopt` for this
     * value at construction time.
     * @endif
     *
     * @param c @lang{ZH} 待窄化的字符。 @endif @lang{EN} The character to narrow. @endif
     * @return @lang{ZH} 包含窄化结果的 `std::optional<char>`；若无对应表示则为 `nullopt`。 @endif
     * @lang{EN} A `std::optional<char>` with the narrowed result; `nullopt` if no
     * single-byte representation exists. @endif
     */
    std::optional<char> narrow(CharT c) const
    {
        return m_narrow[static_cast<unsigned char>(c)];
    }

    using detail::ctype_ops<ctype<CharT>>::narrow;

private:
    /** @lang{ZH} 分类掩码快照表，索引为 `unsigned char` 值。 @endif
     *  @lang{EN} Classification mask snapshot table, indexed by `unsigned char` value. @endif */
    std::array<mask, s_len>                m_table;

    /** @lang{ZH} 大写映射快照表，索引为 `unsigned char` 值。 @endif
     *  @lang{EN} Uppercase mapping snapshot table, indexed by `unsigned char` value. @endif */
    std::array<CharT, s_len>               m_toupper;

    /** @lang{ZH} 小写映射快照表，索引为 `unsigned char` 值。 @endif
     *  @lang{EN} Lowercase mapping snapshot table, indexed by `unsigned char` value. @endif */
    std::array<CharT, s_len>               m_tolower;

    /** @lang{ZH} 拓宽映射快照表，索引为 `unsigned char` 值。 @endif
     *  @lang{EN} Widen mapping snapshot table, indexed by `unsigned char` value. @endif */
    std::array<CharT, s_len>               m_widen;

    /** @lang{ZH} 窄化映射快照表，索引为 `unsigned char` 值；元素可为 `nullopt`。 @endif
     *  @lang{EN} Narrow mapping snapshot table, indexed by `unsigned char` value;
     *  elements may be `nullopt`. @endif */
    std::array<std::optional<char>, s_len> m_narrow;
};

/**
 * @lang{ZH}
 * 多字节字符类型（`wchar_t`、`char32_t`）的 `ctype` facet 特化。
 *
 * 对 `[0, s_len)` 区间内的值，从构造时预建的无锁快照表查询；对超出该区间的值，
 * 在每次调用时直接转发到底层 `ctype_conf<CharT>`，无缓存、无锁。
 *
 * @par 线程安全契约
 * 由于超出表范围的路径会并发调用 conf，`ctype_conf<CharT>` 的 `const` 方法须对并发
 * 调用安全。默认实现满足此要求：成员在构造后不可变；`is`/`toupper`/`tolower` 使用显式
 * 传入不可变 `locale_t` 的 `*_l` 函数；`narrow()` 仅通过每线程 `uselocale` 守卫切换
 * 调用线程的 locale。用户提供的 `ctype_conf<CharT>` 重写须保持此属性。
 *
 * @par 用户重写的自洽性契约
 * `[0, s_len)` 区间内的值由构造时建立的快照表提供，区间外的值则在每次调用时经由虚调用
 * 转发到 conf。若用户派生的 `ctype_conf<CharT>` 中 `is()`、`toupper()`、`tolower()`
 * 之间不自洽（例如 `is(c) & upper` 为 `true` 而 `toupper(c) == c`，或返回与大小写转换
 * 结果相矛盾的掩码），将出现"撕裂"行为：区间内字符使用构造时冻结的快照，区间外字符使用
 * 每次调用时的实时结果。重写须在 `is`/`toupper`/`tolower`（以及在语义上要求的
 * `is`/`widen`/`narrow`）之间保持自洽，以确保无论经由哪条分支服务，同一字符都产生
 * 相同的结果。
 *
 * @tparam CharT
 * @lang{ZH} 多字节字符类型，须满足 `sizeof(CharT) > 1`。 @endif
 * @lang{EN} The multi-byte character type; must satisfy `sizeof(CharT) > 1`. @endif
 *
 * @lang{EN}
 * `ctype` facet specialization for multi-byte character types (`wchar_t`, `char32_t`).
 *
 * Values in `[0, s_len)` are served from precomputed lock-free snapshot tables;
 * values beyond that range are forwarded directly to the underlying `ctype_conf<CharT>`
 * on each call, with no cache and no lock.
 *
 * @par Thread-safety contract
 * Because the out-of-table path calls the conf concurrently, `ctype_conf<CharT>`'s
 * `const` methods must be safe for concurrent invocation. The default implementation
 * satisfies this: its members are immutable after construction; `is`/`toupper`/`tolower`
 * use `*_l` locale functions that take an explicit, immutable `locale_t`; and `narrow()`
 * switches only the calling thread's locale via a per-thread `uselocale` guard.
 * A user-supplied override of `ctype_conf<CharT>` must preserve this property.
 *
 * @par Self-consistency contract for user overrides
 * Values inside `[0, s_len)` are served from the snapshot tables built at construction,
 * while values outside that range go through virtual dispatch to the conf on every call.
 * A user-derived `ctype_conf<CharT>` whose `is()`, `toupper()`, and `tolower()` are not
 * internally consistent — for example, `is(c) & upper` returning `true` while
 * `toupper(c) == c`, or returning a mask that disagrees with what the case-conversion
 * methods imply — will exhibit "torn" behaviour: the inconsistency is frozen into the
 * in-range tables at construction but is re-observed live for every out-of-range
 * character. Overrides must be self-consistent across `is`/`toupper`/`tolower` (and
 * across `is`/`widen`/`narrow` where the semantics demand it) so that the same
 * character produces the same answer regardless of which branch served it.
 * @endif
 */
template <typename CharT>
    requires (sizeof(CharT) > 1)
class ctype<CharT> : public detail::ctype_ops<ctype<CharT>>
{
    // See the matching s_len note in the sizeof(CharT)==1 specialization
    // above. Tables span [0, unsigned_char::max()+1) per-byte; CHAR_BIT == 8
    // is enforced by static_assert in ctype_details.h.
    /** @lang{ZH} 快照表覆盖的值范围大小 `[0, s_len)`，在所有支持平台上均为 256。 @endif
     *  @lang{EN} Size of the value range `[0, s_len)` covered by the snapshot tables;
     *  256 on all supported platforms. @endif */
    constexpr static unsigned s_len = std::numeric_limits<unsigned char>::max() + 1;

    /** @lang{ZH}
     * `CharT` 的无符号对应类型，用于将（可能有符号的）`CharT` 折叠到 `[0, 2^N)` 以进行
     * 表范围检查和索引，避免符号扩展。`make_unsigned_t` 保证与 `CharT` 等宽，任何值
     * 均不被截断。
     * @endif
     * @lang{EN}
     * Unsigned counterpart of `CharT`, used to fold a (possibly signed) `CharT` into
     * `[0, 2^N)` for the table-range check and indexing without sign extension.
     * `make_unsigned_t` guarantees the same width as `CharT`, so no value is truncated
     * regardless of how wide plain `unsigned` happens to be.
     * @endif */
    using uchar_type = std::make_unsigned_t<CharT>;

public:
    /** @lang{ZH} locale 批量 facet 构造机制所用的构造规则，指定构造本 facet 所需的 `ctype_conf<CharT>`。 @endif
     *  @lang{EN} Construction rule for the locale's batch facet-construction mechanism,
     *  specifying that `ctype_conf<CharT>` is needed to construct this facet. @endif */
    using create_rules = facet_create_rule<ctype_conf<CharT>>;

    /** @lang{ZH} 此 facet 操作的字符类型。 @endif
     *  @lang{EN} The character type operated on by this facet. @endif */
    using char_type = CharT;

    /** @lang{ZH} 字符分类属性的位掩码类型，与 `base_ft<ctype>::mask` 一致。 @endif
     *  @lang{EN} Bitmask type for character-classification properties, consistent with
     *  `base_ft<ctype>::mask`. @endif */
    using mask = typename ctype_conf<CharT>::mask;

    /**
     * @lang{ZH}
     * 构造函数。保留 `p_obj` 指针，并从中为 `[0, s_len)` 区间的全部值预建五张快照表。
     * @endif
     *
     * @lang{EN}
     * Constructor. Retains the `p_obj` pointer and builds five snapshot tables for all
     * values in `[0, s_len)` from it.
     * @endif
     *
     * @param p_obj
     * @lang{ZH} 指向 `ctype_conf<CharT>` 实例的共享指针，不得为空，且在本对象生命周期内须保持有效。 @endif
     * @lang{EN} Shared pointer to the `ctype_conf<CharT>` instance; must not be empty
     * and must remain valid for the lifetime of this object. @endif
     *
     * @throws std::runtime_error
     * @lang{ZH} 若 `p_obj` 为空指针。 @endif
     * @lang{EN} If `p_obj` is an empty pointer. @endif
     */
    template <shared_ptr_to<ctype_conf<CharT>> TConfPtr>
    ctype(TConfPtr p_obj)
        : m_obj(p_obj)
    {
        if (!m_obj) throw std::runtime_error("shared_ptr is empty");
        for (unsigned i = 0; i < s_len; ++i)
        {
            m_toupper[i] = m_obj->toupper(static_cast<CharT>(i));
            m_tolower[i] = m_obj->tolower(static_cast<CharT>(i));
            m_widen[i] = m_obj->widen(static_cast<CharT>(i));
            m_narrow[i] = m_obj->narrow(static_cast<CharT>(i));
            m_table[i] = m_obj->is(static_cast<CharT>(i));
        }
    }

    ~ctype() = default;
    ctype(const ctype&) = delete;
    ctype& operator=(const ctype&) = delete;
    ctype(ctype&&) = delete;
    ctype& operator=(ctype&&) = delete;

public:
    /**
     * @lang{ZH}
     * 返回字符 `c` 的分类掩码。区间内查快照表，区间外转发至 conf。
     * @endif
     *
     * @lang{EN}
     * Returns the classification mask for character `c`. Serves from the snapshot
     * table for in-range values; forwards to the conf for out-of-range values.
     * @endif
     *
     * @param c @lang{ZH} 待分类的字符。 @endif @lang{EN} The character to classify. @endif
     * @return @lang{ZH} `c` 的分类掩码。 @endif @lang{EN} The classification mask for `c`. @endif
     */
    mask is(CharT c) const
    {
        return do_is(c);
    }

    /**
     * @lang{ZH}
     * 返回字符 `c` 的大写形式。区间内查快照表，区间外转发至 conf。
     * @endif
     *
     * @lang{EN}
     * Returns the uppercase form of character `c`. Serves from the snapshot table
     * for in-range values; forwards to the conf for out-of-range values.
     * @endif
     *
     * @param c @lang{ZH} 待转换的字符。 @endif @lang{EN} The character to convert. @endif
     * @return @lang{ZH} `c` 的大写形式。 @endif @lang{EN} The uppercase form of `c`. @endif
     */
    CharT toupper(CharT c) const
    {
        return do_toupper(c);
    }

    /**
     * @lang{ZH}
     * 返回字符 `c` 的小写形式。区间内查快照表，区间外转发至 conf。
     * @endif
     *
     * @lang{EN}
     * Returns the lowercase form of character `c`. Serves from the snapshot table
     * for in-range values; forwards to the conf for out-of-range values.
     * @endif
     *
     * @param c @lang{ZH} 待转换的字符。 @endif @lang{EN} The character to convert. @endif
     * @return @lang{ZH} `c` 的小写形式。 @endif @lang{EN} The lowercase form of `c`. @endif
     */
    CharT tolower(CharT c) const
    {
        return do_tolower(c);
    }

    /**
     * @lang{ZH}
     * 将窄字符 `c` 拓宽为 `CharT`（O(1) 快照表查询，覆盖全部 256 个输入值）。
     *
     * **默认行为**（继承自构造时的 `ctype_conf<CharT>::widen`）：与 `std::ctype<wchar_t>::widen()`
     * 语义一致，仅保证对基本源字符集中的字符结果正确。对于多字节 locale（如 UTF-8），
     * 高字节不是独立字符，`btowc()` 在建表时对其返回 `WEOF`；`m_widen` 中对应条目存储的是
     * `WEOF` 强转为 `CharT` 的值（一个看似哨兵但实际未定义含义的值），调用方不得在此默认
     * 行为下对此类字节调用 `widen()`（`widen_seq()` 同理）。
     *
     * 用户可派生 `ctype_conf<CharT>` 并重写 `widen()` 以提供不同的映射；此时此处的快照
     * 表将在构造时从该重写重建，上述注意事项在该派生行为下不再适用。
     * @endif
     *
     * @lang{EN}
     * Widens the narrow character `c` to `CharT` (O(1) snapshot table lookup, covering
     * all 256 input values).
     *
     * **Default behaviour** (inherited from `ctype_conf<CharT>::widen` at construction
     * time): matches `std::ctype<wchar_t>::widen()` semantics — only guaranteed to be
     * correct for the basic source character set. For multi-byte locales (e.g. UTF-8),
     * high bytes are not standalone characters and `btowc()` returns `WEOF` for them at
     * table-build time; the corresponding entries of `m_widen` hold `WEOF` cast to
     * `CharT` (a sentinel-looking but unspecified value), and callers must not call
     * `widen()` on such bytes under this default. This caveat also applies to
     * `widen_seq()` (inherited from `detail::ctype_ops`).
     *
     * A user may derive from `ctype_conf<CharT>` and override `widen()` to supply a
     * different mapping; the lookup table held here is then rebuilt from that override
     * when the owning `ctype<CharT>` is constructed, and the caveat above no longer
     * applies under that derived behaviour.
     * @endif
     *
     * @param c @lang{ZH} 待拓宽的窄字符。 @endif @lang{EN} The narrow character to widen. @endif
     * @return @lang{ZH} `c` 对应的 `CharT` 值。 @endif
     * @lang{EN} The `CharT` value corresponding to `c`. @endif
     */
    CharT widen(char c) const
    {
        return m_widen[static_cast<unsigned char>(c)];
    }

    /**
     * @lang{ZH}
     * 将字符 `c` 窄化为 `char`。区间内查快照表，区间外转发至 conf。
     * @endif
     *
     * @lang{EN}
     * Narrows character `c` to `char`. Serves from the snapshot table for in-range
     * values; forwards to the conf for out-of-range values.
     * @endif
     *
     * @param c @lang{ZH} 待窄化的字符。 @endif @lang{EN} The character to narrow. @endif
     * @return @lang{ZH} 包含窄化结果的 `std::optional<char>`；若无对应单字节表示则为 `nullopt`。 @endif
     * @lang{EN} A `std::optional<char>` with the narrowed result; `nullopt` if no
     * single-byte representation exists. @endif
     */
    std::optional<char> narrow(CharT c) const
    {
        return do_narrow(c);
    }

    using detail::ctype_ops<ctype<CharT>>::narrow;

private:
    /**
     * @lang{ZH}
     * `is` 的内部实现：区间内查无锁快照表，区间外直接转发至（不可变的）conf。
     *
     * @note 此处曾有一个按字符加锁的 LRU 缓存用于慢路径，已移除。基准测试表明对本
     * facet 的实际工作负载而言是净亏损：大字母表（如 CJK）会溢出任何有界缓存并导致
     * 缓存抖动，查询依然重新计算，同时还要承担缓存和互斥锁的开销；而单个互斥锁会串行化
     * 可能共享同一 facet 的多个线程。直接调用 conf 在该场景下更快且无锁，随线程数线性
     * 扩展。（仅对极小、高度重复的工作集，缓存才有优势——本 facet 的实际场景并非如此。）
     * @endif
     *
     * @lang{EN}
     * Internal implementation of `is`: serves from the lock-free snapshot table for
     * in-range values; defers straight to the (immutable) conf for out-of-range values.
     *
     * @note There used to be a per-character locked LRU cache on this slow path; it
     * was removed. Benchmarks showed it to be a net loss for this facet's real workload:
     * large alphabets (e.g. CJK) overflow any bounded cache and thrash, so the lookup
     * recomputes anyway while also paying the cache and mutex overhead; and a single
     * mutex serialises the threads that may share one facet. Calling the conf directly
     * is faster there and lock-free, so it scales with thread count. (It loses to a
     * cache only for a tiny, highly repetitive working set — not the case here.)
     * @endif
     */
    mask do_is(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_table[static_cast<uchar_type>(c)];
        return m_obj->is(c);
    }

    /**
     * @lang{ZH}
     * `toupper` 的内部实现：区间内查快照表，区间外转发至 conf。
     * @endif
     *
     * @lang{EN}
     * Internal implementation of `toupper`: serves from the snapshot table for
     * in-range values; forwards to the conf for out-of-range values.
     * @endif
     */
    CharT do_toupper(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_toupper[static_cast<uchar_type>(c)];
        return m_obj->toupper(c);
    }

    /**
     * @lang{ZH}
     * `tolower` 的内部实现：区间内查快照表，区间外转发至 conf。
     * @endif
     *
     * @lang{EN}
     * Internal implementation of `tolower`: serves from the snapshot table for
     * in-range values; forwards to the conf for out-of-range values.
     * @endif
     */
    CharT do_tolower(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_tolower[static_cast<uchar_type>(c)];
        return m_obj->tolower(c);
    }

    /**
     * @lang{ZH}
     * `narrow` 的内部实现：区间内查快照表，区间外转发至 conf。
     * @endif
     *
     * @lang{EN}
     * Internal implementation of `narrow`: serves from the snapshot table for
     * in-range values; forwards to the conf for out-of-range values.
     * @endif
     */
    std::optional<char> do_narrow(CharT c) const
    {
        if (static_cast<uchar_type>(c) < s_len)
            return m_narrow[static_cast<uchar_type>(c)];
        return m_obj->narrow(c);
    }

private:
    /** @lang{ZH} 底层 `ctype_conf<CharT>` 实例的共享指针，供超出快照范围的字符查询使用。 @endif
     *  @lang{EN} Shared pointer to the underlying `ctype_conf<CharT>` instance, used
     *  for characters beyond the snapshot range. @endif */
    std::shared_ptr<const ctype_conf<CharT>> m_obj;

    /** @lang{ZH} 大写映射快照表，覆盖 `[0, s_len)` 区间。 @endif
     *  @lang{EN} Uppercase mapping snapshot table covering `[0, s_len)`. @endif */
    std::array<CharT, s_len>               m_toupper;

    /** @lang{ZH} 小写映射快照表，覆盖 `[0, s_len)` 区间。 @endif
     *  @lang{EN} Lowercase mapping snapshot table covering `[0, s_len)`. @endif */
    std::array<CharT, s_len>               m_tolower;

    /** @lang{ZH} 拓宽映射快照表，覆盖 `[0, s_len)` 区间。 @endif
     *  @lang{EN} Widen mapping snapshot table covering `[0, s_len)`. @endif */
    std::array<CharT, s_len>               m_widen;

    /** @lang{ZH} 窄化映射快照表，覆盖 `[0, s_len)` 区间；元素可为 `nullopt`。 @endif
     *  @lang{EN} Narrow mapping snapshot table covering `[0, s_len)`; elements may
     *  be `nullopt`. @endif */
    std::array<std::optional<char>, s_len> m_narrow;

    /** @lang{ZH} 分类掩码快照表，覆盖 `[0, s_len)` 区间。 @endif
     *  @lang{EN} Classification mask snapshot table covering `[0, s_len)`. @endif */
    std::array<mask, s_len>                m_table;
};

/**
 * @lang{ZH}
 * CTAD 推导指引：从 `shared_ptr<ctype_conf<CharT>>` 自动推导出 `ctype<CharT>`。
 * @endif
 *
 * @lang{EN}
 * CTAD deduction guide: deduces `ctype<CharT>` from a `shared_ptr<ctype_conf<CharT>>`.
 * @endif
 */
template<typename TConfPtr>
ctype(TConfPtr) -> ctype<typename TConfPtr::element_type::char_type>;
}
