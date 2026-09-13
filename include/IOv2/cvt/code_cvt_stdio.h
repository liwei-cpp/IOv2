// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file code_cvt_stdio.h
 * @lang{ZH}
 * 支持运行时区域设置动态切换的字符编码转换器（code_cvt_stdio）及其配套类型定义文件。
 * 本文件在 `code_cvt` 的基础上增加了以下能力：
 * - `code_cvt_switch`：行为策略，用于在运行时切换字符编码（区域设置）。
 * - `code_cvt_access`：状态对象，用于查询转换器当前使用的区域设置名称。
 * - `code_cvt_stdio`：继承自 `code_cvt<KernelType, wchar_t>`，通过 `adjust()`/`retrieve()`
 *   接口支持编码的动态切换与查询。
 * - `code_cvt_stdio_creator`：`code_cvt_stdio` 的工厂类，满足 `cvt_creator` 概念。
 * @endif
 *
 * @lang{EN}
 * Definition file for the character encoding converter with runtime locale switching
 * support (code_cvt_stdio) and its companion types. This file extends `code_cvt`
 * with the following capabilities:
 * - `code_cvt_switch`: Behavior policy for switching the character encoding (locale)
 *   at runtime.
 * - `code_cvt_access`: Status object for querying the locale name currently in use
 *   by the converter.
 * - `code_cvt_stdio`: Inherits from `code_cvt<KernelType, wchar_t>` and supports
 *   dynamic encoding switching and querying via the `adjust()`/`retrieve()` interface.
 * - `code_cvt_stdio_creator`: Factory class for `code_cvt_stdio`, satisfying the
 *   `cvt_creator` concept.
 * @endif
 */
#pragma once
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/device/std_device.h>

#include <string>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * 运行时区域设置切换的行为策略。
 * 将此策略传入 `code_cvt_stdio::adjust()` 可在运行时动态更换字符编码（区域设置）。
 * 切换时要求编码转换状态处于初始状态；切换操作提供强异常安全保证。
 * @endif
 *
 * @lang{EN}
 * Behavior policy for runtime locale switching.
 * Passing this policy to `code_cvt_stdio::adjust()` dynamically replaces the
 * character encoding (locale) at runtime. The encoding conversion state must be
 * in its initial state at the time of switching. The switch operation provides
 * the strong exception safety guarantee.
 * @endif
 */
struct code_cvt_switch : cvt_behavior
{
    /**
     * @lang{ZH}
     * 以指定的区域设置名称构造切换策略。
     *
     * @param p_code 目标区域设置名称（如 "zh_CN.UTF-8"）。
     * @endif
     *
     * @lang{EN}
     * Construct the switching policy with the specified locale name.
     *
     * @param p_code Target locale name (e.g., "zh_CN.UTF-8").
     * @endif
     */
    explicit code_cvt_switch(std::string p_code)
        : code(std::move(p_code)) {}

    std::string code; ///< 目标区域设置名称 / Target locale name.
};

/**
 * @lang{ZH}
 * 用于查询转换器当前区域设置名称的状态对象。
 * 将此对象传入 `code_cvt_stdio::retrieve()` 后，`code` 字段将被填充为
 * 转换器当前使用的区域设置名称。
 * @endif
 *
 * @lang{EN}
 * Status object for querying the converter's current locale name.
 * After passing this object to `code_cvt_stdio::retrieve()`, the `code` field
 * is populated with the locale name currently in use by the converter.
 * @endif
 */
struct code_cvt_access : cvt_status
{
    std::string code; ///< 查询结果：转换器当前的区域设置名称 / Query result: the converter's current locale name.
};

/**
 * @lang{ZH}
 * 支持运行时区域设置动态切换的字符编码转换器（char <-> wchar_t）。
 *
 * 继承自 `code_cvt<KernelType, wchar_t>`，并通过覆写 `adjust()` 和 `retrieve()` 接口
 * 支持在任意时刻切换字符编码（区域设置）及查询当前编码：
 * - 向 `adjust()` 传入 `code_cvt_switch` 策略以切换编码；
 * - 向 `retrieve()` 传入 `code_cvt_access` 对象以查询当前编码名称。
 *
 * 本类专用于标准流：底层设备只能是 `std_device<STDIN_FILENO>` / `<STDOUT_FILENO>` /
 * `<STDERR_FILENO>`。这类设备是无状态的 fd 包装，缺省构造的对象与被替换的对象指向同一个
 * fd，于是「转换器 tainted 后缺省构造一个设备重新 `attach()`」等价于就地重新初始化——
 * 本类据此在 `put`/`get`/`flush`/`adjust` 入口自动恢复：发现 `is_tainted()` 即先重新附接
 * 同一 fd（`attach()` + `bos()` + `main_cont_beg()`），再执行本次操作。调用方（标准流对象）
 * 因此只需 `clear()` 状态位即可继续，与 `std::wcout`/`std::wcin` 的用法一致；出错那一批里
 * 尚未提交的数据随 taint 丢失，已提交到根转换器的数据在重新附接时刷出。对 `mem_device`
 * 之类有内容的设备，这样的重新附接会换成一个空设备，因此不允许。
 *
 * @tparam KernelType 底层 I/O 转换器类型，须满足 `io_converter` 概念；
 *                    其 `internal_type` 须为 `char`，`device_type` 须为上述三种标准设备之一。
 *
 * @note 线程安全性：本类**不是**线程安全的。多线程并发访问同一实例须通过外部同步机制保护。
 * @endif
 *
 * @lang{EN}
 * Character encoding converter with runtime locale switching support (char <-> wchar_t).
 *
 * Inherits from `code_cvt<KernelType, wchar_t>` and overrides `adjust()` and
 * `retrieve()` to support dynamically switching the character encoding (locale)
 * at any time and querying the current encoding:
 * - Pass a `code_cvt_switch` policy to `adjust()` to switch the encoding.
 * - Pass a `code_cvt_access` object to `retrieve()` to query the current locale name.
 *
 * This class is for the standard streams only: the underlying device must be
 * `std_device<STDIN_FILENO>` / `<STDOUT_FILENO>` / `<STDERR_FILENO>`. Those devices are
 * stateless fd wrappers -- a default-constructed one refers to the same fd as the one it
 * replaces -- so "default-construct a device and `attach()` it once the converter is
 * tainted" amounts to re-initializing in place. The class uses that to recover on its
 * own at the `put`/`get`/`flush`/`adjust` entry points: when `is_tainted()` it first
 * reattaches the same fd (`attach()` + `bos()` + `main_cont_beg()`), then performs the
 * operation. The caller (a standard stream object) therefore only has to `clear()` its
 * state bits to carry on, the same way `std::wcout`/`std::wcin` are used; what the
 * failing batch had not yet committed is lost with the taint, what had reached the root
 * converter is flushed by the reattach. A device with content of its own, such as
 * `mem_device`, would be swapped for an empty one by such a reattach, hence the
 * restriction.
 *
 * @tparam KernelType Underlying I/O converter type, must satisfy the `io_converter`
 *                    concept; its `internal_type` must be `char` and its `device_type`
 *                    one of the three standard devices above.
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization.
 * @endif
 */
template <io_converter KernelType>
    requires (std::is_same_v<typename KernelType::device_type, std_device<STDIN_FILENO>> ||
              std::is_same_v<typename KernelType::device_type, std_device<STDOUT_FILENO>> ||
              std::is_same_v<typename KernelType::device_type, std_device<STDERR_FILENO>>)
class code_cvt_stdio : public code_cvt<KernelType, wchar_t>
{
private:
    using BT = code_cvt<KernelType, wchar_t>;

    // recover() reattaches a default-constructed device of the same type.
    static_assert(std::is_default_constructible_v<typename KernelType::device_type>,
                  "code_cvt_stdio: the device must be default-constructible");

public:
    /**
     * @lang{ZH}
     * 以指定的底层内核和初始区域设置名称构造转换器。
     *
     * @param kernel 底层 I/O 转换器实例（移动传入）。
     * @param code   初始区域设置名称（如 "zh_CN.UTF-8"）。
     * @endif
     *
     * @lang{EN}
     * Construct the converter with the specified underlying kernel and initial locale name.
     *
     * @param kernel Underlying I/O converter instance (moved in).
     * @param code   Initial locale name (e.g., "zh_CN.UTF-8").
     * @endif
     */
    code_cvt_stdio(KernelType kernel, const std::string& code)
        : BT(std::move(kernel), code)
        , m_code(code)
    {}

    code_cvt_stdio(const code_cvt_stdio& val) = default;
    code_cvt_stdio& operator=(const code_cvt_stdio& val) = default;
    code_cvt_stdio(code_cvt_stdio&& val) = default;
    code_cvt_stdio& operator=(code_cvt_stdio&& val) = default;
    ~code_cvt_stdio() = default;

public:
    /**
     * @lang{ZH}
     * @brief 同 `code_cvt::put`，但转换器 tainted 时先重新附接同一 fd 再写（见类文档）。
     * @endif
     * @lang{EN}
     * @brief As `code_cvt::put`, but reattaches the same fd first when the converter is
     * tainted (see the class documentation).
     * @endif
     */
    void put(const typename BT::internal_type* to, std::size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (this->is_tainted()) recover();
        BT::put(to, to_size);
    }

    /**
     * @lang{ZH}
     * @brief 同 `code_cvt::get`，但转换器 tainted 时先重新附接同一 fd 再读（见类文档）。
     * @endif
     * @lang{EN}
     * @brief As `code_cvt::get`, but reattaches the same fd first when the converter is
     * tainted (see the class documentation).
     * @endif
     */
    std::size_t get(typename BT::internal_type* to, std::size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (this->is_tainted()) recover();
        return BT::get(to, to_max);
    }

    /**
     * @lang{ZH}
     * @brief 同 `code_cvt::flush`，但转换器 tainted 时先重新附接同一 fd（见类文档）。
     * @endif
     * @lang{EN}
     * @brief As `code_cvt::flush`, but reattaches the same fd first when the converter is
     * tainted (see the class documentation).
     * @endif
     */
    void flush()
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (this->is_tainted()) recover();
        BT::flush();
    }

public:
    /**
     * @lang{ZH}
     * 响应状态查询调用，支持查询当前区域设置名称。
     *
     * 若 `s` 为 `code_cvt_access` 类型，则将当前区域设置名称（`m_code`）
     * 填充至 `s.code` 并返回。
     * 否则将调用委托给基类 `retrieve()`。
     *
     * @param s 状态对象（输出参数）。
     * @endif
     *
     * @lang{EN}
     * Respond to a status query call, supporting retrieval of the current locale name.
     *
     * If `s` is of type `code_cvt_access`, populates `s.code` with the current
     * locale name (`m_code`) and returns.
     * Otherwise, delegates the call to the base class `retrieve()`.
     *
     * @param s Status object (output parameter).
     * @endif
     */
    void retrieve(cvt_status& s) const
    {
        if (auto* ptr = dynamic_cast<code_cvt_access*>(&s); ptr)
        {
            ptr->code = m_code;
            return;
        }
        BT::retrieve(s);
    }

public:
    /**
     * @lang{ZH}
     * 响应行为策略调用，支持运行时切换字符编码。
     *
     * 若 `acc` 为 `code_cvt_switch` 类型，则执行区域设置切换：
     * 用 `acc.code` 构造新的 `codecvt_kernel` 并原子地替换当前内核，
     * 同时更新缓存的区域设置名称 `m_code`。
     * 转换器 tainted 时先重新附接同一 fd（见类文档），之后要求编码转换状态（`m_cvt_kernel`）
     * 处于初始状态，否则抛出异常。所有可能抛出异常的操作均在提交前完成，确保强异常安全保证。
     * 无论 `acc` 类型如何，最终均链式调用基类 `BT::adjust(acc)`。
     *
     * @param acc 行为策略对象。
     *
     * @throws cvt_error 若当前编码转换状态不处于初始状态；此时不切换。
     * @endif
     *
     * @lang{EN}
     * Respond to a behavior policy call, supporting runtime character encoding switching.
     *
     * If `acc` is of type `code_cvt_switch`, performs a locale switch: constructs a
     * new `codecvt_kernel` from `acc.code` and atomically replaces the current kernel,
     * also updating the cached locale name `m_code`.
     * A tainted converter is first reattached to the same fd (see the class
     * documentation); after that the encoding conversion state (`m_cvt_kernel`) must be
     * in its initial state at the time of switching, otherwise an exception is thrown.
     * All potentially-throwing operations are completed before any commit, ensuring
     * the strong exception safety guarantee.
     * In all cases the call is forwarded to the base-class `BT::adjust(acc)`.
     *
     * @param acc Behavior policy object.
     *
     * @throws cvt_error If the encoding conversion state is not in its initial state;
     *         nothing is switched then.
     * @endif
     */
    void adjust(const cvt_behavior& acc)
    {
        if (this->is_tainted()) recover();
        if (const auto* ptr = dynamic_cast<const code_cvt_switch*>(&acc); ptr)
        {
            if (!this->m_cvt_kernel.is_init_state())
                throw cvt_error("code_cvt_stdio::adjust fail: invalid state");
            // Perform all potentially-throwing operations first, then commit with
            // noexcept moves. The two commit moves below MUST be noexcept; otherwise
            // a partial-throw could leave m_cvt_kernel and m_code mutually
            // inconsistent (mismatched locale handle vs. cached code string).
            static_assert(
                std::is_nothrow_move_assignable_v<codecvt_kernel<char, wchar_t>>,
                "codecvt_kernel<char, wchar_t> move-assign must be noexcept; "
                "otherwise code_cvt_stdio::adjust loses its strong exception guarantee");
            static_assert(
                std::is_nothrow_move_assignable_v<std::string>,
                "std::string move-assign must be noexcept");
            codecvt_kernel<char, wchar_t> new_kernel(ptr->code);
            std::string new_code = ptr->code;
            this->m_cvt_kernel = std::move(new_kernel);
            m_code = std::move(new_code);
        }
        return BT::adjust(acc);
    }

private:
    /**
     * @lang{ZH}
     * @brief 把 tainted 的转换器重新附接到同一 fd 并重新初始化。
     *
     * 等价于 `streambuf::attach()` 对本层做的事：`attach()` 装入缺省构造的同 fd 设备
     * （其内部先 `detach()` 旧设备、刷出根转换器里已提交的数据、清除 taint），再
     * `bos()` + `main_cont_beg()` 回到主内容阶段。旧设备的刷新若再次失败（fd 仍不可写），
     * `attach()` 原样抛出、taint 保持——下一次入口会再试，届时未刷出的数据已被 `detach()`
     * 丢弃，重新附接得以完成。
     * @endif
     *
     * @lang{EN}
     * @brief Reattaches a tainted converter to the same fd and re-initializes it.
     *
     * Equivalent to what `streambuf::attach()` does for this layer: `attach()` installs a
     * default-constructed device on the same fd (internally detaching the old one, which
     * flushes what the root converter had committed and clears the taint), then `bos()` +
     * `main_cont_beg()` return to the main-content phase. If flushing the old device fails
     * again (the fd is still unwritable) `attach()` propagates that and the taint stays;
     * the next entry retries, by which time `detach()` has dropped the unflushed bytes and
     * the reattach goes through.
     * @endif
     */
    void recover()
    {
        this->attach(typename BT::device_type{});
        this->bos();
        this->main_cont_beg();
    }

    std::string m_code; ///< 当前使用的区域设置名称 / Currently active locale name.
};

/**
 * @lang{ZH}
 * `code_cvt_stdio` 转换器的工厂类。
 * 持有一个区域设置名称，并通过 `create()` 方法为给定底层内核生成
 * `code_cvt_stdio` 实例。满足 `cvt_creator` 概念。
 * @endif
 *
 * @lang{EN}
 * Factory class for `code_cvt_stdio` converters.
 * Holds a locale name and creates `code_cvt_stdio` instances for a given
 * underlying kernel via the `create()` method. Satisfies the `cvt_creator` concept.
 * @endif
 */
class code_cvt_stdio_creator
{
public:
    using category = CvtCreatorCategory;

    /**
     * @lang{ZH}
     * 以指定的区域设置名称构造工厂实例。
     *
     * @param name 区域设置名称，将在每次调用 `create()` 时传递给所创建的 `code_cvt_stdio`。
     * @endif
     *
     * @lang{EN}
     * Construct the factory with the specified locale name.
     *
     * @param name Locale name to be passed to each `code_cvt_stdio` instance
     *             created by `create()`.
     * @endif
     */
    explicit code_cvt_stdio_creator(std::string name)
        : m_name(std::move(name)) {}

    /**
     * @lang{ZH}
     * 为指定的底层内核创建一个 `code_cvt_stdio` 实例。
     * 要求 `TKernel::internal_type` 为 `char`（由 `static_assert` 在编译期强制检查）。
     *
     * @tparam TKernel 底层 I/O 转换器类型，须满足 `io_converter` 概念。
     * @param  kernel  底层转换器实例（完美转发）。
     *
     * @return 已配置区域设置的 `code_cvt_stdio<TKernel>` 实例。
     * @endif
     *
     * @lang{EN}
     * Create a `code_cvt_stdio` instance for the specified underlying kernel.
     * Requires `TKernel::internal_type` to be `char` (enforced by `static_assert`
     * at compile time).
     *
     * @tparam TKernel Underlying I/O converter type, must satisfy the `io_converter` concept.
     * @param  kernel  Underlying converter instance (perfect-forwarded).
     *
     * @return A `code_cvt_stdio<TKernel>` instance configured with the stored locale name.
     * @endif
     */
    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        static_assert(std::is_same_v<typename TKernel::internal_type, char>);
        return code_cvt_stdio{std::forward<TKernel>(kernel), m_name};
    }
private:
    std::string m_name; ///< 区域设置名称 / Locale name.
};
}
