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
    using in_iter_type = istreambuf_iterator<streambuf<TDevice, TChar>>;
    using out_iter_type = ostreambuf_iterator<streambuf<TDevice, TChar>>;

    friend in_sentry_type;
    friend out_sentry_type;
    friend istream_operators<TChar>;
    friend out_flusher<iostream<TDevice, TChar>>;
    friend ostream_operators<TChar>;
    friend stream_common_operators;

public:
    /**
     * @lang{ZH}
     * @brief 以默认构造的设备建立流。
     *
     * 供"先默认构造、之后 `attach()` 装设备"这一用法。
     *
     * @warning **构造流可能抛出异常，且异常不会被转成失败位。** 建流要初始化转换器，而这一步
     *          会向设备询问流起点；设备若尚未就绪（例如默认构造的 `file_device` 对应一个未打开
     *          的文件），询问就会抛出 `device_error`。异常来自成员初始化列表，C++ 规定它必然向
     *          外传播——构造函数 function-try-block 的处理器执行到末尾时会自动重抛，其中也不允许
     *          `return`——而且此时流对象尚未诞生，也就无处安放状态位。本库其余的操作（包括
     *          `attach()`）都把异常转为状态位，唯独构造不能，调用方需要自行 `try`。
     * @note 设备本身可用时（如 `mem_device`）默认构造不会抛，得到的是一个可直接使用的空流。
     * @throw device_error 若设备无法完成转换器初始化所需的查询。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over a default-constructed device.
     *
     * For the "default-construct now, `attach()` a device later" usage.
     *
     * @warning **Constructing a stream may throw, and the exception is not turned into a failure
     *          bit.** Building a stream initializes the converter, and that step asks the device
     *          for the stream origin; a device that is not ready yet -- a default-constructed
     *          `file_device`, which refers to no open file -- throws `device_error` when asked.
     *          The exception comes from the member initializer list, which C++ requires to
     *          propagate (the handler of a constructor function-try-block rethrows when control
     *          reaches its end, and a `return` is not allowed there), and the stream object does
     *          not exist yet, so there is nowhere to put a state bit. Every other operation in
     *          this library, `attach()` included, turns exceptions into state; construction alone
     *          cannot, so the caller must `try` around it.
     * @note With a device that is usable as constructed (`mem_device`), this does not throw and
     *       yields an empty stream ready for use.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs.
     * @endif
     */
    iostream()
        : m_streambuf(TDevice()) {}

    /**
     * @lang{ZH}
     * @brief 以 @p dev 建立流。
     * @param dev 底层设备。
     * @throw device_error 若设备无法完成转换器初始化所需的查询；理由见默认构造函数。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over @p dev.
     * @param dev The underlying device.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs; see the default constructor for why this is not reported as state.
     * @endif
     */
    iostream(TDevice dev)
        : m_streambuf(std::move(dev)) {}

    /**
     * @lang{ZH}
     * @brief 以 @p dev 建立流，并由 @p creator 构造转换器。
     * @tparam TCreator 转换器创建器类型。
     * @param dev     底层设备。
     * @param creator 转换器创建器。
     * @throw device_error 若设备无法完成转换器初始化所需的查询；理由见默认构造函数。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over @p dev with the converter built by @p creator.
     * @tparam TCreator The converter-creator type.
     * @param dev     The underlying device.
     * @param creator The converter creator.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs; see the default constructor for why this is not reported as state.
     * @endif
     */
    template <cvt_creator TCreator>
    iostream(TDevice dev, const TCreator& creator)
        : m_streambuf(std::move(dev), creator) {}

    /**
     * @lang{ZH}
     * @brief 以 @p dev 建立流，并装入 locale @p loc。
     * @param dev 底层设备。
     * @param loc 本流的 locale。
     * @throw device_error 若设备无法完成转换器初始化所需的查询；理由见默认构造函数。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over @p dev with locale @p loc installed.
     * @param dev The underlying device.
     * @param loc The stream's locale.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs; see the default constructor for why this is not reported as state.
     * @endif
     */
    iostream(TDevice dev, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev))
        , m_locale(std::move(loc)) {}

    /**
     * @lang{ZH}
     * @brief 以 @p dev 建立流，由 @p creator 构造转换器，并装入 locale @p loc。
     * @tparam TCreator 转换器创建器类型。
     * @param dev     底层设备。
     * @param creator 转换器创建器。
     * @param loc     本流的 locale。
     * @throw device_error 若设备无法完成转换器初始化所需的查询；理由见默认构造函数。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over @p dev with the converter built by @p creator and locale
     *        @p loc installed.
     * @tparam TCreator The converter-creator type.
     * @param dev     The underlying device.
     * @param creator The converter creator.
     * @param loc     The stream's locale.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs; see the default constructor for why this is not reported as state.
     * @endif
     */
    template <cvt_creator TCreator>
    iostream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

private:
    template <typename TLock>
    iostream(TLock&&, const iostream& other)
        : ios_base<TChar>(other)
        , io_state_and_exp(other)
        , istream_operators<TChar>(other)
        , out_flusher<iostream<TDevice, TChar>>(other)
        , ostream_operators<TChar>(other)
        , stream_common_operators(other)
        , m_streambuf(other.m_streambuf)
        , m_locale(other.m_locale) {}

public:
    /**
     * @lang{ZH}
     * @brief 拷贝构造、移动构造与移动赋值。
     * @warning 拷贝构造全程持有**源流**的 `io_mutex()`，移动赋值全程持有**目标流**的
     *          `io_mutex()`；移动构造不加锁，且移动会把源流置于移后状态。仍由调用方负责的是：
     *          不得在其它线程仍可能使用某流时销毁它、或把它作为移动的源。并发契约详见
     *          `operator=(const iostream&)`。
     * @note 移动赋值的 `noexcept` 是有意为之：拷贝赋值的强异常保证依赖它（见其中的
     *       `static_assert`）。加锁在形式上可抛，但对一把已构造的递归互斥量而言只剩"递归计数
     *       耗尽"这一种可能，本库将其视为不可恢复，即 `terminate`。
     * @endif
     *
     * @lang{EN}
     * @brief Copy construction, move construction and move assignment.
     * @warning Copy construction holds the **source's** `io_mutex()` throughout, and move
     *          assignment holds the **destination's** throughout; move construction takes no
     *          lock, and a move leaves the source moved-from. What remains the caller's
     *          responsibility: never destroy, or move from, a stream another thread may still
     *          be using. See `operator=(const iostream&)` for the concurrency contract.
     * @note The `noexcept` on move assignment is deliberate: copy assignment's strong guarantee
     *       depends on it (see the `static_assert` there). Taking the lock can formally throw,
     *       but for an already-constructed recursive mutex the only remaining cause is an
     *       exhausted recursion count, which this library treats as unrecoverable -- that is,
     *       it terminates.
     * @endif
     */
    iostream(const iostream& other) : iostream(std::lock_guard{other.io_mutex()}, other) {}
    iostream(iostream&&) = default;

    iostream& operator=(iostream&& other) noexcept
    {
        if (this == &other) return *this;

        std::lock_guard guard(this->io_mutex());
        ios_base<TChar>::operator=(std::move(other));
        io_state_and_exp::operator=(std::move(other));
        istream_operators<TChar>::operator=(std::move(other));
        out_flusher<iostream<TDevice, TChar>>::operator=(std::move(other));
        ostream_operators<TChar>::operator=(std::move(other));
        stream_common_operators::operator=(std::move(other));
        m_streambuf = std::move(other.m_streambuf);
        m_locale    = std::move(other.m_locale);
        return *this;
    }

    ~iostream() = default;

    /**
     * @lang{ZH}
     * @brief 拷贝赋值；提供强异常保证：先整体拷进临时对象（可能抛出，此时目标尚未被触碰），
     *        再以全程 noexcept 的移动赋值提交。move-only 内核（如 `file_device`）上的拷贝必然
     *        抛出，故自赋值也要先挡掉。
     *
     * @warning 赋值整体替换 `m_streambuf`（内含转换器管线——方向状态即在其中——与一个
     *          `std::deque` 读缓冲区）与 `m_locale`（内含两张哈希表），与 `detach()`/`attach()`
     *          同属生命周期操作，区别在于它涉及**两个**操作数。两个操作数都受 `io_mutex()`
     *          保护：拷贝构造持有源流的锁读出副本，随后的移动赋值持有目标流的锁完成替换。两把
     *          锁**先后获取、互不重叠**，因此任一线程在任一时刻至多持有一把流锁。仍由调用方
     *          负责的是：不得在其它线程仍可能使用某流时销毁它、或把它作为移动的源。另需注意，
     *          赋值由此参与调用方自身的锁定序——`sync(P); X = Q;` 与 `sync(Q); Y = P;` 反向
     *          配对仍会死锁，这与 `sync` 一贯的表述一致。
     * @endif
     *
     * @lang{EN}
     * @brief Copy assignment with the strong exception guarantee: the copy is made into a
     *        temporary first (which may throw, with the destination still untouched), and the
     *        commit is a move assignment, noexcept throughout. A copy always throws on a
     *        move-only kernel (`file_device`), which is why self-assignment is short-circuited.
     *
     * @warning Assignment replaces `m_streambuf` (which holds the converter pipeline, the
     *          direction state included, and a `std::deque` read buffer) and `m_locale` (which
     *          holds two hash tables) wholesale, and is a lifecycle operation just like
     *          `detach()`/`attach()`, except that it involves **two** operands. Both are
     *          covered by `io_mutex()`: the copy construction holds the source's lock while
     *          reading it out, and the move assignment that follows holds the destination's
     *          lock while replacing it. The two locks are taken **in sequence and never
     *          overlap**, so a thread holds at most one stream lock at any instant. What
     *          remains the caller's responsibility: never destroy, or move from, a stream
     *          another thread may still be using. Note also that assignment thereby joins the
     *          caller's own lock order -- `sync(P); X = Q;` paired with `sync(Q); Y = P;` still
     *          deadlocks, exactly as documented for `sync`.
     * @endif
     */
    iostream& operator=(const iostream& other)
    {
        static_assert(std::is_nothrow_move_assignable_v<iostream<TDevice, TChar>>,
                      "copy assignment's strong guarantee requires a noexcept move assignment");
        if (this != &other)
        {
            iostream tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

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