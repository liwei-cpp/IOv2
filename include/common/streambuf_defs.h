/**
 * @file streambuf_defs.h
 * @lang{ZH}
 * 流缓冲区相关的前向声明和类型特征。
 * @endif
 * @lang{EN}
 * Forward declarations and type traits for stream buffers.
 * @endif
 */

#pragma once
#include <device/device_concepts.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * 双向流缓冲区的前向声明。
 *
 * @tparam TDevice 底层设备类型，必须满足 io_device 概念
 * @tparam TChar 字符类型
 * @endif
 *
 * @lang{EN}
 * Forward declaration for bidirectional stream buffer.
 *
 * @tparam TDevice The underlying device type, must satisfy io_device concept
 * @tparam TChar The character type
 * @endif
 */
template <io_device TDevice, typename TChar>
struct streambuf;

/**
 * @lang{ZH}
 * 输入流缓冲区的前向声明。
 *
 * @tparam TDevice 底层设备类型，必须满足 io_device 概念
 * @tparam TChar 字符类型
 * @endif
 *
 * @lang{EN}
 * Forward declaration for input stream buffer.
 *
 * @tparam TDevice The underlying device type, must satisfy io_device concept
 * @tparam TChar The character type
 * @endif
 */
template <io_device TDevice, typename TChar>
struct istreambuf;

/**
 * @lang{ZH}
 * 输出流缓冲区的前向声明。
 *
 * @tparam TDevice 底层设备类型，必须满足 io_device 概念
 * @tparam TChar 字符类型
 * @endif
 *
 * @lang{EN}
 * Forward declaration for output stream buffer.
 *
 * @tparam TDevice The underlying device type, must satisfy io_device concept
 * @tparam TChar The character type
 * @endif
 */
template <io_device TDevice, typename TChar>
struct ostreambuf;

/// @cond INTERNAL
template <typename T>
struct is_streambuf_impl
{
    constexpr static bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_streambuf_impl<streambuf<TDevice, TChar>>
{
    constexpr static bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 streambuf 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is a streambuf specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
constexpr static bool is_streambuf = is_streambuf_impl<T>::value;

/// @cond INTERNAL
template <typename T>
struct is_istreambuf_impl
{
    constexpr static bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_istreambuf_impl<istreambuf<TDevice, TChar>>
{
    constexpr static bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 istreambuf 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an istreambuf specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
constexpr static bool is_istreambuf = is_istreambuf_impl<T>::value;

/// @cond INTERNAL
template <typename T>
struct is_ostreambuf_impl
{
    constexpr static bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_ostreambuf_impl<ostreambuf<TDevice, TChar>>
{
    constexpr static bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 ostreambuf 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an ostreambuf specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
constexpr static bool is_ostreambuf = is_ostreambuf_impl<T>::value;

/**
 * @lang{ZH}
 * 输入流缓冲区迭代器的前向声明。
 *
 * @tparam TStreamBuf 流缓冲区类型，必须是 streambuf 或 istreambuf
 * @endif
 *
 * @lang{EN}
 * Forward declaration for input stream buffer iterator.
 *
 * @tparam TStreamBuf The stream buffer type, must be streambuf or istreambuf
 * @endif
 */
template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_istreambuf<TStreamBuf>)
class istreambuf_iterator;

/**
 * @lang{ZH}
 * 输出流缓冲区迭代器的前向声明。
 *
 * @tparam TStreamBuf 流缓冲区类型，必须是 streambuf 或 ostreambuf
 * @endif
 *
 * @lang{EN}
 * Forward declaration for output stream buffer iterator.
 *
 * @tparam TStreamBuf The stream buffer type, must be streambuf or ostreambuf
 * @endif
 */
template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_ostreambuf<TStreamBuf>)
class ostreambuf_iterator;

/// @cond INTERNAL
template <typename T>
struct is_istreambuf_iterator_impl
{
    constexpr static bool value = false;
};

template <typename TStreamBuf>
struct is_istreambuf_iterator_impl<istreambuf_iterator<TStreamBuf>>
{
    constexpr static bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 istreambuf_iterator 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an istreambuf_iterator specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
concept is_istreambuf_iterator = is_istreambuf_iterator_impl<T>::value;

/// @cond INTERNAL
template <typename T>
struct is_ostreambuf_iterator_impl
{
    constexpr static bool value = false;
};

template <typename TStreamBuf>
struct is_ostreambuf_iterator_impl<ostreambuf_iterator<TStreamBuf>>
{
    constexpr static bool value = true;
};
/// @endcond

/**
 * @lang{ZH}
 * 检查类型 T 是否为 ostreambuf_iterator 特化。
 *
 * @tparam T 要检查的类型
 * @endif
 *
 * @lang{EN}
 * Checks if type T is an ostreambuf_iterator specialization.
 *
 * @tparam T The type to check
 * @endif
 */
template <typename T>
concept is_ostreambuf_iterator = is_ostreambuf_iterator_impl<T>::value;
}
