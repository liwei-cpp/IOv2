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
}
