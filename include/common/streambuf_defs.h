#pragma once
#include <device/device_concepts.h>

namespace IOv2
{
template <io_device TDevice, typename TChar>
struct streambuf;

template <io_device TDevice, typename TChar>
struct istreambuf;

template <io_device TDevice, typename TChar>
struct ostreambuf;

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

template <typename T>
constexpr static bool is_streambuf = is_streambuf_impl<T>::value;

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

template <typename T>
constexpr static bool is_istreambuf = is_istreambuf_impl<T>::value;

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

template <typename T>
constexpr static bool is_ostreambuf = is_ostreambuf_impl<T>::value;

template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_istreambuf<TStreamBuf>)
class istreambuf_iterator;

template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_ostreambuf<TStreamBuf>)
class ostreambuf_iterator;

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

template <typename T>
concept is_istreambuf_iterator = is_istreambuf_iterator_impl<T>::value;

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

template <typename T>
concept is_ostreambuf_iterator = is_ostreambuf_iterator_impl<T>::value;
}