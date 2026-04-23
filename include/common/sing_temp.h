/**
 * @file sing_temp.h
 * @lang{ZH}
 * 基于 CRTP 的单例模板，用于静态初始化期间的单例管理。
 * @endif
 * @lang{EN}
 * CRTP-based singleton template for singleton management during static initialization.
 * @endif
 */

#pragma once
#include <array>
#include <cstdlib>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * 基于 CRTP（奇异递归模板模式）的单例模板基类。
 *
 * 此类设计用于在静态初始化期间（main() 之前）创建单例对象。
 * 通过引用计数机制确保单例在所有翻译单元中只被构造和析构一次。
 *
 * @tparam T 派生类类型（CRTP 模式）
 *
 * @note 此实现假设 init 对象仅在 main() 之前的静态初始化期间创建。
 *       静态初始化是单线程的，因此不需要原子操作。
 *       如果需要在 main() 之后动态创建 init 对象，必须添加线程同步机制。
 *
 * @par 示例
 * @code
 * // 定义单例类
 * class __my_singleton : public some_base_class
 *                      , public sing_temp<__my_singleton>
 * {
 *     friend sing_temp<__my_singleton>;
 *
 * private:
 *     __my_singleton() { ... }  // 私有构造函数
 *     ~__my_singleton() { ... }
 *
 *     __my_singleton(const __my_singleton&) = delete;
 *     __my_singleton& operator=(const __my_singleton&) = delete;
 * };
 *
 * // 在头文件中声明静态初始化器和引用
 * static __my_singleton::init _my_singleton_init;
 * static __my_singleton& my_singleton = *__my_singleton::ptr();
 *
 * // 使用单例
 * my_singleton.doSomething();
 * @endcode
 * @endif
 *
 * @lang{EN}
 * CRTP (Curiously Recurring Template Pattern) based singleton template base class.
 *
 * This class is designed for creating singleton objects during static initialization
 * (before main()). Uses reference counting to ensure the singleton is constructed
 * and destructed exactly once across all translation units.
 *
 * @tparam T The derived class type (CRTP pattern)
 *
 * @note This implementation assumes init objects are only created during static
 *       initialization before main(). Static initialization is single-threaded,
 *       so atomic operations are not needed.
 *       If init objects need to be created dynamically after main(), thread
 *       synchronization mechanisms must be added.
 *
 * @par Example
 * @code
 * // Define the singleton class
 * class __my_singleton : public some_base_class
 *                      , public sing_temp<__my_singleton>
 * {
 *     friend sing_temp<__my_singleton>;
 *
 * private:
 *     __my_singleton() { ... }  // Private constructor
 *     ~__my_singleton() { ... }
 *
 *     __my_singleton(const __my_singleton&) = delete;
 *     __my_singleton& operator=(const __my_singleton&) = delete;
 * };
 *
 * // Declare static initializer and reference in header
 * static __my_singleton::init _my_singleton_init;
 * static __my_singleton& my_singleton = *__my_singleton::ptr();
 *
 * // Use the singleton
 * my_singleton.doSomething();
 * @endcode
 * @endif
 */
template <typename T>
class sing_temp
{
public:
    /**
     * @lang{ZH}
     * 用于管理单例生命周期的初始化器类。
     *
     * 每个翻译单元应声明一个静态的 init 对象。第一个被构造的 init 对象
     * 将构造单例，最后一个被析构的 init 对象将析构单例。
     *
     * @warning 如果派生类定义了在构造期间使用的静态成员，它们必须在任何
     *          init 对象之前初始化。使用 `inline static` 可以确保这一点。
     * @endif
     *
     * @lang{EN}
     * Initializer class for managing singleton lifetime.
     *
     * Each translation unit should declare a static init object. The first init
     * object constructed will construct the singleton, and the last init object
     * destructed will destruct the singleton.
     *
     * @warning If the derived class defines static members used during construction,
     *          they must be initialized BEFORE any init object. Use `inline static`
     *          to ensure this.
     * @endif
     */
    struct init
    {
        init()
        {
            static_assert(std::is_class_v<T>, "sing_temp: T must be a class type");
            static_assert(!std::is_abstract_v<T>, "sing_temp: T cannot be abstract");

            auto& count = RefCount();
            if (count++ == 0)
            {
                T* ptr = sing_temp::ptr();
                try {
                    new (ptr) T();
                } catch (...) {
                    // sing_temp is designed for static initialization before main().
                    // If T() throws, the singleton cannot be constructed and there is
                    // no safe recovery path. Abort explicitly to avoid leaving the
                    // reference count in an inconsistent state.
                    std::abort();
                }
            }
        }

        ~init()
        {
            auto& count = RefCount();
            if (--count == 0)
            {
                T* ptr = sing_temp::ptr();
                ptr->~T();
            }
        }

        /**
         * @lang{ZH}
         * 获取引用计数。
         * @return 引用计数的引用
         * @endif
         *
         * @lang{EN}
         * Get the reference count.
         * @return Reference to the reference count
         * @endif
         */
        [[nodiscard]] static auto& RefCount()
        {
            static unsigned count{0};
            return count;
        }

        init(const init&) = delete;
        init& operator=(const init&) = delete;
        init(init&&) = delete;
        init& operator=(init&&) = delete;
    };

protected:
    sing_temp() = default;
    ~sing_temp() = default;

public:
    sing_temp(const sing_temp&) = delete;
    sing_temp& operator=(const sing_temp&) = delete;
    sing_temp(sing_temp&&) = delete;
    sing_temp& operator=(sing_temp&&) = delete;

    /**
     * @lang{ZH}
     * 获取指向单例对象的指针。
     *
     * @return 指向单例对象的指针
     *
     * @warning 调用此函数前必须确保至少有一个 init 对象存在，
     *          否则返回的指针指向未构造的内存，解引用会导致未定义行为。
     * @endif
     *
     * @lang{EN}
     * Get a pointer to the singleton object.
     *
     * @return Pointer to the singleton object
     *
     * @warning At least one init object must exist before calling this function,
     *          otherwise the returned pointer points to unconstructed memory
     *          and dereferencing it causes undefined behavior.
     * @endif
     */
    [[nodiscard]] static T* ptr()
    {
        static_assert(std::is_class_v<T>, "sing_temp: T must be a class type");
        static_assert(!std::is_abstract_v<T>, "sing_temp: T cannot be abstract");

        alignas(T) static std::array<char, sizeof(T)> sing_buf;
        return reinterpret_cast<T*>(sing_buf.data());
    }
};
}
