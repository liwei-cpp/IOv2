/**
 * @file facet_common.h
 * @lang{ZH}
 * facet 基础设施的公共类型定义与工具。
 *
 * 本文件提供以下核心组件：
 * - `abs_ft`：所有 facet 的抽象基类，持有运行时 facet id。
 * - `base_ft<TFacet>`：以 facet 模板为参数的中间基类，用于建立 facet 继承层次。
 * - `ft_basic<TFacet<CharT>>`：为 `(facet 模板, 字符类型)` 对提供静态类型键机制的具体基类。
 * - `facet_create_rule<...>`、`facet_create_pack<...>` 及相关 traits：描述 facet 构造规则的
 *   元编程工具，用于 locale 的批量 facet 构造。
 * - `out_of_wchar_range`：检测字符值是否超出 `wchar_t` 表示范围的工具函数。
 * @endif
 *
 * @lang{EN}
 * Common type definitions and utilities for the facet infrastructure.
 *
 * This file provides the following core components:
 * - `abs_ft`: Abstract base class for all facets, carrying the runtime facet id.
 * - `base_ft<TFacet>`: Intermediate base class parameterized on the facet template,
 *   used to establish the facet inheritance hierarchy.
 * - `ft_basic<TFacet<CharT>>`: Concrete base class providing the static type-key
 *   mechanism for a `(facet template, character type)` pair.
 * - `facet_create_rule<...>`, `facet_create_pack<...>`, and related traits: Metaprogramming
 *   utilities describing facet construction rules, used for batch facet construction in a locale.
 * - `out_of_wchar_range`: Utility function to detect whether a character value is
 *   outside the representable range of `wchar_t`.
 * @endif
 */

#pragma once
#include <bit>
#include <concepts>
#include <cstddef>
#include <cwchar>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * 所有 facet 的抽象基类。
 *
 * 持有运行时 facet id，用于在丢失具体类型信息时（如经由 `abs_ft&`，例如 `locale::involve`）
 * 通过运行时值将 facet 键入 locale。
 *
 * 该类不可复制也不可移动。
 * @endif
 *
 * @lang{EN}
 * Abstract base class for all facets.
 *
 * Carries the runtime facet id, which is used to key a facet into a locale by
 * its runtime value when the concrete type is no longer known (e.g., via
 * `abs_ft&` in `locale::involve`).
 *
 * This class is neither copyable nor movable.
 * @endif
 */
struct abs_ft
{
    /**
     * @lang{ZH}
     * 构造函数，以运行时 facet id 初始化实例。
     * @endif
     *
     * @lang{EN}
     * Constructor; initializes the instance with the runtime facet id.
     * @endif
     *
     * @param id
     * @lang{ZH} 此 facet 实例的运行时唯一标识符。 @endif
     * @lang{EN} The runtime unique identifier for this facet instance. @endif
     */
    explicit abs_ft(size_t id)
        : m_id(id) {}

    abs_ft(const abs_ft&) = delete;
    abs_ft& operator=(const abs_ft&) = delete;
    abs_ft(abs_ft&&) = delete;
    abs_ft& operator=(abs_ft&&) = delete;

    virtual ~abs_ft() = default;

    /**
     * @lang{ZH}
     * 实例访问器：读取此 facet 携带的 id。
     *
     * 该 id 用于在具体类型不再可知时（如 `abs_ft&`，例如 `locale::involve`），
     * 通过运行时值将 facet 键入 locale。
     *
     * @note `ft_basic` 引入了一个**静态**的同名 `id()`，即每类型 facet 键（`TF::id()`）；
     * 在派生 facet 上，该静态成员会隐藏此实例访问器。这是刻意为之且无害的，**仅因为**
     * 每个 facet 在构造时都以自身的静态 `id()` 初始化 `m_id`，使二者始终返回相同的值。
     * 请勿破坏此不变量——不可构造 `m_id` 与其静态 `id()` 不一致的 facet，否则静态键
     * 与此实例访问器将静默地产生分歧，导致 locale facet 查找异常。
     * @endif
     *
     * @lang{EN}
     * Instance accessor: reads the id carried by this facet, used to key it
     * into a locale by its runtime value when the concrete type is no longer
     * known (an `abs_ft&`, e.g. `locale::involve`).
     *
     * @note `ft_basic` introduces a *static*, same-named `id()` that is the
     * per-type facet key (`TF::id()`); on derived facets that static hides this
     * accessor. This is deliberate and harmless ONLY because every facet seeds
     * `m_id` from its own static `id()` at construction, so both always return the
     * same value. Do not break that invariant -- never construct a facet whose
     * `m_id` differs from its static `id()` -- or the static key and this accessor
     * would silently diverge and locale facet lookup would misbehave.
     * @endif
     *
     * @return
     * @lang{ZH} 此 facet 实例的运行时 id。 @endif
     * @lang{EN} The runtime id of this facet instance. @endif
     */
    [[nodiscard]] size_t id() const noexcept { return m_id; }

private:
    /** @lang{ZH} 此 facet 实例的运行时唯一标识符。 @endif
     *  @lang{EN} The runtime unique identifier for this facet instance. @endif */
    const size_t m_id;
};

/**
 * @lang{ZH}
 * 以 facet 模板为参数的中间基类。
 *
 * 作为 `abs_ft` 与具体 `ft_basic` 特化之间的桥接层，用于建立以 facet 模板
 * 为标识的继承层次结构，使同一 facet 模板的不同字符类型特化可共享同一中间基类。
 * @endif
 *
 * @lang{EN}
 * Intermediate base class parameterized on the facet template.
 *
 * Acts as a bridge layer between `abs_ft` and a concrete `ft_basic`
 * specialization, establishing an inheritance hierarchy keyed on the facet
 * template. This allows different character-type specializations of the same
 * facet template to share a common intermediate base.
 * @endif
 *
 * @tparam TFacet
 * @lang{ZH} facet 模板，例如 `ctype`、`collate` 等。 @endif
 * @lang{EN} The facet template, e.g. `ctype`, `collate`, etc. @endif
 */
template <template<typename> class TFacet>
class base_ft : public abs_ft
{
public:
    using abs_ft::abs_ft;
};

template <typename TFacet> class ft_basic;

/**
 * @lang{ZH}
 * 为 `(facet 模板, 字符类型)` 对提供静态类型键机制的具体基类。
 *
 * 每个 `ft_basic<TFacet<CharT>>` 特化维护一个每类型唯一的 `s_id` 静态变量，
 * 静态成员函数 `id()` 返回其地址作为稳定的编译期 facet 键。构造函数自动以此值
 * 初始化 `abs_ft::m_id`，保证静态键与实例访问器始终一致。
 * @endif
 *
 * @lang{EN}
 * Concrete base class providing the static type-key mechanism for a
 * `(facet template, character type)` pair.
 *
 * Each `ft_basic<TFacet<CharT>>` specialization maintains a per-type unique
 * `s_id` static variable; the static member function `id()` returns its address
 * as a stable, compile-time facet key. The constructor automatically seeds
 * `abs_ft::m_id` with this value, ensuring the static key and the instance
 * accessor always agree.
 * @endif
 *
 * @tparam TFacet
 * @lang{ZH} 完整的 facet 类型，形如 `SomeFacet<CharT>`。 @endif
 * @lang{EN} The full facet type, e.g. `SomeFacet<CharT>`. @endif
 */
template <template<typename> class TFacet, typename CharT>
class ft_basic<TFacet<CharT>> : public base_ft<TFacet>
{
    // id() returns std::bit_cast<size_t>(&s_id); std::bit_cast is well-formed only
    // when sizeof(To) == sizeof(From), so the pointer and size_t must be equal in
    // size (not merely "pointer fits into size_t").
    static_assert(sizeof(void*) == sizeof(size_t),
                  "ft_basic::id() uses std::bit_cast<size_t>(pointer), which is "
                  "well-formed only when sizeof(void*) == sizeof(size_t)");
public:
    /** @lang{ZH} 此 facet 特化所绑定的字符类型。 @endif
     *  @lang{EN} The character type bound to this facet specialization. @endif */
    using char_type = CharT;
public:
    /**
     * @lang{ZH}
     * 转发构造函数。以此类型的静态 `id()` 值初始化 `abs_ft::m_id`，并将其余参数
     * 转发给 `base_ft<TFacet>`。
     * @endif
     *
     * @lang{EN}
     * Forwarding constructor. Seeds `abs_ft::m_id` with this type's static `id()`
     * value and forwards the remaining arguments to `base_ft<TFacet>`.
     * @endif
     */
    template <typename... T>
        requires std::constructible_from<base_ft<TFacet>, size_t, T...>
    ft_basic(T&&... args)
        : base_ft<TFacet>(id(), std::forward<T>(args)...) {}

    /**
     * @lang{ZH}
     * 静态每类型 facet 键：返回此 `(facet 模板, 字符类型)` 对稳定且唯一的 id，
     * 取自每类型 `s_id` 变量的地址。此值为编译期键，用于按类型定位 facet（`TF::id()`）。
     * 构造函数以此值初始化 `abs_ft::m_id`，保证其与 `abs_ft::id()` 实例访问器
     * 始终保持一致；参见 `abs_ft::id()` 上的说明以了解共名的理由。
     * @endif
     *
     * @lang{EN}
     * Static per-type facet key: a stable, unique id for this
     * `(facet template, char type)` pair, taken from the address of the per-type
     * `s_id`. This is the compile-time key used to locate facets by type
     * (`TF::id()`). The constructor seeds `abs_ft::m_id` with this value, keeping
     * it in agreement with the `abs_ft::id()` instance accessor; see the note on
     * `abs_ft::id()` for why the shared name is intentional and safe.
     * @endif
     *
     * @return
     * @lang{ZH} 此 facet 类型唯一的运行时 id 值。 @endif
     * @lang{EN} The unique runtime id value for this facet type. @endif
     */
    static size_t id() noexcept { return std::bit_cast<size_t>(&s_id); }
private:
    /**
     * @lang{ZH}
     * 每类型唯一标识符变量。`id()` 返回其地址，因此跨 DSO 的 facet 标识依赖动态
     * 链接器将所有副本合并为单一运行时地址。以下部署模式会静默地破坏此机制，导致本应
     * 相同的 facet 在不同 DSO 中得到不同的 id：
     * - 编译任何实例化 `ft_basic<...>` 的翻译单元时使用 `-fvisibility=hidden` 而不重新
     *   导出 `s_id`。
     * - 链接 DSO 时使用 `-Bsymbolic` / `-Bsymbolic-functions`，导致内部引用绑定至 DSO
     *   本地副本。
     * - 使用 `RTLD_LOCAL`（默认值）`dlopen()` 插件。
     * - 跨 Windows DLL 边界共享 facet 实例（PE 没有 ELF 弱符号统一机制，模板在每个 DLL
     *   中独立实例化）。
     *
     * 若有上述任一需求，应将 `id()` 改为基于值的标识（如 `typeid(TFacet).name()` 的哈希），
     * 而非地址。
     * @endif
     *
     * @lang{EN}
     * Per-type unique identity variable. Because `id()` returns `&s_id`, cross-DSO
     * facet identity depends on the dynamic linker merging all copies of `s_id` to a
     * single runtime address. The following deployment patterns silently break this
     * and yield per-DSO ids for what should be the same facet:
     * - Building any TU that instantiates `ft_basic<...>` with `-fvisibility=hidden`
     *   without re-exporting `s_id`.
     * - Linking a DSO with `-Bsymbolic` / `-Bsymbolic-functions`, which binds
     *   internal references to the DSO-local copy.
     * - `dlopen()`ing a plugin with `RTLD_LOCAL` (the default).
     * - Sharing facet instances across Windows DLL boundaries (PE has no equivalent
     *   of ELF weak-symbol unification; templates are re-instantiated per DLL).
     *
     * If any of the above is needed, switch `id()` to a value-based identity
     * (e.g. hash of `typeid(TFacet).name()`) rather than an address.
     * @endif
     */
    inline static const void* s_id = nullptr;
};

/**
 * @lang{ZH}
 * 描述单条 facet 构造规则的标签类型。
 *
 * 模板参数列表编码了构造该 facet 所需的类型信息，供 locale 的批量 facet
 * 构造机制在编译期解析。
 * @endif
 *
 * @lang{EN}
 * Tag type describing a single facet construction rule.
 *
 * The template argument list encodes the type information needed to construct
 * the facet, for use by the locale's batch facet-construction mechanism at
 * compile time.
 * @endif
 */
template <typename...>
struct facet_create_rule;

/**
 * @lang{ZH}
 * 若干条 facet 构造规则组成的有序包。
 *
 * locale 的批量构造机制按包内规则的顺序依次实例化各 facet，直至包为空。
 * @endif
 *
 * @lang{EN}
 * An ordered pack of facet construction rules.
 *
 * The locale's batch-construction mechanism instantiates each facet in order
 * until the pack is empty.
 * @endif
 */
template <typename...>
struct facet_create_pack;

/**
 * @lang{ZH}
 * 类型特征：判断 `T` 是否为包含至少一个类型参数的 `facet_create_rule` 特化。
 * @endif
 *
 * @lang{EN}
 * Type trait: `true` if `T` is a `facet_create_rule` specialization with at least
 * one type argument.
 * @endif
 *
 * @tparam T
 * @lang{ZH} 待检测的类型。 @endif
 * @lang{EN} The type to inspect. @endif
 */
template <typename T>
constexpr static bool is_nonempty_facet_create_rule = false;

template <typename... T>
constexpr static bool is_nonempty_facet_create_rule<facet_create_rule<T...>> = (sizeof...(T) != 0);

/**
 * @lang{ZH}
 * 类型特征：判断 `T` 是否为包含至少一个元素的 `facet_create_pack` 特化。
 * @endif
 *
 * @lang{EN}
 * Type trait: `true` if `T` is a `facet_create_pack` specialization with at least
 * one element.
 * @endif
 *
 * @tparam T
 * @lang{ZH} 待检测的类型。 @endif
 * @lang{EN} The type to inspect. @endif
 */
template <typename T>
constexpr static bool is_nonempty_facet_create_pack = false;

template <typename... T>
constexpr static bool is_nonempty_facet_create_pack<facet_create_pack<T...>> = (sizeof...(T) != 0);

/**
 * @lang{ZH}
 * 类型特征：获取 `facet_create_pack` 特化中的规则数量；对非 pack 类型返回 0。
 * @endif
 *
 * @lang{EN}
 * Type trait: the number of rules in a `facet_create_pack` specialization;
 * returns 0 for non-pack types.
 * @endif
 *
 * @tparam T
 * @lang{ZH} 待检测的类型。 @endif
 * @lang{EN} The type to inspect. @endif
 */
template <typename T>
constexpr static size_t facet_create_pack_size = 0;

template <typename... T>
constexpr static size_t facet_create_pack_size<facet_create_pack<T...>> = sizeof...(T);

/**
 * @lang{ZH}
 * 提取非空 `facet_create_pack` 中第一个规则的类型，以 `type` 成员类型给出。
 * @endif
 *
 * @lang{EN}
 * Extracts the type of the first rule in a non-empty `facet_create_pack`,
 * exposed as the member type `type`.
 * @endif
 *
 * @tparam T
 * @lang{ZH} 非空的 `facet_create_pack` 特化。 @endif
 * @lang{EN} A non-empty `facet_create_pack` specialization. @endif
 */
template <typename T>
    requires (facet_create_pack_size<T> > 0)
struct facet_create_pack_head;

template <typename H, typename... T>
struct facet_create_pack_head<facet_create_pack<H, T...>>
{
    using type = H;
};

/**
 * @lang{ZH}
 * 移除非空 `facet_create_pack` 的第一个规则后剩余的包，以 `type` 成员类型给出。
 * @endif
 *
 * @lang{EN}
 * The remaining pack after removing the first rule of a non-empty
 * `facet_create_pack`, exposed as the member type `type`.
 * @endif
 *
 * @tparam T
 * @lang{ZH} 非空的 `facet_create_pack` 特化。 @endif
 * @lang{EN} A non-empty `facet_create_pack` specialization. @endif
 */
template <typename T>
    requires (facet_create_pack_size<T> > 0)
struct facet_create_pack_tail;

template <typename H, typename... T>
struct facet_create_pack_tail<facet_create_pack<H, T...>>
{
    using type = facet_create_pack<T...>;
};

/**
 * @lang{ZH}
 * 检测字符值是否超出 `wchar_t` 的可表示范围。
 *
 * 用于在任何 facet 的 API 边界处保护 C 库的 `*_l` 宽字符函数
 * （`iswctype_l`、`towupper_l`、`towlower_l`、`wctob` 等），因为 C 标准规定，
 * 对不能以 `wchar_t` 表示的输入，这些函数的行为是由实现定义的。
 *
 * 仅对 `CharT == char32_t` 且平台上 `char32_t` 覆盖范围严格大于 `wchar_t` 的情况
 * 有实际意义（例如 Linux：`wchar_t` 为有符号 32 位，`WCHAR_MAX = 0x7FFFFFFF`；
 * `char32_t` 为无符号 32 位）。对其他所有字符类型（`wchar_t`、`char`、`char8_t`、
 * `char16_t`），在所有支持的平台上输入始终可以 `wchar_t` 表示，因此此函数静态地
 * 等同于无操作。
 *
 * **`char32_t` 在 Linux 上的精确边界**：本函数恰好对 `char32_t` 值域的上半部分
 * `[0x80000000, 0xFFFFFFFF]` 返回 `true`，因为那里 `WCHAR_MAX == 0x7FFFFFFF`。
 * 整个有效 Unicode 码点空间 U+0000..U+10FFFF 远低于此截止值，因此本守卫从不拒绝
 * 合法 Unicode；它仅过滤调用方可能被重新解释为 `char32_t` 的非 Unicode 位模式。
 * 有意通过 `char32_t` 通道传递任意 32 位整数的调用方不应将 `out_of_wchar_range`
 * 作为 Unicode 有效性检查使用。
 *
 * `CharT` 被约束为五种标准 C++ 字符类型，以确保对非字符整型或浮点类型的误用
 * 在编译期报错，而非静默返回 `false`。
 * @endif
 *
 * @lang{EN}
 * Returns `true` if `c` has no `wchar_t` representation. Used to guard the
 * C `*_l` wide-character functions (`iswctype_l`, `towupper_l`, `towlower_l`,
 * `wctob`, ...) at any facet's API boundary, since the C standard says
 * their behaviour is implementation-defined for inputs that are not
 * representable as a `wchar_t`.
 *
 * Only non-trivial for `CharT == char32_t` on platforms where `char32_t`
 * covers a strictly wider range than `wchar_t` (e.g. Linux: signed
 * 32-bit `wchar_t` with `WCHAR_MAX = 0x7FFFFFFF`, vs unsigned 32-bit
 * `char32_t`). For every other character type (`wchar_t`, `char`, `char8_t`,
 * `char16_t`) the input is always representable as `wchar_t` on every
 * supported platform, and the function is statically a no-op.
 *
 * **Precise boundary for `char32_t` on Linux**: this function returns `true`
 * exactly for the upper half of the `char32_t` range, `[0x80000000, 0xFFFFFFFF]`,
 * because `WCHAR_MAX == 0x7FFFFFFF` there. The entire valid Unicode code-point
 * space U+0000..U+10FFFF lies well below this cutoff, so the guard never rejects
 * legal Unicode; it only filters out non-Unicode bit patterns that a caller may
 * have reinterpreted as `char32_t`. Callers that intentionally pass arbitrary
 * 32-bit integers through a `char32_t` channel should not rely on
 * `out_of_wchar_range` as a Unicode-validity check.
 *
 * `CharT` is constrained to the five standard C++ character types so
 * that misuse with non-character integral or floating types (where
 * the `wchar_t`-range question is meaningless) fails at compile time
 * instead of silently returning `false`.
 * @endif
 *
 * @tparam CharT
 * @lang{ZH}
 * 字符类型，须为 `char`、`wchar_t`、`char8_t`、`char16_t` 或 `char32_t` 之一。
 * @endif
 * @lang{EN}
 * The character type; must be one of `char`, `wchar_t`, `char8_t`, `char16_t`,
 * or `char32_t`.
 * @endif
 *
 * @param c
 * @lang{ZH} 要检测的字符值。 @endif
 * @lang{EN} The character value to test. @endif
 *
 * @return
 * @lang{ZH}
 * 若 `c` 无法以 `wchar_t` 表示则返回 `true`；否则返回 `false`。
 * 对 `char32_t` 之外的所有字符类型始终返回 `false`。
 * @endif
 * @lang{EN}
 * `true` if `c` cannot be represented as `wchar_t`; `false` otherwise.
 * Always returns `false` for every character type other than `char32_t`.
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, char>     ||
             std::is_same_v<CharT, wchar_t>  ||
             std::is_same_v<CharT, char8_t>  ||
             std::is_same_v<CharT, char16_t> ||
             std::is_same_v<CharT, char32_t>
constexpr bool out_of_wchar_range(CharT c) noexcept
{
    if constexpr (std::is_same_v<CharT, char32_t>)
        return c > static_cast<char32_t>(WCHAR_MAX);
    else
        return false;
}
}
