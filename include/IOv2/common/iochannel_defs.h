// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file iochannel_defs.h
 * @lang{ZH}
 * 通道相关的前向声明和类型特征。
 * @endif
 * @lang{EN}
 * Forward declarations and type traits for channels.
 * @endif
 */

#pragma once
#include <IOv2/device/device_concepts.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * 双向通道的前向声明。
 *
 * @tparam TDevice 底层设备类型，必须满足 io_device 概念
 * @tparam TChar 字符类型
 * @endif
 *
 * @lang{EN}
 * Forward declaration for bidirectional channel.
 *
 * @tparam TDevice The underlying device type, must satisfy io_device concept
 * @tparam TChar The character type
 * @endif
 */
template <io_device TDevice, typename TChar>
struct iochannel;

/**
 * @lang{ZH}
 * 输入通道的前向声明。
 *
 * @tparam TDevice 底层设备类型，必须满足 io_device 概念
 * @tparam TChar 字符类型
 * @endif
 *
 * @lang{EN}
 * Forward declaration for input channel.
 *
 * @tparam TDevice The underlying device type, must satisfy io_device concept
 * @tparam TChar The character type
 * @endif
 */
template <io_device TDevice, typename TChar>
struct ichannel;

/**
 * @lang{ZH}
 * 输出通道的前向声明。
 *
 * @tparam TDevice 底层设备类型，必须满足 io_device 概念
 * @tparam TChar 字符类型
 * @endif
 *
 * @lang{EN}
 * Forward declaration for output channel.
 *
 * @tparam TDevice The underlying device type, must satisfy io_device concept
 * @tparam TChar The character type
 * @endif
 */
template <io_device TDevice, typename TChar>
struct ochannel;

/// @cond INTERNAL
template <typename T>
struct is_iochannel_impl
{
    static constexpr bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_iochannel_impl<iochannel<TDevice, TChar>>
{
    static constexpr bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 iochannel 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an iochannel specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
static constexpr bool is_iochannel = is_iochannel_impl<T>::value;

/// @cond INTERNAL
template <typename T>
struct is_ichannel_impl
{
    static constexpr bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_ichannel_impl<ichannel<TDevice, TChar>>
{
    static constexpr bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 ichannel 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an ichannel specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
static constexpr bool is_ichannel = is_ichannel_impl<T>::value;

/// @cond INTERNAL
template <typename T>
struct is_ochannel_impl
{
    static constexpr bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_ochannel_impl<ochannel<TDevice, TChar>>
{
    static constexpr bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 ochannel 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an ochannel specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
static constexpr bool is_ochannel = is_ochannel_impl<T>::value;

/**
 * @lang{ZH}
 * 输入通道迭代器的前向声明。
 *
 * @tparam TChannel 通道类型，必须是 iochannel 或 ichannel
 * @endif
 *
 * @lang{EN}
 * Forward declaration for input channel iterator.
 *
 * @tparam TChannel The channel type, must be iochannel or ichannel
 * @endif
 */
template <typename TChannel>
    requires (is_iochannel<TChannel> || is_ichannel<TChannel>)
class ichannel_iterator;

/**
 * @lang{ZH}
 * 输出通道迭代器的前向声明。
 *
 * @tparam TChannel 通道类型，必须是 iochannel 或 ochannel
 * @endif
 *
 * @lang{EN}
 * Forward declaration for output channel iterator.
 *
 * @tparam TChannel The channel type, must be iochannel or ochannel
 * @endif
 */
template <typename TChannel>
    requires (is_iochannel<TChannel> || is_ochannel<TChannel>)
class ochannel_iterator;

/// @cond INTERNAL
template <typename T>
struct is_ichannel_iterator_impl
{
    static constexpr bool value = false;
};

template <typename TChannel>
struct is_ichannel_iterator_impl<ichannel_iterator<TChannel>>
{
    static constexpr bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 ichannel_iterator 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an ichannel_iterator specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
concept is_ichannel_iterator = is_ichannel_iterator_impl<T>::value;

/// @cond INTERNAL
template <typename T>
struct is_ochannel_iterator_impl
{
    static constexpr bool value = false;
};

template <typename TChannel>
struct is_ochannel_iterator_impl<ochannel_iterator<TChannel>>
{
    static constexpr bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 ochannel_iterator 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an ochannel_iterator specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
concept is_ochannel_iterator = is_ochannel_iterator_impl<T>::value;
}
