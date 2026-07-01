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
#include <common/iov2_export.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * 基于 CRTP（奇异递归模板模式）的单例模板基类。
 *
 * 此类设计用于在静态初始化期间（main() 之前）创建单例对象。
 * 在头文件中定义唯一的 inline init 对象，确保单例在整个程序中只被构造和析构一次。
 *
 * @tparam T 派生类类型（CRTP 模式）
 *
 * @note 此实现假设唯一的 inline init 对象在 main() 之前的静态初始化期间创建。
 *       静态初始化是单线程的；由于全程序只有一个 inline init 对象，构造与析构
 *       各发生一次，因此不需要引用计数或原子操作。
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
 * // 在头文件中声明 inline 初始化器和引用（inline = 全程序唯一实体）
 * inline __my_singleton::init _my_singleton_init;
 * inline __my_singleton& my_singleton = *__my_singleton::ptr();
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
 * (before main()). A single inline init object defined in the header ensures the
 * singleton is constructed and destructed exactly once across the whole program.
 *
 * @tparam T The derived class type (CRTP pattern)
 *
 * @note This implementation assumes the single inline init object is created during
 *       static initialization before main(). Static initialization is single-threaded,
 *       and because there is exactly one inline init object program-wide, construction
 *       and destruction each happen once, so no reference counting or atomics are needed.
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
 * // Declare inline initializer and reference in header (inline = one entity program-wide)
 * inline __my_singleton::init _my_singleton_init;
 * inline __my_singleton& my_singleton = *__my_singleton::ptr();
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
     * 在头文件中声明唯一的 inline init 对象。因为 inline 变量全程序只有一个
     * 实体，所以其构造函数构造单例、析构函数析构单例，各发生一次，无需引用计数。
     *
     * @warning 必须用 `inline` 声明 init 对象（而非 `static`）。`static` 会在每个
     *          翻译单元生成一份 init，导致单例被重复构造/析构，属于未定义行为。
     * @warning 如果派生类定义了在构造期间使用的静态成员，它们必须在 init 对象
     *          之前初始化。使用 `inline static` 可以确保这一点。
     * @endif
     *
     * @lang{EN}
     * Initializer class for managing singleton lifetime.
     *
     * Declare exactly one inline init object in the header. Because an inline
     * variable is a single entity across the whole program, its constructor
     * constructs the singleton and its destructor destroys it -- each exactly
     * once -- so no reference counting is needed.
     *
     * @warning The init object MUST be declared `inline`, not `static`. A `static`
     *          init produces one object per translation unit, which would construct
     *          and destroy the singleton multiple times (undefined behavior).
     * @warning If the derived class defines static members used during construction,
     *          they must be initialized BEFORE the init object. Use `inline static`
     *          to ensure this.
     * @endif
     */
    struct init
    {
        init()
        {
            static_assert(std::is_class_v<T>, "sing_temp: T must be a class type");
            static_assert(!std::is_abstract_v<T>, "sing_temp: T cannot be abstract");

            // A single inline init object exists program-wide, so this runs exactly
            // once -- no reference count is needed to guard against re-entry.
            try {
                sing_temp::instance() = ::new (sing_temp::storage()) T();
            } catch (...) {
                // sing_temp is designed for static initialization before main().
                // If T() throws, the singleton cannot be constructed and there is
                // no safe recovery path. Abort explicitly.
                std::abort();
            }
        }

        ~init()
        {
            // Mirror of the constructor: runs exactly once, destroys the singleton.
            sing_temp::instance()->~T();
            sing_temp::instance() = nullptr;
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
        return instance();
    }

private:
    /**
     * @lang{ZH}
     * 获取指向单例对象的规范指针的引用。
     *
     * 该指针由构造单例的 placement-new 表达式产出并在此保存；对单例的所有访问都
     * 经由它，因此无需 std::launder。其值在构造前 / 析构后为 nullptr。
     *
     * @return 对该规范指针的引用（便于 init 写入与清空）
     * @endif
     *
     * @lang{EN}
     * Get a reference to the canonical pointer to the singleton object.
     *
     * The pointer is produced and stored by the placement-new that constructs the
     * singleton; every access goes through it, so std::launder is never needed. Its
     * value is nullptr before construction / after destruction.
     *
     * @return Reference to the canonical pointer (so init can set and clear it)
     * @endif
     */
    [[nodiscard]] static T*& instance() noexcept
    {
        static T* p = nullptr;
        return p;
    }

    /**
     * @lang{ZH}
     * 获取单例的原始、正确对齐的存储。
     *
     * 仅用作 placement-new 的目标；对象本身一律通过 instance() 持有的指针访问。
     *
     * @return 指向存储首字节的指针
     * @endif
     *
     * @lang{EN}
     * Get the raw, correctly-aligned storage for the singleton.
     *
     * Used only as the placement-new target; the object itself is always accessed
     * through the pointer held by instance().
     *
     * @return Pointer to the first byte of the storage
     * @endif
     */
    [[nodiscard]] static void* storage() noexcept
    {
        alignas(T) static std::array<std::byte, sizeof(T)> sing_buf;
        return sing_buf.data();
    }
};
}
