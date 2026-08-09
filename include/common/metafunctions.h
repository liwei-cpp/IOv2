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
     * 当且仅当 wchar_t 与 char32_t 等宽、且二者对同一字符的编码一致时为 true
     * （借助两个中文字符在编译期校验）。在 glibc/Linux 上为 true；在 wchar_t
     * 为 16 位 UTF-16 的平台（如 MSVC）上为 false。
     *
     * 涉及 char32_t / char8_t 的宽字符处理路径依赖此前提：它们把数据当作 wchar_t
     * 序列交给 wcsxfrm / wcscoll 等 C 库函数，从而复用 wchar_t 的本地化能力。
     * @endif
     *
     * @lang{EN}
     * Whether this platform's wchar_t is a UTF-32 code unit.
     *
     * True iff wchar_t has the same width as char32_t and the two agree on the
     * encoding of the same character (verified at compile time via two CJK
     * characters). True on glibc/Linux; false where wchar_t is a 16-bit UTF-16
     * code unit (e.g. MSVC).
     *
     * char32_t / char8_t wide-character paths rely on this assumption: they
     * hand the data to C library functions such as wcsxfrm / wcscoll as if it
     * were a wchar_t sequence, thereby reusing wchar_t's localization facilities.
     * @endif
     */
    inline constexpr bool wchar_t_is_utf32 =
        (sizeof(char32_t) == sizeof(wchar_t)) &&
        (static_cast<wchar_t>(U'李') == L'李') &&
        (static_cast<char32_t>(L'伟') == U'伟');

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
     * 判定一个输入迭代器能否后退一步。
     *
     * 标准的迭代器分类里没有"单趟但可后退"这一档：`std::bidirectional_iterator` 蕴含
     * `forward_iterator`，因而要求 multi-pass 保证与真正的引用语义。这两条对
     * `stamp_input_iterator<istreambuf_iterator<...>>` 都不成立——它的流位置在共享的
     * streambuf 里（递增一个副本会改变原件看到的内容），`operator*` 也只返回纯右值；
     * 但它靠内部记录的回退日志确实提供了 `operator--`（经 `sputbackc` 把字符放回）。
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
     * `stamp_input_iterator<istreambuf_iterator<...>>`, whose stream position lives in the
     * shared streambuf (incrementing a copy changes what the original observes) and whose
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
