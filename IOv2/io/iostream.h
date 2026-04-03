#pragma once
#include <mutex>

#include <io/io_concepts.h>
#include <io/ostream.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>
#include <io/utilities/stream_common_operators.h>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class iostream : public ios_base<TChar>
               , public istream_operators<iostream<TDevice, TChar>, TChar>
               , public ostream_operators<iostream<TDevice, TChar>, TChar>
               , public stream_common_operators<iostream<TDevice, TChar>, TDevice, TChar>
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using in_sentry_type = in_sentry<iostream<TDevice, TChar>, true>;
    using out_sentry_type = out_sentry<iostream<TDevice, TChar>, true>;

    friend in_sentry_type;
    friend out_sentry_type;
    friend istream_operators<iostream<TDevice, TChar>, TChar>;
    friend ostream_operators<iostream<TDevice, TChar>, TChar>;
    friend stream_common_operators<iostream<TDevice, TChar>, TDevice, TChar>;

public:
    iostream()
        : m_streambuf(TDevice()) {}

    iostream(TDevice dev)
        : m_streambuf(std::move(dev)) {}

    template <cvt_creator TCreator>
    iostream(TDevice dev, const TCreator& creator)
        : m_streambuf(std::move(dev), creator) {}

    iostream(TDevice dev, locale<char_type> loc)
        : m_streambuf(std::move(dev))
        , m_locale(std::move(loc)) {}

    template <cvt_creator TCreator>
    iostream(TDevice dev, const TCreator& creator, locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

public:
    iostream& switch_to_put()
    {
        try
        {
            m_streambuf.switch_to_put();
        }
        catch(...)
        {
            this->handle_exception(std::current_exception());
        }
        return *this;
    }

    iostream& switch_to_get()
    {
        try
        {
            m_streambuf.switch_to_get();
        }
        catch(...)
        {
            this->handle_exception(std::current_exception());
        }
        return *this;
    }

private:
    streambuf<TDevice, TChar> m_streambuf;
    abs_ostream* m_tie_stream = nullptr;
    locale<char_type> m_locale;
    std::mutex        m_io_mutex;
};

template <io_device TDevice>
iostream(TDevice) -> iostream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
iostream(TDevice, const TCreator&) -> iostream<TDevice,
                                               ext_to_int<TDevice, TCreator>>;

template <io_device TDevice, typename TChar>
    requires (std::is_same_v<typename TDevice::char_type, TChar>)
iostream(TDevice, locale<TChar>) -> iostream<TDevice, TChar>;

template <io_device TDevice, cvt_creator TCreator, typename TChar>
    requires (std::is_same_v<ext_to_int<TDevice, TCreator>, TChar>)
iostream(TDevice, const TCreator&, locale<TChar>) -> iostream<TDevice, TChar>;
}