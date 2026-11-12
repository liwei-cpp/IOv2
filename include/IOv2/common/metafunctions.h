// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file metafunctions.h
 * @lang{ZH}
 * 模板元编程工具和类型特征。
 * @endif
 * @lang{EN}
 * Template metaprogramming utilities and type traits.
 * @endif
 */

#pragma once
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>

namespace IOv2
{
    /**
     * @lang{ZH}
     * 用于 static_assert 的依赖型 false 值。
     *
     * 在模板中使用 static_assert(false, ...) 会导致编译器在模板定义时
     * 就报错，而不是等到模板实例化时。使用 dependent_false_v 可以延迟
     * 断言失败到模板实例化时。
     *
     * @tparam T 任意类型参数包，用于制造依赖性
     * @endif
     *
     * @lang{EN}
     * Dependent false value for static_assert.
     *
     * Using static_assert(false, ...) in a template causes the compiler to
     * error at template definition time rather than instantiation time.
     * Using dependent_false_v defers the assertion failure to instantiation.
     *
     * @tparam T Any type parameter pack, used to create dependency
     * @endif
     */
    template <typename... T>
    inline constexpr bool dependent_false_v = false;

    /**
     * @lang{ZH}
     * 用于 static_assert 的依赖型 false 值（非类型模板参数版本）。
     *
     * @tparam T 任意非类型模板参数包
     * @endif
     *
     * @lang{EN}
     * Dependent false value for static_assert (non-type template parameter version).
     *
     * @tparam T Any non-type template parameter pack
     * @endif
     */
    template <auto... T>
    inline constexpr bool dependent_false_nttp_v = false;

    /**
     * @lang{ZH}
     * 判断类型是否为"小类型"。
     *
     * 小类型定义为大小不超过两个指针大小的类型。小类型通常按值传递更高效，
     * 而大类型按引用传递更高效。
     *
     * @tparam T 要检查的类型
     * @endif
     *
     * @lang{EN}
     * Checks if a type is a "small type".
     *
     * A small type is defined as a type whose size is no larger than two pointers.
     * Small types are typically more efficient to pass by value, while large types
     * are more efficient to pass by reference.
     *
     * @tparam T The type to check
     * @endif
     */
    template <typename T>
    inline constexpr bool is_small_type_v = sizeof(T) <= 2 * sizeof(void*);

    /**
     * @lang{ZH}
     * 判断当前平台的 wchar_t 是否为 UTF-32 编码单元。
     *
     * 当且仅当 C 库声明了 `__STDC_ISO_10646__`（即承诺所有 locale 下 wchar_t 的值都是
     * Unicode 码位）、wchar_t 与 char32_t 等宽、且二者对同一字符的编码一致时为 true
     * （借助两个中文字符在编译期校验，以通用字符名书写，不受源文件编码影响）。
     * 在 glibc/Linux 上为 true；在 wchar_t
     * 为 16 位 UTF-16 的平台（如 MSVC），以及未声明该宏的平台（如 macOS，其非 UTF-8
     * locale 下 wchar_t 是本地码值）上为 false。
     *
     * 涉及 char32_t / char8_t 的宽字符处理路径依赖此前提：它们把数据当作 wchar_t
     * 序列交给 wcsxfrm / wcscoll 等 C 库函数，从而复用 wchar_t 的本地化能力。
     * @endif
     *
     * @lang{EN}
     * Whether this platform's wchar_t is a UTF-32 code unit.
     *
     * True iff the C library defines `__STDC_ISO_10646__` (its promise that
     * wchar_t values are Unicode code points in every locale), wchar_t has the
     * same width as char32_t, and the two agree on the encoding of the same
     * character (verified at compile time via two CJK characters, spelled with
     * universal character names so the source file's encoding does not matter). True on
     * glibc/Linux; false where wchar_t is a 16-bit UTF-16 code unit (e.g. MSVC)
     * and where the macro is not defined (e.g. macOS, whose wchar_t holds
     * locale-specific values outside UTF-8 locales).
     *
     * char32_t / char8_t wide-character paths rely on this assumption: they
     * hand the data to C library functions such as wcsxfrm / wcscoll as if it
     * were a wchar_t sequence, thereby reusing wchar_t's localization facilities.
     * @endif
     */
    inline constexpr bool wchar_t_is_utf32 =
#if defined(__STDC_ISO_10646__)
        (sizeof(char32_t) == sizeof(wchar_t)) &&
        (static_cast<wchar_t>(U'\u674E') == L'\u674E') &&  // 李
        (static_cast<char32_t>(L'\u4F1F') == U'\u4F1F');   // 伟
#else
        false;
#endif

    /**
     * @lang{ZH}
     * 本平台的 wchar_t 是否为 UTF-16 编码单元。
     *
     * 要求目标为 Windows（`_WIN32`），wchar_t 与 char16_t 等宽、同一字符两者编码一致，且 BMP
     * 以外的字符在宽字符串里占两个单元（代理对）。`_WIN32` 在这里的作用相当于
     * `wchar_t_is_utf32` 里的 `__STDC_ISO_10646__`：Win32 API 与 UCRT 规定 wchar_t 在任何
     * locale 下都是 UTF-16，字面量检查只能证明编译器的宽字符编码。`__STDC_ISO_10646__` 本身
     * 不能用：它承诺每个字符装进一个 wchar_t，UTF-16 平台按定义不会定义它。字面量用通用
     * 字符名书写，不受源文件编码影响。与 `wchar_t_is_utf32` 互斥；两者可能都为假（如 macOS）。
     * @endif
     *
     * @lang{EN}
     * Whether this platform's wchar_t is a UTF-16 code unit.
     *
     * Requires a Windows target (`_WIN32`), wchar_t as wide as char16_t, the two encoding the
     * same character alike, and a character outside the BMP taking two units (a surrogate
     * pair) in a wide string. `_WIN32` plays the part `__STDC_ISO_10646__` plays in
     * `wchar_t_is_utf32`: the Win32 API and the UCRT define wchar_t as UTF-16 in every
     * locale, while the literal checks only prove the compiler's wide encoding.
     * `__STDC_ISO_10646__` itself will not do: it promises every character fits in one
     * wchar_t, which a UTF-16 platform by definition does not define. The literals are
     * spelled with universal character names, so the source file's encoding does not
     * matter. Exclusive with `wchar_t_is_utf32`; both may be false (macOS, say).
     * @endif
     */
    inline constexpr bool wchar_t_is_utf16 =
#if defined(_WIN32)
        (sizeof(char16_t) == sizeof(wchar_t)) &&
        (static_cast<wchar_t>(u'\u674E') == L'\u674E') &&  // 李
        (static_cast<char16_t>(L'\u4F1F') == u'\u4F1F') && // 伟
        (sizeof(L"\U0001F600") == 3 * sizeof(wchar_t));      // a surrogate pair and the null
#else
        false;
#endif

    /**
     * @lang{ZH}
     * shared_ptr_to 概念的实现辅助结构。
     * @endif
     *
     * @lang{EN}
     * Implementation helper struct for the shared_ptr_to concept.
     * @endif
     */
    template<typename, typename>
    struct shared_ptr_to_impl : std::false_type {};

    /// @cond INTERNAL
    template<typename T, typename U>
    struct shared_ptr_to_impl<std::shared_ptr<U>, T> : std::bool_constant<std::is_convertible_v<U*, const T*>> {};
    /// @endcond

    /**
     * @lang{ZH}
     * 检查类型 P 是否为指向可转换为 T 的类型的 shared_ptr。
     *
     * @tparam P 要检查的类型（应为 std::shared_ptr 特化）
     * @tparam T 目标类型
     *
     * @par 示例
     * @code
     * struct Base {};
     * struct Derived : Base {};
     *
     * // 接受任何指向 Base 或其派生类的 shared_ptr
     * template <shared_ptr_to<Base> Ptr>
     * void process(Ptr ptr) { ... }
     *
     * auto derived_ptr = std::make_shared<Derived>();
     * process(derived_ptr);  // OK: Derived* 可转换为 Base*
     *
     * auto int_ptr = std::make_shared<int>();
     * process(int_ptr);  // 编译错误: int* 不可转换为 Base*
     * @endcode
     * @endif
     *
     * @lang{EN}
     * Checks if type P is a shared_ptr to a type convertible to T.
     *
     * @tparam P The type to check (should be a std::shared_ptr specialization)
     * @tparam T The target type
     *
     * @par Example
     * @code
     * struct Base {};
     * struct Derived : Base {};
     *
     * // Accept any shared_ptr to Base or its derived classes
     * template <shared_ptr_to<Base> Ptr>
     * void process(Ptr ptr) { ... }
     *
     * auto derived_ptr = std::make_shared<Derived>();
     * process(derived_ptr);  // OK: Derived* is convertible to Base*
     *
     * auto int_ptr = std::make_shared<int>();
     * process(int_ptr);  // Compile error: int* is not convertible to Base*
     * @endcode
     * @endif
     */
    template<typename P, typename T>
    concept shared_ptr_to = shared_ptr_to_impl<std::decay_t<P>, T>::value;

    /**
     * @lang{ZH}
     * 判断类型 T 是否为类模板 TTemplate 的某个特化。
     *
     * TTemplate 的模板参数可以带概念约束或 `requires` 子句：按 [temp.arg.template]，
     * 模板模板形参不带约束时忽略实参模板的约束，且参数包 `typename...` 可匹配任意个
     * 类型参数。但 TTemplate 只能含**类型**参数——含非类型参数或模板模板参数的模板
     * 无法匹配，用到时直接编译失败。
     *
     * 判定是精确的：不去除 cv 限定与引用（`const X<int>` 为 false），也不认派生类
     * （派生自 `X<int>` 的类型为 false）。
     *
     * @tparam T 要检查的类型
     * @tparam TTemplate 只含类型参数的类模板
     *
     * @par 示例
     * @code
     * static_assert( is_specialization_of<std::vector<int>, std::vector>);
     * static_assert(!is_specialization_of<int, std::vector>);
     * static_assert(!is_specialization_of<const std::vector<int>, std::vector>);
     * @endcode
     * @endif
     *
     * @lang{EN}
     * Checks whether type T is a specialization of class template TTemplate.
     *
     * TTemplate's parameters may carry type-constraints or a `requires` clause: per
     * [temp.arg.template], constraints on the argument template are ignored when the
     * template template parameter is unconstrained, and the pack `typename...` matches
     * any number of type parameters. TTemplate must have **type** parameters only,
     * though -- a template with a non-type or template template parameter does not
     * match, and using it is a compile error.
     *
     * The test is exact: cv-qualifiers and references are not stripped
     * (`const X<int>` yields false), and derived classes are not recognized (a type
     * derived from `X<int>` yields false).
     *
     * @tparam T The type to check
     * @tparam TTemplate A class template with type parameters only
     *
     * @par Example
     * @code
     * static_assert( is_specialization_of<std::vector<int>, std::vector>);
     * static_assert(!is_specialization_of<int, std::vector>);
     * static_assert(!is_specialization_of<const std::vector<int>, std::vector>);
     * @endcode
     * @endif
     */
    template <typename T, template <typename...> class TTemplate>
    inline constexpr bool is_specialization_of = false;

    /// @cond INTERNAL
    template <template <typename...> class TTemplate, typename... TArgs>
    inline constexpr bool is_specialization_of<TTemplate<TArgs...>, TTemplate> = true;
    /// @endcond

    /**
     * @lang{ZH}
     * `is_specialization_of` 的 concept 形式，可用于简写约束。
     *
     * 判定规则与限制同 `is_specialization_of`。
     *
     * @tparam T 要检查的类型
     * @tparam TTemplate 只含类型参数的类模板
     *
     * @par 示例
     * @code
     * template <specialization_of<std::vector> TVec>
     * void f(const TVec& v);
     * @endcode
     * @endif
     *
     * @lang{EN}
     * Concept form of `is_specialization_of`, usable as a type-constraint.
     *
     * Same rules and limitations as `is_specialization_of`.
     *
     * @tparam T The type to check
     * @tparam TTemplate A class template with type parameters only
     *
     * @par Example
     * @code
     * template <specialization_of<std::vector> TVec>
     * void f(const TVec& v);
     * @endcode
     * @endif
     */
    template <typename T, template <typename...> class TTemplate>
    concept specialization_of = is_specialization_of<T, TTemplate>;

    /**
     * @lang{ZH}
     * 判断类型 T 能否作为无序容器（`std::unordered_map` 等）在默认哈希器与判等器下的键。
     *
     * 要求 T 可判等，且 `std::hash<T>` 可默认构造、以 `const T&` 调用的结果可转换为
     * `std::size_t`。没有特化的 `std::hash<T>` 是被禁用的特化，不满足本概念。
     *
     * 不要求可拷贝：只能移动的类型同样可以作为无序容器的键。需要拷贝键的使用者应另行约束。
     *
     * @tparam T 要检查的类型
     * @endif
     *
     * @lang{EN}
     * Whether type T can serve as the key of an unordered container (such as
     * `std::unordered_map`) under the default hasher and key-equality predicate.
     *
     * Requires T to be equality comparable, and `std::hash<T>` to be default-constructible
     * with its call on a `const T&` yielding something convertible to `std::size_t`. A
     * `std::hash<T>` that has not been specialized is a disabled specialization and does
     * not satisfy this concept.
     *
     * Copyability is not required: a move-only type can be an unordered container's key as
     * well. Users that copy keys must constrain that separately.
     *
     * @tparam T The type to check
     * @endif
     */
    template <typename T>
    concept unordered_key = std::equality_comparable<T>
                         && requires (const T& v) {
                                { std::hash<T>{}(v) } -> std::convertible_to<std::size_t>;
                            };

    /**
     * @lang{ZH}
     * 判定一个输入迭代器能否后退一步。
     *
     * 标准的迭代器分类里没有"单趟但可后退"这一档：`std::bidirectional_iterator` 蕴含
     * `forward_iterator`，因而要求 multi-pass 保证与真正的引用语义。这两条对
     * `stamp_input_iterator<ichannel_iterator<...>>` 都不成立——它的流位置在共享的
     * iochannel 里（递增一个副本会改变原件看到的内容），`operator*` 也只返回纯右值；
     * 但它靠内部记录的回退日志确实提供了 `operator--`（经 `putbackc` 把字符放回）。
     *
     * 于是"需要回退"的算法不能拿 `std::bidirectional_iterator` 当代理：这样写会漏掉上面
     * 那种迭代器，而给它贴双向标签又是谎报。本概念直接命名"能后退"这一能力本身，让约束
     * 只要求真正用到的东西。
     *
     * @note 双向迭代器被完全包含，无需另行析取：`std::bidirectional_iterator` 的定义中
     *       已含 `{ --i } -> std::same_as<I&>`，且它蕴含 `input_iterator`。
     * @warning 满足本概念**不代表**可以交给 `std::advance` / `std::ranges::advance` 后退。
     *          前者按 C++17 的 `iterator_category` 分派、后者要求 `bidirectional_iterator`
     *          概念，负数实参在两边都是前置条件违反（libstdc++ 开启断言时直接 `abort`）。
     *          请直接调用 `--`。
     *
     * @tparam I 待检测的迭代器类型
     * @endif
     *
     * @lang{EN}
     * Whether an input iterator can step one position backwards.
     *
     * The standard iterator taxonomy has no category for "single-pass yet able to step
     * back": `std::bidirectional_iterator` implies `forward_iterator`, and so demands the
     * multi-pass guarantee and true reference semantics. Neither holds for
     * `stamp_input_iterator<ichannel_iterator<...>>`, whose stream position lives in the
     * shared iochannel (incrementing a copy changes what the original observes) and whose
     * `operator*` yields a prvalue -- yet it does provide `operator--`, putting characters
     * back from an internal log.
     *
     * An algorithm that needs to back up therefore cannot use `std::bidirectional_iterator`
     * as a proxy: doing so excludes such iterators, while tagging them bidirectional would
     * be a false claim. This concept names the capability itself, so a constraint asks only
     * for what is actually used.
     *
     * @note Bidirectional iterators are subsumed and need no separate disjunct:
     *       `std::bidirectional_iterator` already requires `{ --i } -> std::same_as<I&>`
     *       and implies `input_iterator`.
     * @warning Satisfying this concept does **not** mean the iterator may be moved backwards
     *          with `std::advance` / `std::ranges::advance`. The former dispatches on the
     *          C++17 `iterator_category`, the latter requires the `bidirectional_iterator`
     *          concept; a negative argument violates the precondition of both (and aborts
     *          outright once libstdc++ assertions are on). Call `--` directly.
     *
     * @tparam I The iterator type under inspection
     * @endif
     */
    template <typename I>
    concept steppable_back = std::input_iterator<I>
                          && requires (I i) { { --i } -> std::same_as<I&>; };
}
