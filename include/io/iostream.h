#pragma once
#include <common/copyable_mutex.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>
#include <io/utilities/stream_common_operators.h>

#include <mutex>
#include <type_traits>
#include <utility>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class iostream : public ios_base<TChar>
               , public io_state_and_exp
               , public istream_operators<TChar>
               , public out_flusher<iostream<TDevice, TChar>>
               , public ostream_operators<TChar>
               , public stream_common_operators
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using in_sentry_type = in_sentry<iostream<TDevice, TChar>, true>;
    using out_sentry_type = out_sentry<iostream<TDevice, TChar>, true>;

    friend in_sentry_type;
    friend out_sentry_type;
    friend istream_operators<TChar>;
    friend out_flusher<iostream<TDevice, TChar>>;
    friend ostream_operators<TChar>;
    friend stream_common_operators;

public:
    iostream()
        : m_streambuf(TDevice()) {}

    iostream(TDevice dev)
        : m_streambuf(std::move(dev)) {}

    template <cvt_creator TCreator>
    iostream(TDevice dev, const TCreator& creator)
        : m_streambuf(std::move(dev), creator) {}

    iostream(TDevice dev, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev))
        , m_locale(std::move(loc)) {}

    template <cvt_creator TCreator>
    iostream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

public:
    /**
     * @lang{ZH}
     * @brief 将底层缓冲区切换到写入方向。
     *
     * 与本流的其它操作一样，本函数持有 `io_mutex()`。这不是可有可无的：切换方向会重定位
     * 转换器、清空读缓冲区（一个 `std::deque`）并翻转转换器的方向标志，而 `in_sentry` /
     * `out_sentry` 在**持有同一把锁**的前提下调用的正是同一批底层函数。若此处不加锁，同一
     * 份状态就存在一条加锁、一条不加锁的访问路径，两者并发即为数据竞争——不只是标志撕裂，
     * 而是那个 `deque` 会被一边 `clear()`、一边 `sgetc()` 读取。
     * @note 流处于失败状态时直接返回、不触碰缓冲区，与 `tell()` / `flush()` 的做法一致。
     * @return 流自身的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the underlying buffer to the put direction.
     *
     * Like every other operation on this stream, this holds `io_mutex()`. That is not
     * optional: switching direction repositions the converter, clears the read buffer (a
     * `std::deque`) and flips the converter's direction flag -- and `in_sentry` / `out_sentry`
     * call those very same underlying functions **while holding that same lock**. Without it
     * here, one piece of state would have both a locked and an unlocked access path, and
     * running them concurrently is a data race -- not merely a torn flag, but that `deque`
     * being `clear()`ed on one side while `sgetc()` reads it on the other.
     * @note Returns without touching the buffer when the stream is in a failed state, matching
     *       what `tell()` and `flush()` do.
     * @return A reference to the stream itself.
     * @endif
     */
    iostream& switch_to_put()
    {
        std::lock_guard guard(this->io_mutex());
        if (!static_cast<bool>(*this)) return *this;
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

    /**
     * @lang{ZH}
     * @brief 将底层缓冲区切换到读取方向。
     *
     * 加锁与失败状态的处理同 `switch_to_put()`，理由亦相同。
     * @return 流自身的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the underlying buffer to the get direction.
     *
     * Locking and failed-state handling are as in `switch_to_put()`, and for the same reasons.
     * @return A reference to the stream itself.
     * @endif
     */
    iostream& switch_to_get()
    {
        std::lock_guard guard(this->io_mutex());
        if (!static_cast<bool>(*this)) return *this;
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
    IOv2::locale<char_type> m_locale;
};

template <io_device TDevice>
iostream(TDevice) -> iostream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
iostream(TDevice, const TCreator&)
    -> iostream<TDevice,
                typename decltype(streambuf{std::declval<TDevice>(),
                                            std::declval<const TCreator&>()})::char_type>;

template <io_device TDevice, typename TChar>
    requires (std::is_same_v<typename TDevice::char_type, TChar>)
iostream(TDevice, locale<TChar>) -> iostream<TDevice, TChar>;

template <io_device TDevice, cvt_creator TCreator, typename TChar>
    requires (std::is_same_v<
                  typename decltype(streambuf{std::declval<TDevice>(),
                                              std::declval<const TCreator&>()})::char_type,
                  TChar>)
iostream(TDevice, const TCreator&, locale<TChar>) -> iostream<TDevice, TChar>;
}