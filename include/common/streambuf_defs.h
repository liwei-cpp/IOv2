#pragma once
#include <device/device_concepts.h>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class streambuf;

template <io_device TDevice, typename TChar>
class istreambuf;

template <io_device TDevice, typename TChar>
class ostreambuf;

template <typename T>
struct is_streambuf_imp
{
    constexpr static bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_streambuf_imp<streambuf<TDevice, TChar>>
{
    constexpr static bool value = true;
};

template <typename T>
constexpr static bool is_streambuf = is_streambuf_imp<T>::value;

template <typename T>
struct is_istreambuf_imp
{
    constexpr static bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_istreambuf_imp<istreambuf<TDevice, TChar>>
{
    constexpr static bool value = true;
};

template <typename T>
constexpr static bool is_istreambuf = is_istreambuf_imp<T>::value;

template <typename T>
struct is_ostreambuf_imp
{
    constexpr static bool value = false;
};

template <io_device TDevice, typename TChar>
struct is_ostreambuf_imp<ostreambuf<TDevice, TChar>>
{
    constexpr static bool value = true;
};

template <typename T>
constexpr static bool is_ostreambuf = is_ostreambuf_imp<T>::value;

template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_istreambuf<TStreamBuf>)
class istreambuf_iterator;

template <typename TStreamBuf>
    requires (is_streambuf<TStreamBuf> || is_ostreambuf<TStreamBuf>)
class ostreambuf_iterator;

template <typename T>
struct is_istreambuf_iterator
{
    constexpr static bool value = false;
};

template <typename TStreamBuf>
struct is_istreambuf_iterator<istreambuf_iterator<TStreamBuf>>
{
    constexpr static bool value = true;
};

template <typename T>
concept is_istreambuf_iterator_v = is_istreambuf_iterator<T>::value;

template <typename T>
struct is_ostreambuf_iterator
{
    constexpr static bool value = false;
};

template <typename TStreamBuf>
struct is_ostreambuf_iterator<ostreambuf_iterator<TStreamBuf>>
{
    constexpr static bool value = true;
};

template <typename T>
concept is_ostreambuf_iterator_v = is_ostreambuf_iterator<T>::value;
}