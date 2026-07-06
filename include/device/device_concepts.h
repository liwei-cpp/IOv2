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
                { c.dtell() } -> std::same_as<size_t>;
                { c.dsize() } -> std::same_as<size_t>;
                { a.dseek(std::declval<size_t>()) } -> std::same_as<void>;
                { a.drseek(std::declval<size_t>()) } -> std::same_as<void>;
            };

        /**
         * @lang{ZH}
         * @brief 设备支持写入操作的概念。
         *
         * 要求设备提供写入数据（dput）和刷新缓冲区（dflush）的接口。
         * @tparam T 要检查的设备类型。
         * @endif
         *
         * @lang{EN}
         * @brief Concept for devices that support write operations.
         *
         * Requires the device to provide interfaces for writing data (dput) and
         * flushing the buffer (dflush).
         * @tparam T The device type to check.
         * @endif
         */
        template <typename T>
        concept support_put = requires(T a)
            {
                { a.dput(std::declval<const typename T::char_type*>(), std::declval<size_t>()) } -> std::same_as<void>;
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
                { a.dget(std::declval<typename T::char_type*>(), std::declval<size_t>()) } -> std::same_as<size_t>;
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
