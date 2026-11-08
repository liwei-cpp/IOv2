// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file iochannel_defs.h
 * @lang{ZH}
 * 通道及通道迭代器的前向声明。
 * @endif
 * @lang{EN}
 * Forward declarations for channels and channel iterators.
 * @endif
 */

#pragma once
#include <IOv2/common/metafunctions.h>
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
    requires (specialization_of<TChannel, iochannel> || specialization_of<TChannel, ichannel>)
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
    requires (specialization_of<TChannel, iochannel> || specialization_of<TChannel, ochannel>)
class ochannel_iterator;
}
