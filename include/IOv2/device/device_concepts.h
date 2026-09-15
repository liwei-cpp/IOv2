// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file device_concepts.h
 * @lang{ZH}
 * 定义了 I/O 设备所需满足的 C++ 概念（Concepts）。
 * 这些概念用于在编译期检查设备类型是否提供了预期的接口，例如定位、读取或写入。
 * @endif
 *
 * @lang{EN}
 * Defines the C++ concepts that I/O devices must satisfy.
 * These concepts are used at compile-time to verify whether a device type provides
 * the expected interface, such as for positioning, reading, or writing.
 * @endif
 */
#pragma once
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace IOv2
{
    /**
     * @lang{ZH}
     * 包含设备概念底层实现的命名空间。
     * @endif
     *
     * @lang{EN}
     * Namespace containing the underlying implementation of device concepts.
     * @endif
     */
    namespace dev_cpt
    {
        /**
         * @lang{ZH}
         * @brief 设备支持定位操作的概念。
         *
         * 要求设备提供获取当前位置、获取总大小、从头定位和从尾定位的接口。
         * @tparam T 要检查的设备类型。
         * @endif
         *
         * @lang{EN}
         * @brief Concept for devices that support positioning operations.
         *
         * Requires the device to provide interfaces for getting the current position,
         * getting the total size, seeking from the beginning, and seeking from the end.
         * @tparam T The device type to check.
         * @endif
         */
        template <typename T>
        concept support_positioning = requires(T a, const T& c)
            {
                { c.dtell() } -> std::same_as<std::size_t>;
                { c.dsize() } -> std::same_as<std::size_t>;
                { a.dseek(std::declval<std::size_t>()) } -> std::same_as<void>;
                { a.drseek(std::declval<std::size_t>()) } -> std::same_as<void>;
            };

        /**
         * @lang{ZH}
         * @brief 设备支持写入操作的概念。
         *
         * 要求设备提供写入数据（dput）和刷新缓冲区（dflush）的接口。
         * @note **自带内部缓冲的设备必须在自己的析构函数里 `dflush()`。** 正常销毁一个流只会把
         *       转换器的缓冲经 `dput()` 推给设备（`root_cvt::~root_cvt` 调的是 `flush()`），
         *       **不会**替设备调用 `dflush()`；单独的 `detach()` 也只把仍打开的设备交还调用方。
         *       显式的设备冲刷发生在 unitbuf 哨兵析构、`flush(true)`、`root_cvt::attach()` 销毁
         *       被替换的旧输出设备之前，以及标准输出流 `reset()` 丢弃已分离设备之前。
         *       `file_device` 与 `std_device` 的析构函数仍自行 `dflush()`，以覆盖正常析构和调用方
         *       取得设备后的生命周期；`mem_device` 无内部缓冲故不需要。
         * @tparam T 要检查的设备类型。
         * @endif
         *
         * @lang{EN}
         * @brief Concept for devices that support write operations.
         *
         * Requires the device to provide interfaces for writing data (dput) and
         * flushing the buffer (dflush).
         * @note **A device that buffers internally must `dflush()` in its own destructor.**
         *       Normal stream destruction only pushes the converter's buffer down to the device
         *       via `dput()` (`root_cvt::~root_cvt` calls `flush()`); it does **not** call the
         *       device's `dflush()`. A standalone `detach()` likewise returns the still-open
         *       device to its caller. Explicit device flushes occur in the unitbuf sentry's
         *       destructor, in `flush(true)`, before `root_cvt::attach()` destroys a replaced
         *       output device, and before a standard output stream's `reset()` discards its
         *       detached device. The destructors of `file_device` and `std_device` still call
         *       `dflush()` themselves to cover normal destruction and the returned device's
         *       subsequent lifetime; `mem_device` has no internal buffer and needs no such step.
         * @tparam T The device type to check.
         * @endif
         */
        template <typename T>
        concept support_put = requires(T a)
            {
                { a.dput(std::declval<const typename T::char_type*>(), std::declval<std::size_t>()) } -> std::same_as<void>;
                { a.dflush() } -> std::same_as<void>;
            };

        /**
         * @lang{ZH}
         * @brief 设备支持读取操作的概念。
         *
         * 要求设备提供读取数据（dget）和检查文件末尾（deof）的接口。
         * @tparam T 要检查的设备类型。
         * @endif
         *
         * @lang{EN}
         * @brief Concept for devices that support read operations.
         *
         * Requires the device to provide interfaces for reading data (dget) and
         * checking for the end of the file (deof).
         * @tparam T The device type to check.
         * @endif
         */
        template <typename T>
        concept support_get = requires(T a)
            {
                { a.dget(std::declval<typename T::char_type*>(), std::declval<std::size_t>()) } -> std::same_as<std::size_t>;
                { a.deof() } -> std::same_as<bool>;
            };
    }

    /**
     * @lang{ZH}
     * @brief I/O 设备的统一概念。
     *
     * 一个类型如果能被称为 I/O 设备，它必须定义 `char_type` 类型，
     * 至少支持读取（support_get）或写入（support_put）操作之一，
     * 并且移动构造和移动赋值操作不能抛出异常。
     * @tparam T 要检查的设备类型。
     * @endif
     *
     * @lang{EN}
     * @brief Unified concept for an I/O device.
     *
     * For a type to be considered an I/O device, it must define the `char_type` type,
     * support at least one of read (support_get) or write (support_put) operations,
     * and have noexcept move construction and move assignment.
     * @tparam T The device type to check.
     * @endif
     */
    template <typename T>
    concept io_device =
        requires{ typename T::char_type; } &&
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_assignable_v<T> &&
        (dev_cpt::support_put<T> || dev_cpt::support_get<T>);
}
