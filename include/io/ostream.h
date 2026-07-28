#pragma once
#include <common/copyable_mutex.h>
#include <common/defs.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <io/fp_defs/base_fp.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>
#include <io/utilities/ostream_operators.h>
#include <io/utilities/stream_common_operators.h>
#include <locale/locale.h>

#include <mutex>
#include <type_traits>
#include <utility>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class ostream : public ios_base<TChar>
              , public io_state_and_exp
              , public out_flusher<ostream<TDevice, TChar>>
              , public ostream_operators<TChar>
              , public stream_common_operators
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using out_sentry_type = out_sentry<ostream<TDevice, TChar>, false>;

    friend out_sentry_type;
    friend out_flusher<ostream<TDevice, TChar>>;
    friend ostream_operators<TChar>;
    friend stream_common_operators;

public:
    ostream()
        : m_streambuf(TDevice()) {}

    ostream(TDevice dev)
        : m_streambuf(std::move(dev)) {}

    template <cvt_creator TCreator>
    ostream(TDevice dev, const TCreator& creator)
        : m_streambuf(std::move(dev), creator) {}

    ostream(TDevice dev, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev))
        , m_locale(std::move(loc)) {}

    template <cvt_creator TCreator>
    ostream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

private:
    ostreambuf<TDevice, TChar> m_streambuf;
    IOv2::locale<char_type> m_locale;
};

template <io_device TDevice>
ostream(TDevice) -> ostream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
ostream(TDevice, const TCreator&)
    -> ostream<TDevice,
               typename decltype(ostreambuf{std::declval<TDevice>(),
                                            std::declval<const TCreator&>()})::char_type>;

template <io_device TDevice, typename TChar>
    requires (std::is_same_v<typename TDevice::char_type, TChar>)
ostream(TDevice, locale<TChar>) -> ostream<TDevice, TChar>;

template <io_device TDevice, cvt_creator TCreator, typename TChar>
    requires (std::is_same_v<
                  typename decltype(ostreambuf{std::declval<TDevice>(),
                                               std::declval<const TCreator&>()})::char_type,
                  TChar>)
ostream(TDevice, const TCreator&, locale<TChar>) -> ostream<TDevice, TChar>;

// common manips
/**
 * @lang{ZH}
 * @brief 写出一个换行符并刷新本流。
 *
 * @note 本操纵符**不触碰格式标志**。强制刷新是经 `put()` 的 `force_flush` 参数传给
 *       `out_sentry` 的，而不是临时置位再复位 `unitbuf`：后者的读-改-写跨越了中间的
 *       `put()`，既非原子也非异常安全——并发下两个线程各自读到对方的中间态，其中一个会
 *       静默地不刷新；`ctype` facet 缺失等异常路径上则会把 `unitbuf` 永久遗留下来，此后
 *       每一次输出都被降级为逐字符刷新。顺带地，其它线程也不会再观察到本流的格式标志被
 *       临时改动。
 * @note `locale` 在持有 `io_mutex()` 的情况下读取。`locale(loc)` setter 会 move-assign
 *       `m_locale`，而 `locale` 自身的移动赋值不加任何锁——锁外读取即为对其内部两个哈希表
 *       的数据竞争。
 * @warning 那把锁**必须在 `put()` 之前释放**。`out_sentry` 在获取 `io_mutex()` 之前先
 *          `tie()->flush()` 目标流；若此处仍持有本流的锁，该步骤会让线程同时持有两把流锁，
 *          破坏 sentry 文档所依赖的"任一时刻本线程至多持有一把流锁"这一不死锁保证。
 * @param os 目标输出流。
 * @throw stream_error 若缺少用于宽化 `'\n'` 的 ctype facet。
 * @endif
 *
 * @lang{EN}
 * @brief Writes a newline and flushes the stream.
 *
 * @note This manipulator **does not touch the format flags**. The forced flush is handed to
 *       `out_sentry` through `put()`'s `force_flush` argument rather than by setting and
 *       clearing `unitbuf` around the call: that read-modify-write straddles the `put()` and
 *       is neither atomic nor exception-safe -- concurrently, two threads each observe the
 *       other's intermediate state and one silently skips its flush, while on an exception
 *       path (a missing `ctype` facet, say) `unitbuf` is left set for good, degrading every
 *       later write to a per-character flush. As a bonus, other threads no longer observe this
 *       stream's format flags being perturbed.
 * @note The locale is read while holding `io_mutex()`. The `locale(loc)` setter move-assigns
 *       `m_locale`, and locale's own move-assignment takes no lock whatsoever, so reading it
 *       outside that lock is a data race on its two internal hash maps.
 * @warning That lock **must be released before `put()`**. `out_sentry` flushes the tied stream
 *          before acquiring `io_mutex()`; holding this stream's lock across it would leave the
 *          thread holding two stream locks at once, breaking the "at most one stream lock per
 *          thread" no-deadlock guarantee the sentry documentation relies on.
 * @param os The target output stream.
 * @throw stream_error If the ctype facet used to widen `'\n'` is missing.
 * @endif
 */
template <ostream_type T>
inline void endl(T& os)
{
    using TChar = typename T::char_type;

    TChar nl;
    {
        std::lock_guard guard(os.io_mutex());
        auto mp = os.locale().template get<ctype<TChar>>();
        if (!mp)
            throw stream_error("endl fail: cannot get ctype facet");
        nl = mp->widen('\n');
    }

    os.put(nl, /*force_flush=*/true);
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