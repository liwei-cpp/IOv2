/**
 * @file cvt_concepts.h
 * @lang{ZH}
 * 转换器概念（Concepts）与基础类型定义文件。
 * 本文件定义了 IOv2 转换器体系的核心抽象：行为策略基类、状态基类、
 * I/O 方向枚举，以及用于静态约束转换器能力的 C++20 概念集合。
 * @endif
 *
 * @lang{EN}
 * Converter concept and foundational type definitions.
 * This file defines the core abstractions of the IOv2 converter system:
 * the behavior policy base class, the status base class, the I/O direction
 * enumeration, and the set of C++20 concepts that statically constrain
 * converter capabilities.
 * @endif
 */
#pragma once
#include <common/defs.h>

#include <concepts>
#include <cstddef>
#include <exception>
#include <type_traits>
#include <utility>

namespace IOv2
{
    /**
     * @lang{ZH}
     * 转换器行为策略的多态基类。
     * 派生类通过继承此类来定义特定的转换行为，
     * 并通过转换器的 `adjust()` 接口将行为策略传递给转换器实例。
     * @endif
     *
     * @lang{EN}
     * Polymorphic base class for converter behavior policies.
     * Derived classes define specific conversion behaviors and pass
     * them to converter instances via the converter's `adjust()` interface.
     * @endif
     */
    struct cvt_behavior
    {
        cvt_behavior() = default;
        cvt_behavior(const cvt_behavior&) = default;
        cvt_behavior& operator=(const cvt_behavior&) = default;
        cvt_behavior(cvt_behavior&&) = default;
        cvt_behavior& operator=(cvt_behavior&&) = default;
        virtual ~cvt_behavior() = default;
    };

    /**
     * @lang{ZH}
     * 转换器状态的多态基类。
     * 派生类通过继承此类来携带转换器的内部状态信息，
     * 并通过转换器的 `retrieve()` 接口从转换器实例中提取该状态。
     * @endif
     *
     * @lang{EN}
     * Polymorphic base class for converter status.
     * Derived classes carry the internal state information of a converter
     * and are populated from converter instances via the converter's
     * `retrieve()` interface.
     * @endif
     */
    struct cvt_status
    {
        cvt_status() = default;
        cvt_status(const cvt_status&) = default;
        cvt_status& operator=(const cvt_status&) = default;
        cvt_status(cvt_status&&) = default;
        cvt_status& operator=(cvt_status&&) = default;
        virtual ~cvt_status() = default;
    };

    /**
     * @lang{ZH}
     * 转换器当前的 I/O 方向状态枚举。
     * - `neutral`：未指定方向，既非输入也非输出。
     * - `input`：输入（读取）模式。
     * - `output`：输出（写入）模式。
     * @endif
     *
     * @lang{EN}
     * Enumeration of the current I/O direction state for a converter.
     * - `neutral`: Unspecified direction, neither input nor output.
     * - `input`: Input (reading) mode.
     * - `output`: Output (writing) mode.
     * @endif
     */
    enum class io_status : char
    {
        neutral, ///< 未指定方向 / Unspecified direction.
        input,   ///< 输入（读取）模式 / Input (reading) mode.
        output   ///< 输出（写入）模式 / Output (writing) mode.
    };

    /**
     * @lang{ZH}
     * 转换器能力约束概念的定义命名空间。
     * 该命名空间中的概念用于在编译期静态检查转换器类型所具备的操作能力。
     * @endif
     *
     * @lang{EN}
     * Namespace containing concept definitions that constrain converter capabilities.
     * The concepts in this namespace statically verify at compile time which
     * operations a converter type supports.
     * @endif
     */
    namespace cvt_cpt
    {
        /**
         * @lang{ZH}
         * 约束类型 T 支持写入操作。
         * T 须提供以下成员函数，且签名须与规范完全一致：
         * - `put(const internal_type*, size_t) -> void`：将数据写入转换器。
         * - `flush() -> void`：将所有缓冲数据刷出转换器。
         * @endif
         *
         * @lang{EN}
         * Constrains type T to support write operations.
         * T must provide the following member functions with exact signatures:
         * - `put(const internal_type*, size_t) -> void`: Write data into the converter.
         * - `flush() -> void`: Flush all buffered data out of the converter.
         * @endif
         */
        template <typename T>
        concept support_put = requires(T a)
            {
                { a.put(std::declval<const typename T::internal_type*>(), std::declval<size_t>()) } -> std::same_as<void>;
                { a.flush() } -> std::same_as<void>;
            };

        /**
         * @lang{ZH}
         * 约束类型 T 支持读取操作。
         * T 须提供以下成员函数，且签名须与规范完全一致：
         * - `get(internal_type*, size_t) -> size_t`：从转换器读取数据，返回实际读取的元素数量。
         * - `is_eof() -> bool`：查询转换器是否已到达数据末尾。
         * @endif
         *
         * @lang{EN}
         * Constrains type T to support read operations.
         * T must provide the following member functions with exact signatures:
         * - `get(internal_type*, size_t) -> size_t`: Read data from the converter,
         *   returning the number of elements actually read.
         * - `is_eof() -> bool`: Query whether the converter has reached the end of data.
         * @endif
         */
        template <typename T>
        concept support_get = requires(T a)
            {
                { a.get(std::declval<typename T::internal_type*>(), std::declval<size_t>()) } -> std::same_as<size_t>;
                { a.is_eof() } -> std::same_as<bool>;
            };

        /**
         * @lang{ZH}
         * 约束类型 T 支持流定位操作。
         * T 须提供以下成员函数，且签名须与规范完全一致：
         * - `tell() -> size_t`（const）：返回当前流位置。
         * - `seek(size_t) -> void`：将流定位到指定的绝对位置。
         * - `rseek(size_t) -> void`：将流定位到指定的相对位置。
         * @endif
         *
         * @lang{EN}
         * Constrains type T to support stream positioning operations.
         * T must provide the following member functions with exact signatures:
         * - `tell() -> size_t` (const): Return the current stream position.
         * - `seek(size_t) -> void`: Position the stream to the specified absolute position.
         * - `rseek(size_t) -> void`: Position the stream to the specified relative position.
         * @endif
         */
        template <typename T>
        concept support_positioning = requires(T a, const T& c)
            {
                { c.tell() } -> std::same_as<size_t>;
                { a.seek(std::declval<size_t>()) } -> std::same_as<void>;
                { a.rseek(std::declval<size_t>()) } -> std::same_as<void>;
            };

        /**
         * @lang{ZH}
         * 约束类型 T 同时支持读写操作，并可在两种模式之间切换。
         * T 须同时满足 `support_put` 和 `support_get`，并额外提供：
         * - `switch_to_get() -> void`：切换至输入（读取）模式。
         * - `switch_to_put() -> void`：切换至输出（写入）模式。
         * @endif
         *
         * @lang{EN}
         * Constrains type T to support both read and write operations and to
         * switch between the two modes. T must satisfy both `support_put` and
         * `support_get`, and additionally provide:
         * - `switch_to_get() -> void`: Switch to input (reading) mode.
         * - `switch_to_put() -> void`: Switch to output (writing) mode.
         * @endif
         */
        template <typename T>
        concept support_io_switch = support_put<T> &&
            support_get<T> &&
            requires(T a)
            {
                { a.switch_to_get() } -> std::same_as<void>;
                { a.switch_to_put() } -> std::same_as<void>;
            };

        /**
         * @lang{ZH}
         * 约束类型 T 具备所有转换器必须提供的基础接口，包括：
         * - 设备管理：`device()`、`detach()`、`attach()`
         * - 生命周期控制：`bos()`（建立初始 IO 状态）、`main_cont_beg()`（进入主内容阶段）
         * - 行为与状态交互：`adjust()`（设置行为策略）、`retrieve()`（提取内部状态）
         * @endif
         *
         * @lang{EN}
         * Constrains type T to provide all mandatory interfaces required by a converter:
         * - Device management: `device()`, `detach()`, `attach()`
         * - Lifecycle control: `bos()` (establish initial IO state),
         *   `main_cont_beg()` (transition into the main content phase)
         * - Behavior and status interaction: `adjust()` (apply a behavior policy),
         *   `retrieve()` (extract internal state)
         * @endif
         */
        template <typename T>
        concept mandatory_methods = requires(T a, const T& c,
                                             const cvt_behavior& b, cvt_status& s)
            {
                { a.device() } -> std::same_as<typename T::device_type&>;
                { a.detach() } noexcept -> std::same_as<std::pair<typename T::device_type, std::exception_ptr>>;
                { a.attach(std::declval<typename T::device_type>()) } -> std::same_as<typename T::device_type>;

                { a.bos() } -> std::same_as<io_status>;
                { a.main_cont_beg() } -> std::same_as<void>;

                { a.adjust(b) } -> std::same_as<void>;
                { c.retrieve(s) } -> std::same_as<void>;
            };
    }

    /**
     * @lang{ZH}
     * 完整 I/O 转换器的顶层概念约束。
     * 满足此概念的类型 T 须同时具备：
     * - 至少支持读取（`support_get`）或写入（`support_put`）操作之一；
     * - 满足 `mandatory_methods` 所规定的全部基础接口；
     * - 定义 `internal_type`（内部数据类型）和 `external_type`（外部数据类型）关联类型；
     * - 移动构造和移动赋值操作不能抛出异常。
     * @endif
     *
     * @lang{EN}
     * Top-level concept constraint for a complete I/O converter.
     * A type T satisfying this concept must:
     * - Support at least one of `support_get` (read) or `support_put` (write);
     * - Satisfy all mandatory interfaces defined by `mandatory_methods`;
     * - Expose both `internal_type` (internal data type) and `external_type`
     *   (external data type) as associated types;
     * - Have noexcept move construction and move assignment.
     * @endif
     */
    template<typename T>
    concept io_converter = (cvt_cpt::support_put<T> || cvt_cpt::support_get<T>) &&
        cvt_cpt::mandatory_methods<T> &&
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_assignable_v<T> &&
        requires {
            typename T::internal_type;
            typename T::external_type;
        };

    /**
     * @lang{ZH}
     * 转换器工厂（Creator）的类别标签类型。
     * 用于 `cvt_creator` 概念中的类型分类：将 `category` 成员类型定义为
     * `CvtCreatorCategory` 的类型即被视为转换器工厂。
     * @endif
     *
     * @lang{EN}
     * Category tag type for converter creators.
     * Used for type classification in the `cvt_creator` concept: any type that
     * defines its `category` member type as `CvtCreatorCategory` is recognized
     * as a converter creator.
     * @endif
     */
    struct CvtCreatorCategory;

    /**
     * @lang{ZH}
     * 约束类型 T 为转换器工厂类型。
     * T 须将其 `category` 成员类型定义为 `CvtCreatorCategory`。
     * @endif
     *
     * @lang{EN}
     * Constrains type T to be a converter creator type.
     * T must define its `category` member type as `CvtCreatorCategory`.
     * @endif
     */
    template<typename T>
    concept cvt_creator = std::is_same_v<typename T::category, CvtCreatorCategory>;
}
