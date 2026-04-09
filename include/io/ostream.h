#pragma once
#include <functional>
#include <mutex>

#include <io/io_concepts.h>
#include <common/defs.h>
#include <locale/locale.h>
#include <io/utilities/ostream_operators.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>
#include <io/fp_defs/base_fp.h>

#include <io/utilities/ostream_operators.h>
#include <io/utilities/stream_common_operators.h>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class ostream : public ios_base<TChar>
              , public ostream_operators<ostream<TDevice, TChar>, TChar>
              , public stream_common_operators<ostream<TDevice, TChar>, TDevice, TChar>
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using out_sentry_type = out_sentry<ostream<TDevice, TChar>, false>;

    friend out_sentry_type;
    friend ostream_operators<ostream<TDevice, TChar>, TChar>;
    friend stream_common_operators<ostream<TDevice, TChar>, TDevice, TChar>;

public:
    ostream()
        : m_streambuf(TDevice()) {}

    ostream(TDevice dev)
        : m_streambuf(std::move(dev)) {}

    template <cvt_creator TCreator>
    ostream(TDevice dev, const TCreator& creator)
        : m_streambuf(std::move(dev), creator) {}

    ostream(TDevice dev, locale<char_type> loc)
        : m_streambuf(std::move(dev))
        , m_locale(std::move(loc)) {}

    template <cvt_creator TCreator>
    ostream(TDevice dev, const TCreator& creator, locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

private:
    ostreambuf<TDevice, TChar> m_streambuf;
    abs_ostream* m_tie_stream = nullptr;
    locale<char_type> m_locale;
    std::mutex        m_io_mutex;
};

template <io_device TDevice>
ostream(TDevice) -> ostream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
ostream(TDevice, const TCreator&) -> ostream<TDevice,
                                             ext_to_int<TDevice, TCreator>>;

template <io_device TDevice, typename TChar>
    requires (std::is_same_v<typename TDevice::char_type, TChar>)
ostream(TDevice, locale<TChar>) -> ostream<TDevice, TChar>;

template <io_device TDevice, cvt_creator TCreator, typename TChar>
    requires (std::is_same_v<ext_to_int<TDevice, TCreator>, TChar>)
ostream(TDevice, const TCreator&, locale<TChar>) -> ostream<TDevice, TChar>;

// common manips
template <ostream_type T>
inline void endl(T& os)
{
    using TChar = typename T::char_type;
    const bool b_unitbuf = (os.flags() & ios_defs::unitbuf);
    unitbuf(os);

    const auto& loc = os.locale();
    auto mp = loc.template get<ctype<TChar>>();
    if (!mp)
        throw stream_error("cannot get numeric facet");
    os.put(mp->widen('\n'));

    if (!b_unitbuf)
        nounitbuf(os);
}

template <ostream_type T>
inline void ends(T& os)
{
    using TChar = typename T::char_type;
    os.put(TChar());
}

template <ostream_type T>
inline void flush(T& os)
{
    os.flush();
}

// https://github.com/gcc-mirror/gcc/blob/075ec330307c5b1fe5ed166a633c718c06b01437/libstdc%2B%2B-v3/include/bits/ostream.h#L80
// TODO: add quoted
// https://github.com/gcc-mirror/gcc/blob/075ec330307c5b1fe5ed166a633c718c06b01437/libstdc%2B%2B-v3/include/std/iomanip#L88
}