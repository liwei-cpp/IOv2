/**
 * @file runtime_cvt.h
 * @lang{ZH}
 * 运行时类型擦除转换器定义文件。
 * 本文件通过经典的虚函数类型擦除模式，将任意满足 `io_converter` 约束的转换器
 * 包装为统一的 `runtime_cvt<TDevice, TInt>` 类型，使转换器可以在运行时多态地存储与操作。
 * 主要包含以下三层结构：
 * - `abs_runtime_cvt_imp`：定义类型擦除接口的抽象基类；
 * - `runtime_cvt_imp`：持有具体转换器内核的实现类；
 * - `runtime_cvt`：面向用户的类型擦除转换器包装类。
 * @endif
 *
 * @lang{EN}
 * Runtime type-erased converter definition file.
 * This file wraps any converter satisfying the `io_converter` constraint into a uniform
 * `runtime_cvt<TDevice, TInt>` type using the classic virtual-function type-erasure pattern,
 * enabling converters to be stored and used polymorphically at runtime.
 * The implementation consists of three layers:
 * - `abs_runtime_cvt_imp`: abstract base class defining the type-erased interface;
 * - `runtime_cvt_imp`: concrete implementation class holding a specific converter kernel;
 * - `runtime_cvt`: user-facing type-erased converter wrapper.
 * @endif
 */
#pragma once

#include <common/defs.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * 内部实现细节命名空间。
 * @endif
 *
 * @lang{EN}
 * Namespace for internal implementation details.
 * @endif
 */
namespace detail
{
/**
 * @lang{ZH}
 * 预构造的异常指针，用于在 `runtime_cvt` 持有空实例时抛出。
 * 预先构造可避免在可能无法分配内存的场景中重复创建异常对象。
 * @endif
 *
 * @lang{EN}
 * Pre-constructed exception pointer thrown when a `runtime_cvt` holds a null implementation.
 * Pre-constructing it avoids repeated allocation in contexts where memory may be unavailable.
 * @endif
 */
inline const std::exception_ptr runtime_cvt_null_err =
    std::make_exception_ptr(cvt_error("runtime_cvt: null instance"));
}

/**
 * @lang{ZH}
 * 类型擦除转换器实现的抽象基类。
 * 声明了转换器所需的完整虚函数接口，使 `runtime_cvt` 可通过基类指针操作任意具体转换器。
 * 此类禁止拷贝赋值和移动，避免切片问题；多态拷贝应通过 `clone()` 完成。
 * @endif
 *
 * @lang{EN}
 * Abstract base class for the type-erased converter implementation.
 * Declares the complete virtual interface required by a converter, allowing `runtime_cvt`
 * to operate on any concrete converter through a base-class pointer.
 * Copy assignment and move are disabled to prevent slicing; polymorphic copying is
 * performed via `clone()`.
 * @endif
 *
 * @tparam TDevice 底层设备类型，须满足 `io_device`。
 *                / The underlying device type, must satisfy `io_device`.
 * @tparam TInt    转换器内部数据类型。 / The converter's internal data type.
 */
template <io_device TDevice, typename TInt>
class abs_runtime_cvt_imp
{
public:
    using device_type = TDevice;
    using internal_type = TInt;
    using external_type = typename device_type::char_type;

public:
    abs_runtime_cvt_imp() = default;
    abs_runtime_cvt_imp(const abs_runtime_cvt_imp&) = default;
    abs_runtime_cvt_imp(abs_runtime_cvt_imp&&) = delete;
    abs_runtime_cvt_imp& operator=(const abs_runtime_cvt_imp&) = delete;
    abs_runtime_cvt_imp& operator=(abs_runtime_cvt_imp&&) = delete;
    virtual ~abs_runtime_cvt_imp() = default;

public:
    /**
     * @lang{ZH}
     * 多态拷贝：克隆当前实现对象，返回指向新实例的 `unique_ptr`。
     * 若具体内核不支持拷贝构造，实现类应抛出 `cvt_error`。
     * @endif
     *
     * @lang{EN}
     * Polymorphic copy: clone the current implementation object and return a
     * `unique_ptr` to the new instance.
     * If the concrete kernel does not support copy construction, the implementation
     * should throw `cvt_error`.
     * @endif
     *
     * @return 指向新克隆实例的 `unique_ptr`。
     *         / A `unique_ptr` to the newly cloned instance.
     */
    virtual std::unique_ptr<abs_runtime_cvt_imp> clone() const & = 0;

    /// @lang{ZH} 返回底层设备的引用。 @endif @lang{EN} Return a reference to the underlying device. @endif
    virtual device_type& device() = 0;
    /// @lang{ZH} 分离底层设备，重置 I/O 状态为 `neutral`，不抛出异常。 @endif @lang{EN} Detach the underlying device and reset I/O status to `neutral`, without throwing. @endif
    virtual std::pair<device_type, std::exception_ptr> detach() noexcept = 0;
    /// @lang{ZH} 将设备绑定到此转换器，同时重置 I/O 状态为 `neutral`。 @endif @lang{EN} Attach a device to this converter and reset I/O status to `neutral`. @endif
    virtual void attach(device_type&& dev = device_type{}) = 0;
    /// @lang{ZH} 向转换器应用行为策略。 @endif @lang{EN} Apply a behavior policy to the converter. @endif
    virtual void adjust(const cvt_behavior& acc) = 0;
    /// @lang{ZH} 提取转换器的内部状态。 @endif @lang{EN} Extract the converter's internal status. @endif
    virtual void retrieve(cvt_status& acc) const = 0;
    /// @lang{ZH} 查询是否已到达数据末尾。 @endif @lang{EN} Query whether the end of data has been reached. @endif
    virtual bool is_eof() = 0;

    /// @lang{ZH} 建立初始 I/O 状态，返回确定的方向。 @endif @lang{EN} Establish the initial I/O state and return the determined direction. @endif
    virtual io_status bos() = 0;
    /// @lang{ZH} 通知转换器进入主内容阶段。 @endif @lang{EN} Notify the converter to transition into the main content phase. @endif
    virtual void main_cont_beg() = 0;

    /// @lang{ZH} 从转换器读取至多 `to_max` 个元素，返回实际读取数量。 @endif @lang{EN} Read up to `to_max` elements from the converter; return the number actually read. @endif
    virtual size_t get(internal_type* to, size_t to_max) = 0;
    /// @lang{ZH} 向转换器写入 `to_size` 个元素。 @endif @lang{EN} Write `to_size` elements into the converter. @endif
    virtual void put(const internal_type* to, size_t to_size) = 0;
    /// @lang{ZH} 将所有缓冲数据刷出转换器。 @endif @lang{EN} Flush all buffered data out of the converter. @endif
    virtual void flush() = 0;

    /// @lang{ZH} 返回当前流位置。 @endif @lang{EN} Return the current stream position. @endif
    [[nodiscard]] virtual size_t tell() const = 0;
    /// @lang{ZH} 将流定位到指定绝对位置。 @endif @lang{EN} Seek the stream to the specified absolute position. @endif
    virtual void seek(size_t pos) = 0;
    /// @lang{ZH} 将流定位到指定相对位置。 @endif @lang{EN} Seek the stream to the specified relative position. @endif
    virtual void rseek(size_t pos) = 0;
    /// @lang{ZH} 切换至输入（读取）模式。 @endif @lang{EN} Switch to input (reading) mode. @endif
    virtual void switch_to_get() = 0;
    /// @lang{ZH} 切换至输出（写入）模式。 @endif @lang{EN} Switch to output (writing) mode. @endif
    virtual void switch_to_put() = 0;
};

/**
 * @lang{ZH}
 * 持有具体转换器内核的类型擦除实现类。
 * 继承自 `abs_runtime_cvt_imp`，通过转发调用将所有虚函数接口委托给内部的内核实例。
 * 对于内核不支持的可选能力（读取、写入、定位、I/O 切换），在编译期通过 `if constexpr`
 * 检测，若调用到不支持的接口则在运行时抛出 `cvt_error`。
 * `switch_to_get` 和 `switch_to_put` 通过 `m_io_status` 追踪当前方向，实现幂等调用。
 * @endif
 *
 * @lang{EN}
 * Type-erased implementation class that holds a concrete converter kernel.
 * Inherits from `abs_runtime_cvt_imp` and forwards all virtual interface calls to the
 * internal kernel instance.
 * Optional capabilities (read, write, positioning, I/O switch) that the kernel may not
 * support are detected at compile time via `if constexpr`; calling an unsupported interface
 * throws `cvt_error` at runtime.
 * `switch_to_get` and `switch_to_put` track the current direction via `m_io_status`
 * to make repeated calls idempotent.
 * @endif
 *
 * @tparam KernelType 被包装的具体转换器内核类型，须满足 `io_converter`。
 *                   / The concrete converter kernel type being wrapped, must satisfy `io_converter`.
 */
template <io_converter KernelType>
class runtime_cvt_imp : public abs_runtime_cvt_imp<typename KernelType::device_type, typename KernelType::internal_type>
{
    using BT = abs_runtime_cvt_imp<typename KernelType::device_type, typename KernelType::internal_type>;
public:
    using device_type = typename KernelType::device_type;
    using internal_type = typename KernelType::internal_type;
    using external_type = typename device_type::char_type;

public:
    /**
     * @lang{ZH}
     * 从内核的 const 引用拷贝构造，I/O 状态初始化为 `neutral`。
     * @endif
     *
     * @lang{EN}
     * Copy-constructs from a const kernel reference; I/O status is initialized to `neutral`.
     * @endif
     *
     * @param kernel 待包装的转换器内核（拷贝）。 / The converter kernel to wrap (copied).
     */
    runtime_cvt_imp(const KernelType& kernel)
        : m_kernel(kernel)
        , m_io_status(io_status::neutral) {}

    /**
     * @lang{ZH}
     * 从内核的右值引用移动构造，I/O 状态初始化为 `neutral`。
     * @endif
     *
     * @lang{EN}
     * Move-constructs from a kernel rvalue reference; I/O status is initialized to `neutral`.
     * @endif
     *
     * @param kernel 待包装的转换器内核（移动）。 / The converter kernel to wrap (moved).
     */
    runtime_cvt_imp(KernelType&& kernel)
        : m_kernel(std::move(kernel))
        , m_io_status(io_status::neutral) {}

public:
    /**
     * @lang{ZH}
     * 多态拷贝实现。若内核类型支持拷贝构造则克隆当前实例，否则抛出 `cvt_error`。
     * @endif
     *
     * @lang{EN}
     * Polymorphic copy implementation. Clones the current instance if the kernel type
     * supports copy construction; otherwise throws `cvt_error`.
     * @endif
     *
     * @return 指向新克隆实例的 `unique_ptr`。
     *         / A `unique_ptr` to the newly cloned instance.
     * @throws cvt_error 若内核不支持拷贝构造。 / If the kernel does not support copy construction.
     */
    std::unique_ptr<BT> clone() const & override
    {
        if constexpr (std::copy_constructible<KernelType>)
            return std::make_unique<runtime_cvt_imp>(*this);
        else
            throw cvt_error("runtime_cvt fail: kernel does not support copy construction");
    }

    device_type& device() override
    {
        return m_kernel.device();
    }

    std::pair<device_type, std::exception_ptr> detach() noexcept override
    {
        auto result = m_kernel.detach();
        m_io_status = io_status::neutral;
        return result;
    }

    void attach(device_type&& dev = device_type{}) override
    {
        m_kernel.attach(std::move(dev));
        m_io_status = io_status::neutral;
    }

    void adjust(const cvt_behavior& acc) override
    {
        return m_kernel.adjust(acc);
    }

    void retrieve(cvt_status& acc) const override
    {
        return m_kernel.retrieve(acc);
    }

    /**
     * @lang{ZH}
     * 若内核不支持读取操作，抛出 `cvt_error`，否则委托给内核。
     * @endif
     *
     * @lang{EN}
     * Throws `cvt_error` if the kernel does not support read operations; otherwise delegates to the kernel.
     * @endif
     */
    bool is_eof() override
    {
        if constexpr(!cvt_cpt::support_get<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support get");
        else
            return m_kernel.is_eof();
    }

    io_status bos() override
    {
        m_io_status = m_kernel.bos();
        return m_io_status;
    }

    void main_cont_beg() override
    {
        return m_kernel.main_cont_beg();
    }

    /**
     * @lang{ZH}
     * 若内核不支持读取操作，抛出 `cvt_error`，否则委托给内核。
     * @endif
     *
     * @lang{EN}
     * Throws `cvt_error` if the kernel does not support read operations; otherwise delegates to the kernel.
     * @endif
     */
    size_t get(internal_type* to, size_t to_max) override
    {
        if constexpr(!cvt_cpt::support_get<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support get");
        else
            return m_kernel.get(to, to_max);
    }

    /**
     * @lang{ZH}
     * 若内核不支持写入操作，抛出 `cvt_error`，否则委托给内核。
     * @endif
     *
     * @lang{EN}
     * Throws `cvt_error` if the kernel does not support write operations; otherwise delegates to the kernel.
     * @endif
     */
    void put(const internal_type* to, size_t to_size) override
    {
        if constexpr(!cvt_cpt::support_put<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support put");
        else
            return m_kernel.put(to, to_size);
    }

    /**
     * @lang{ZH}
     * 若内核不支持写入操作，抛出 `cvt_error`，否则委托给内核。
     * @endif
     *
     * @lang{EN}
     * Throws `cvt_error` if the kernel does not support write operations; otherwise delegates to the kernel.
     * @endif
     */
    void flush() override
    {
        if constexpr(!cvt_cpt::support_put<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support put");
        else
            return m_kernel.flush();
    }

    /**
     * @lang{ZH}
     * 若内核不支持流定位操作，抛出 `cvt_error`，否则委托给内核。
     * @endif
     *
     * @lang{EN}
     * Throws `cvt_error` if the kernel does not support positioning; otherwise delegates to the kernel.
     * @endif
     */
    [[nodiscard]] size_t tell() const override
    {
        if constexpr (!cvt_cpt::support_positioning<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support positioning");
        else
            return m_kernel.tell();
    }

    /**
     * @lang{ZH}
     * 若内核不支持流定位操作，抛出 `cvt_error`，否则委托给内核。
     * @endif
     *
     * @lang{EN}
     * Throws `cvt_error` if the kernel does not support positioning; otherwise delegates to the kernel.
     * @endif
     */
    void seek(size_t pos) override
    {
        if constexpr (!cvt_cpt::support_positioning<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support positioning");
        else
            m_kernel.seek(pos);
    }

    /**
     * @lang{ZH}
     * 若内核不支持流定位操作，抛出 `cvt_error`，否则委托给内核。
     * @endif
     *
     * @lang{EN}
     * Throws `cvt_error` if the kernel does not support positioning; otherwise delegates to the kernel.
     * @endif
     */
    void rseek(size_t pos) override
    {
        if constexpr (!cvt_cpt::support_positioning<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support positioning");
        else
            m_kernel.rseek(pos);
    }

    /**
     * @lang{ZH}
     * 切换至输入模式。若已处于输入模式则直接返回（幂等）；
     * 若内核不支持 I/O 切换，抛出 `cvt_error`。
     * @endif
     *
     * @lang{EN}
     * Switch to input mode. Returns immediately if already in input mode (idempotent).
     * Throws `cvt_error` if the kernel does not support I/O switching.
     * @endif
     */
    void switch_to_get() override
    {
        if (m_io_status == io_status::input) return;
        if constexpr (!cvt_cpt::support_io_switch<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support I/O switch");
        else
        {
            m_kernel.switch_to_get();
            m_io_status = io_status::input;
        }
    }

    /**
     * @lang{ZH}
     * 切换至输出模式。若已处于输出模式则直接返回（幂等）；
     * 若内核不支持 I/O 切换，抛出 `cvt_error`。
     * @endif
     *
     * @lang{EN}
     * Switch to output mode. Returns immediately if already in output mode (idempotent).
     * Throws `cvt_error` if the kernel does not support I/O switching.
     * @endif
     */
    void switch_to_put() override
    {
        if (m_io_status == io_status::output) return;
        if constexpr (!cvt_cpt::support_io_switch<KernelType>)
            throw cvt_error("runtime_cvt fail: kernel does not support I/O switch");
        else
        {
            m_kernel.switch_to_put();
            m_io_status = io_status::output;
        }
    }

private:
    KernelType m_kernel;
    io_status m_io_status; ///< 追踪当前 I/O 方向，用于 switch_to_get/switch_to_put 的幂等检查。
                           ///< Tracks current I/O direction for idempotent switch_to_get/switch_to_put.
};

/**
 * @lang{ZH}
 * 面向用户的运行时类型擦除转换器。
 * 通过 `unique_ptr<abs_runtime_cvt_imp>` 持有具体转换器实现，使具体类型对外不可见。
 * 支持拷贝（通过 `clone()` 实现多态拷贝）与移动语义。
 * 所有成员函数在转发调用前均检查内部指针是否为空：
 * 若为空则抛出异常（`noexcept` 函数 `detach` 除外，其在空指针时调用 `std::terminate`）。
 * @endif
 *
 * @lang{EN}
 * User-facing runtime type-erased converter.
 * Holds the concrete converter implementation via `unique_ptr<abs_runtime_cvt_imp>`,
 * keeping the concrete type invisible to callers.
 * Supports both copy (via polymorphic `clone()`) and move semantics.
 * All member functions check the internal pointer before forwarding the call:
 * a null pointer throws an exception, except for the `noexcept` function `detach`,
 * which calls `std::terminate` on null.
 * @endif
 *
 * @tparam TDevice 底层设备类型，须满足 `io_device`。
 *                / The underlying device type, must satisfy `io_device`.
 * @tparam TInt    转换器内部数据类型。 / The converter's internal data type.
 */
template <io_device TDevice, typename TInt>
class runtime_cvt
{
public:
    using device_type = TDevice;
    using internal_type = TInt;
    using external_type = typename device_type::char_type;

public:
    /**
     * @lang{ZH}
     * 从任意兼容的转换器内核构造。接受左值（拷贝）和右值（移动）两种形式。
     * 约束要求：
     * - `KernelType` 不得为 `runtime_cvt` 自身（防止此构造函数拦截拷贝/移动构造）；
     * - `KernelType` 须满足 `io_converter`；
     * - 若传入左值引用，则内核类型须支持拷贝构造（因内核将被存储于堆上）。
     * @endif
     *
     * @lang{EN}
     * Constructs from any compatible converter kernel, accepting both lvalue (copy)
     * and rvalue (move) forms.
     * Constraints:
     * - `KernelType` must not be `runtime_cvt` itself (prevents hijacking copy/move construction);
     * - `KernelType` must satisfy `io_converter`;
     * - If an lvalue reference is passed, the kernel type must be copy-constructible
     *   (because the kernel will be stored on the heap).
     * @endif
     *
     * @tparam KernelType 转换器内核类型。 / The converter kernel type.
     * @param kernel 待包装的转换器内核。 / The converter kernel to wrap.
     */
    template <typename KernelType>
        requires (!std::is_same_v<std::remove_cvref_t<KernelType>, runtime_cvt> &&
                  io_converter<std::remove_cvref_t<KernelType>> &&
                  (!std::is_lvalue_reference_v<KernelType> ||
                   std::copy_constructible<std::remove_cvref_t<KernelType>>))
    runtime_cvt(KernelType&& kernel)
        : m_ptr(std::make_unique<runtime_cvt_imp<std::remove_cvref_t<KernelType>>>(std::forward<KernelType>(kernel))) {}

    /**
     * @lang{ZH}
     * 拷贝构造：通过 `clone()` 对内部实现进行多态拷贝。若源对象为空则拷贝结果也为空。
     * @endif
     *
     * @lang{EN}
     * Copy constructor: performs a polymorphic copy of the internal implementation via `clone()`.
     * If the source is null, the copy is also null.
     * @endif
     *
     * @param val 源 `runtime_cvt` 实例。 / The source `runtime_cvt` instance.
     */
    runtime_cvt(const runtime_cvt& val)
        : m_ptr(val.m_ptr ? val.m_ptr->clone() : nullptr) {}

    /**
     * @lang{ZH}
     * 拷贝赋值：通过 `clone()` 对内部实现进行多态拷贝。若源对象为空则目标也置为空。
     * @endif
     *
     * @lang{EN}
     * Copy assignment: performs a polymorphic copy of the internal implementation via `clone()`.
     * If the source is null, the target is set to null.
     * @endif
     *
     * @param val 源 `runtime_cvt` 实例。 / The source `runtime_cvt` instance.
     * @return 对 `*this` 的引用。 / A reference to `*this`.
     */
    runtime_cvt& operator=(const runtime_cvt& val)
    {
        if (this != &val)
            m_ptr = val.m_ptr ? val.m_ptr->clone() : nullptr;
        return *this;
    }

    runtime_cvt(runtime_cvt&&) = default;
    runtime_cvt& operator=(runtime_cvt&&) = default;
    ~runtime_cvt() = default;

public:
    /// @lang{ZH} 返回底层设备的引用；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Return a reference to the underlying device; throws if the internal pointer is null. @endif
    device_type& device()
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->device();
    }

    /**
     * @lang{ZH}
     * 分离底层设备，重置 I/O 状态为 `neutral`。
     * 此函数声明为 `noexcept`：若内部指针为空，调用 `std::terminate` 而非抛出异常。
     * @endif
     *
     * @lang{EN}
     * Detach the underlying device and reset I/O status to `neutral`.
     * This function is `noexcept`: if the internal pointer is null, it calls `std::terminate`
     * rather than throwing.
     * @endif
     *
     * @return 包含设备和任意待传播异常的 pair。
     *         / A pair containing the device and any pending exception to propagate.
     */
    std::pair<device_type, std::exception_ptr> detach() noexcept
    {
        if (!m_ptr) std::terminate();
        return m_ptr->detach();
    }

    /// @lang{ZH} 将设备绑定到此转换器；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Attach a device to this converter; throws if the internal pointer is null. @endif
    void attach(device_type&& dev = device_type{})
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        m_ptr->attach(std::move(dev));
    }

    /// @lang{ZH} 向转换器应用行为策略；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Apply a behavior policy to the converter; throws if the internal pointer is null. @endif
    void adjust(const cvt_behavior& acc)
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->adjust(acc);
    }

    /// @lang{ZH} 提取转换器的内部状态；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Extract the converter's internal status; throws if the internal pointer is null. @endif
    void retrieve(cvt_status& acc) const
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->retrieve(acc);
    }

    /// @lang{ZH} 查询是否已到达数据末尾；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Query whether the end of data has been reached; throws if the internal pointer is null. @endif
    bool is_eof()
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->is_eof();
    }

    /// @lang{ZH} 建立初始 I/O 状态；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Establish the initial I/O state; throws if the internal pointer is null. @endif
    io_status bos()
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->bos();
    }

    /// @lang{ZH} 通知转换器进入主内容阶段；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Notify the converter to enter the main content phase; throws if the internal pointer is null. @endif
    void main_cont_beg()
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->main_cont_beg();
    }

    /// @lang{ZH} 从转换器读取至多 `to_max` 个元素；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Read up to `to_max` elements from the converter; throws if the internal pointer is null. @endif
    size_t get(internal_type* to, size_t to_max)
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->get(to, to_max);
    }

    /// @lang{ZH} 向转换器写入 `to_size` 个元素；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Write `to_size` elements into the converter; throws if the internal pointer is null. @endif
    void put(const internal_type* to, size_t to_size)
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->put(to, to_size);
    }

    /// @lang{ZH} 将所有缓冲数据刷出转换器；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Flush all buffered data out of the converter; throws if the internal pointer is null. @endif
    void flush()
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->flush();
    }

    /// @lang{ZH} 返回当前流位置；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Return the current stream position; throws if the internal pointer is null. @endif
    [[nodiscard]] size_t tell() const
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->tell();
    }

    /// @lang{ZH} 将流定位到指定绝对位置；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Seek the stream to the specified absolute position; throws if the internal pointer is null. @endif
    void seek(size_t pos)
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        m_ptr->seek(pos);
    }

    /// @lang{ZH} 将流定位到指定相对位置；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Seek the stream to the specified relative position; throws if the internal pointer is null. @endif
    void rseek(size_t pos)
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        m_ptr->rseek(pos);
    }

    /// @lang{ZH} 切换至输入（读取）模式；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Switch to input (reading) mode; throws if the internal pointer is null. @endif
    void switch_to_get()
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->switch_to_get();
    }

    /// @lang{ZH} 切换至输出（写入）模式；若内部指针为空则抛出异常。 @endif
    /// @lang{EN} Switch to output (writing) mode; throws if the internal pointer is null. @endif
    void switch_to_put()
    {
        if (!m_ptr) std::rethrow_exception(detail::runtime_cvt_null_err);
        return m_ptr->switch_to_put();
    }
private:
    std::unique_ptr<abs_runtime_cvt_imp<TDevice, TInt>> m_ptr;
};

/**
 * @lang{ZH}
 * `runtime_cvt` 的推导指引：从内核类型自动推导 `TDevice` 和 `TInt`。
 * @endif
 *
 * @lang{EN}
 * Deduction guide for `runtime_cvt`: automatically deduces `TDevice` and `TInt`
 * from the kernel type.
 * @endif
 *
 * @tparam KernelType 转换器内核类型，须满足 `io_converter`。
 *                   / The converter kernel type, must satisfy `io_converter`.
 */
template <io_converter KernelType>
runtime_cvt(KernelType&&) -> runtime_cvt<typename std::remove_cvref_t<KernelType>::device_type, typename std::remove_cvref_t<KernelType>::internal_type>;
}
