#pragma once
#include <mutex>

#include <io/io_concepts.h>
#include <io/ostream.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/stream_common_operators.h>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class istream : public ios_base<TChar>
              , public istream_operators<istream<TDevice, TChar>, TChar>
              , public stream_common_operators<istream<TDevice, TChar>, TDevice, TChar>
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using in_sentry_type = in_sentry<istream<device_type, char_type>, false>;

    friend in_sentry_type;
    friend istream_operators<istream<TDevice, TChar>, TChar>;
    friend stream_common_operators<istream<TDevice, TChar>, TDevice, TChar>;

public:
    istream()
        : m_streambuf(TDevice()) {}

    istream(TDevice dev)
        : m_streambuf(std::move(dev)) {}

    template <cvt_creator TCreator>
    istream(TDevice dev, const TCreator& creator)
        : m_streambuf(std::move(dev), creator) {}

    istream(TDevice dev, locale<char_type> loc)
        : m_streambuf(std::move(dev))
        , m_locale(std::move(loc)) {}

    template <cvt_creator TCreator>
    istream(TDevice dev, const TCreator& creator, locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

private:
    istreambuf<TDevice, TChar> m_streambuf;
    abs_ostream* m_tie_stream = nullptr;
    locale<char_type> m_locale;
    std::mutex        m_io_mutex;
};

template <io_device TDevice>
istream(TDevice) -> istream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
istream(TDevice, const TCreator&) -> istream<TDevice,
                                             ext_to_int<TDevice, TCreator>>;

template <io_device TDevice, typename TChar>
    requires (std::is_same_v<typename TDevice::char_type, TChar>)
istream(TDevice, locale<TChar>) -> istream<TDevice, TChar>;

template <io_device TDevice, cvt_creator TCreator, typename TChar>
    requires (std::is_same_v<ext_to_int<TDevice, TCreator>, TChar>)
istream(TDevice, const TCreator&, locale<TChar>) -> istream<TDevice, TChar>;

// common manips
template <istream_type T>
inline void ws(T& is)
{
    using sentry_type = typename T::in_sentry_type;
    try
    {
        sentry_type cerb(is, false);
    }
    catch(...)
    {
        is.handle_exception(std::current_exception());
    }
}
}