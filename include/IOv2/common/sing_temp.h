// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

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
#include <IOv2/common/iov2_export.h>

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
 * 在头文件中定义唯一的 inline init 对象，确保单例在整个程序中只被构造一次；
 * 进程退出时对单例做什么由派生类在构造时登记的退出钩子决定（见 `exit_hook`）。
 *
 * @tparam T 派生类类型（CRTP 模式）
 *
 * @note 此实现假设唯一的 inline init 对象在 main() 之前的静态初始化期间创建。
 *       静态初始化是单线程的；由于全程序只有一个 inline init 对象，构造发生一次、
 *       退出逻辑执行一次，因此不需要引用计数或原子操作。
 *
 * @par 示例
 * @code
 * // 定义单例类（默认：退出时析构）
 * class my_singleton_t : public some_base_class
 *                      , public sing_temp<my_singleton_t>
 * {
 *     friend sing_temp<my_singleton_t>;
 *
 * private:
 *     my_singleton_t() { ... }  // 私有构造函数
 *     ~my_singleton_t() { ... }
 *
 *     my_singleton_t(const my_singleton_t&) = delete;
 *     my_singleton_t& operator=(const my_singleton_t&) = delete;
 * };
 *
 * // 登记退出钩子：退出时只刷新、不析构（输出流一类的单例）
 * class my_stream_t : public some_ostream_base
 *                   , public sing_temp<my_stream_t>
 * {
 *     friend sing_temp<my_stream_t>;
 *
 * private:
 *     my_stream_t()
 *         : sing_temp<my_stream_t>([](my_stream_t* p) noexcept { try { p->flush(); } catch (...) {} })
 *     {}
 *     ...
 * };
 *
 * // 在头文件中声明 inline 初始化器和引用（inline = 全程序唯一实体）
 * inline my_singleton_t::init _my_singleton_init;
 * inline my_singleton_t& my_singleton = *my_singleton_t::ptr();
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
 * singleton is constructed exactly once across the whole program; what happens to it
 * at process exit is decided by the exit hook the derived class registers at
 * construction (see `exit_hook`).
 *
 * @tparam T The derived class type (CRTP pattern)
 *
 * @note This implementation assumes the single inline init object is created during
 *       static initialization before main(). Static initialization is single-threaded,
 *       and because there is exactly one inline init object program-wide, construction
 *       happens once and the exit logic runs once, so no reference counting or atomics
 *       are needed.
 *
 * @par Example
 * @code
 * // Define the singleton class (default: destroyed at exit)
 * class my_singleton_t : public some_base_class
 *                      , public sing_temp<my_singleton_t>
 * {
 *     friend sing_temp<my_singleton_t>;
 *
 * private:
 *     my_singleton_t() { ... }  // Private constructor
 *     ~my_singleton_t() { ... }
 *
 *     my_singleton_t(const my_singleton_t&) = delete;
 *     my_singleton_t& operator=(const my_singleton_t&) = delete;
 * };
 *
 * // Register an exit hook: flush at exit, never destroy (output-stream-like singletons)
 * class my_stream_t : public some_ostream_base
 *                   , public sing_temp<my_stream_t>
 * {
 *     friend sing_temp<my_stream_t>;
 *
 * private:
 *     my_stream_t()
 *         : sing_temp<my_stream_t>([](my_stream_t* p) noexcept { try { p->flush(); } catch (...) {} })
 *     {}
 *     ...
 * };
 *
 * // Declare inline initializer and reference in header (inline = one entity program-wide)
 * inline my_singleton_t::init _my_singleton_init;
 * inline my_singleton_t& my_singleton = *my_singleton_t::ptr();
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
     * @brief 退出钩子的类型：以单例指针调用、不抛出的函数指针。
     *
     * 派生类通过 `sing_temp(exit_hook)` 构造函数登记它，唯一的 init 对象析构时以
     * `T*` 调用一次；对单例做什么——析构、刷新、放着不管——由钩子自己决定。未登记
     * （默认构造 `sing_temp`）时 init 对象析构单例（`~T()`，之后 `ptr()` 返回 nullptr）。
     * 钩子不析构单例时，单例的存储（静态缓冲）与其持有的堆内存保持可达、由操作系统在
     * 进程结束时回收，退出后 `ptr()` 与既有引用仍然有效——这正是 libstdc++ 对
     * `std::cout` 等标准流对象的处理方式，也是需要在退出阶段被其它线程继续使用的
     * 进程级单例应当采用的方式。无捕获的 `noexcept` lambda 可直接转换为本类型。
     *
     * 选用函数指针而非 `std::function`：常量初始化、平凡析构，不引入新的静态初始化 /
     * 析构顺序依赖。
     * @endif
     *
     * @lang{EN}
     * @brief Type of the exit hook: a non-throwing function pointer called with the
     * singleton pointer.
     *
     * The derived class registers it through the `sing_temp(exit_hook)` constructor;
     * it is called once with a `T*` when the single init object is destroyed, and what
     * it does to the singleton -- destroy it, flush it, leave it alone -- is up to the
     * hook. Without a hook (default-constructed `sing_temp`) the init object destroys
     * the singleton (`~T()`, after which `ptr()` returns nullptr). When the hook does
     * not destroy the singleton, its storage (a static buffer) and the heap it owns stay
     * reachable and are reclaimed by the operating system at process end, and `ptr()`
     * and existing references remain valid after exit begins -- exactly how libstdc++
     * treats `std::cout` and the other standard stream objects, and the right choice for
     * any process-wide singleton that other threads may still use during exit. A
     * captureless `noexcept` lambda converts to this type directly.
     *
     * A function pointer rather than `std::function`: constant-initialized and trivially
     * destructible, so it adds no static initialization / destruction ordering edge.
     * @endif
     */
    using exit_hook = void (*)(T*) noexcept;

    /**
     * @lang{ZH}
     * 用于管理单例生命周期的初始化器类。
     *
     * 在头文件中声明唯一的 inline init 对象。因为 inline 变量全程序只有一个
     * 实体，所以其构造函数构造单例、析构函数执行退出逻辑，各发生一次，无需引用计数。
     *
     * @warning 必须用 `inline` 声明 init 对象（而非 `static`）。`static` 会在每个
     *          翻译单元生成一份 init，导致单例被重复构造、退出逻辑被重复执行，
     *          属于未定义行为。
     * @warning 如果派生类定义了在构造期间使用的静态成员，它们必须在 init 对象
     *          之前初始化。使用 `inline static` 可以确保这一点。
     * @endif
     *
     * @lang{EN}
     * Initializer class for managing singleton lifetime.
     *
     * Declare exactly one inline init object in the header. Because an inline
     * variable is a single entity across the whole program, its constructor
     * constructs the singleton and its destructor runs the exit logic -- each
     * exactly once -- so no reference counting is needed.
     *
     * @warning The init object MUST be declared `inline`, not `static`. A `static`
     *          init produces one object per translation unit, which would construct
     *          the singleton and run the exit logic multiple times (undefined behavior).
     * @warning If the derived class defines static members used during construction,
     *          they must be initialized BEFORE the init object. Use `inline static`
     *          to ensure this.
     * @endif
     */
    struct init
    {
        init() noexcept
        {
            static_assert(std::is_class_v<T>, "sing_temp: T must be a class type");
            static_assert(!std::is_abstract_v<T>, "sing_temp: T cannot be abstract");

            // A single inline init object exists program-wide, so this runs exactly
            // once -- no reference count is needed to guard against re-entry.
            try {
                // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
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
            // Mirror of the constructor: runs exactly once. A registered hook decides
            // what happens to the singleton; without one it is destroyed.
            if (sing_temp::s_exit_hook != nullptr) {
                sing_temp::s_exit_hook(sing_temp::instance());
            } else {
                sing_temp::instance()->~T();
                sing_temp::instance() = nullptr;
            }
        }

        init(const init&) = delete;
        init& operator=(const init&) = delete;
        init(init&&) = delete;
        init& operator=(init&&) = delete;
    };

private:
    friend T;

    /**
     * @lang{ZH}
     * @brief 默认构造：不登记退出钩子，init 对象析构时析构单例。
     * @endif
     * @lang{EN}
     * @brief Default construction: no exit hook; the init object destroys the singleton.
     * @endif
     */
    sing_temp() = default;

    /**
     * @lang{ZH}
     * @brief 登记退出钩子。
     *
     * 由派生类的构造函数在其初始化列表里调用；派生类的构造发生在 init 对象的构造函数
     * 内部，因此钩子一定先于 init 对象的析构登记完毕。
     *
     * @param hook 退出钩子，见 `exit_hook`。
     * @endif
     *
     * @lang{EN}
     * @brief Registers the exit hook.
     *
     * Called from the derived class's constructor in its initializer list; the derived
     * class is constructed inside the init object's constructor, so the hook is always
     * registered before the init object is destroyed.
     *
     * @param hook The exit hook, see `exit_hook`.
     * @endif
     */
    explicit sing_temp(exit_hook hook) noexcept
    {
        s_exit_hook = hook;
    }

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
     * @note 未登记退出钩子时，init 对象析构后本函数返回 nullptr；
     *       登记的钩子不析构单例时，本函数继续返回有效指针。
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
     * @note Without an exit hook this returns nullptr once the init object has been
     *       destroyed; when the registered hook does not destroy the singleton it keeps
     *       returning a valid pointer.
     * @endif
     */
    [[nodiscard]] static T* ptr() noexcept
    {
        return instance();
    }

private:
    /**
     * @lang{ZH}
     * @brief 已登记的退出钩子；nullptr 表示未登记。
     *
     * 常量初始化的平凡对象，无动态初始化、不登记 atexit；由 `sing_temp(exit_hook)`
     * 写入、`init::~init()` 读取。
     * @endif
     *
     * @lang{EN}
     * @brief The registered exit hook; nullptr when none is registered.
     *
     * A constant-initialized trivial object: no dynamic initialization, no atexit
     * registration; written by `sing_temp(exit_hook)`, read by `init::~init()`.
     * @endif
     */
    inline static exit_hook s_exit_hook = nullptr;

    /**
     * @lang{ZH}
     * 获取指向单例对象的规范指针的引用。
     *
     * 该指针由构造单例的 placement-new 表达式产出并在此保存；对单例的所有访问都
     * 经由它，因此无需 std::launder。其值在构造前为 nullptr；未登记退出钩子时
     * 析构后亦为 nullptr，登记了钩子时 init 不清空它。
     *
     * @return 对该规范指针的引用（便于 init 写入与清空）
     * @endif
     *
     * @lang{EN}
     * Get a reference to the canonical pointer to the singleton object.
     *
     * The pointer is produced and stored by the placement-new that constructs the
     * singleton; every access goes through it, so std::launder is never needed. Its
     * value is nullptr before construction, and after destruction when no exit hook
     * is registered; with a hook, init leaves it untouched.
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
