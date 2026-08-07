#pragma once
#include <common/copyable_mutex.h>
#include <common/defs.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <io/traits/traits_base.h>
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
class ostream : public ios_state<TChar>
              , public out_flusher<ostream<TDevice, TChar>>
              , public ostream_operators<TChar>
              , public stream_common_operators
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using out_sentry_type = out_sentry<ostream<TDevice, TChar>, false>;
    using out_iter_type = ostreambuf_iterator<ostreambuf<TDevice, TChar>>;

    friend out_sentry_type;
    friend out_flusher<ostream<TDevice, TChar>>;
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
    ostream()
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
    ostream(TDevice dev)
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
    ostream(TDevice dev, const TCreator& creator)
        : m_streambuf(std::move(dev), creator) {}

    /**
     * @lang{ZH}
     * @brief 以 @p dev 建立流，并装入 locale @p loc。
     * @param dev 底层设备。
     * @param loc 本流的 locale。
     * @throw device_error 若设备无法完成转换器初始化所需的查询；理由见默认构造函数。
     * @note 本重载要求 @p loc 的字符类型与设备的 `char_type` 一致。约束落在**构造函数**上而不是
     *       只落在推导指引上：构造函数生成的隐式推导指引会继承它的约束，故 CTAD 与显式写出
     *       模板实参这两条路径同时被挡住；只约束推导指引则挡不住后者，不匹配的组合会一路走到
     *       转换器内部才失败。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over @p dev with locale @p loc installed.
     * @param dev The underlying device.
     * @param loc The stream's locale.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs; see the default constructor for why this is not reported as state.
     * @note This overload requires @p loc's character type to match the device's `char_type`.
     *       The constraint sits on the **constructor** rather than only on a deduction guide,
     *       because the implicit guide a constructor generates inherits its constraints: that
     *       closes both CTAD and explicitly-written template arguments. Constraining only the
     *       guide leaves the latter open, and a mismatched pair then fails deep inside the
     *       converter instead.
     * @endif
     */
    ostream(TDevice dev, IOv2::locale<char_type> loc)
        requires (std::is_same_v<typename TDevice::char_type, TChar>)
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
     * @note 本重载要求 @p loc 的字符类型与**转换管线产出的** `char_type` 一致——那未必是设备的
     *       字符类型，例如 char 设备配上编码转换器即产出 wchar_t 流。约束为何落在构造函数上，
     *       见只带 locale 的那个重载。
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
     * @note This overload requires @p loc's character type to match the `char_type` the
     *       **converter pipeline produces**, which need not be the device's -- a char device
     *       with a code converter yields a wide stream. See the locale-only overload for why the
     *       constraint sits on the constructor.
     * @endif
     */
    template <cvt_creator TCreator>
    ostream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        requires (std::is_same_v<
                      typename decltype(ostreambuf{std::declval<TDevice>(),
                                              std::declval<const TCreator&>()})::char_type,
                      TChar>)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

private:
    template <typename TLock>
    ostream(TLock&&, const ostream& other)
        : ios_state<TChar>(other)
        , out_flusher<ostream<TDevice, TChar>>(other)
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
     *          `operator=(const ostream&)`。
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
     *          be using. See `operator=(const ostream&)` for the concurrency contract.
     * @note The `noexcept` on move assignment is deliberate: copy assignment's strong guarantee
     *       depends on it (see the `static_assert` there). Taking the lock can formally throw,
     *       but for an already-constructed recursive mutex the only remaining cause is an
     *       exhausted recursion count, which this library treats as unrecoverable -- that is,
     *       it terminates.
     * @endif
     */
    ostream(const ostream& other) : ostream(std::lock_guard{other.io_mutex()}, other) {}
    ostream(ostream&&) = default;

    ostream& operator=(ostream&& other) noexcept
    {
        if (this == &other) return *this;

        std::lock_guard guard(this->io_mutex());
        ios_state<TChar>::operator=(std::move(other));
        out_flusher<ostream<TDevice, TChar>>::operator=(std::move(other));
        ostream_operators<TChar>::operator=(std::move(other));
        stream_common_operators::operator=(std::move(other));
        m_streambuf = std::move(other.m_streambuf);
        m_locale    = std::move(other.m_locale);
        return *this;
    }

    ~ostream() = default;

    /**
     * @lang{ZH}
     * @brief 拷贝赋值；提供强异常保证：先整体拷进临时对象（可能抛出，此时目标尚未被触碰），
     *        再以全程 noexcept 的移动赋值提交。move-only 内核（如 `file_device`）上的拷贝必然
     *        抛出，故自赋值也要先挡掉。
     *
     * @warning 赋值整体替换 `m_streambuf`（内含转换器管线，设备即由其持有）与 `m_locale`
     *          （内含两张哈希表），与 `detach()`/`attach()` 同属生命周期操作，区别在于它涉及
     *          **两个**操作数。两个操作数都受 `io_mutex()` 保护：拷贝构造持有源流的锁读出副本，
     *          随后的移动赋值持有目标流的锁完成替换。两把锁**先后获取、互不重叠**，因此任一
     *          线程在任一时刻至多持有一把流锁。仍由调用方负责的是：不得在其它线程仍可能使用
     *          某流时销毁它、或把它作为移动的源。另需注意，赋值由此参与调用方自身的锁定序——
     *          `sync(P); X = Q;` 与 `sync(Q); Y = P;` 反向配对仍会死锁，这与 `sync` 一贯的
     *          表述一致。
     * @endif
     *
     * @lang{EN}
     * @brief Copy assignment with the strong exception guarantee: the copy is made into a
     *        temporary first (which may throw, with the destination still untouched), and the
     *        commit is a move assignment, noexcept throughout. A copy always throws on a
     *        move-only kernel (`file_device`), which is why self-assignment is short-circuited.
     *
     * @warning Assignment replaces `m_streambuf` (which holds the converter pipeline, and the
     *          device through it) and `m_locale` (which holds two hash tables) wholesale, and
     *          is a lifecycle operation just like `detach()`/`attach()`, except that it
     *          involves **two** operands. Both are covered by `io_mutex()`: the copy
     *          construction holds the source's lock while reading it out, and the move
     *          assignment that follows holds the destination's lock while replacing it. The two
     *          locks are taken **in sequence and never overlap**, so a thread holds at most one
     *          stream lock at any instant. What remains the caller's responsibility: never
     *          destroy, or move from, a stream another thread may still be using. Note also
     *          that assignment thereby joins the caller's own lock order -- `sync(P); X = Q;`
     *          paired with `sync(Q); Y = P;` still deadlocks, exactly as documented for `sync`.
     * @endif
     */
    ostream& operator=(const ostream& other)
    {
        static_assert(std::is_nothrow_move_assignable_v<ostream<TDevice, TChar>>,
                      "the noexcept on move assignment must stay: it is what keeps the commit "
                      "step below from leaving *this half-updated");
        if (this != &other)
        {
            ostream tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

private:
    ostreambuf<TDevice, TChar> m_streambuf;
    IOv2::locale<char_type> m_locale;
};

// Only these two need a guide: TChar appears nowhere in their parameters, so the implicit guide
// cannot deduce it. The two overloads that take a locale deduce TChar from it through their own
// implicit guide, which inherits the constraint written on the constructor -- repeating that
// constraint here would just duplicate it, and the constructor's copy is the one that also covers
// explicitly-written template arguments.
template <io_device TDevice>
ostream(TDevice) -> ostream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
ostream(TDevice, const TCreator&)
    -> ostream<TDevice,
               typename decltype(ostreambuf{std::declval<TDevice>(),
                                            std::declval<const TCreator&>()})::char_type>;

// common manips
/**
 * @lang{ZH}
 * @brief 写出一个换行符并刷新本流的操纵符对象，用法为 `os << IOv2::endl`；类型是个空标签，
 *        逻辑全在 `io_traits<TChar, endl_t>::swrite` 里。
 *
 * 方向被编码进类型本身：`io_traits<TChar, endl_t>` 只提供 `swrite`，`is >> endl` 因此不满足提取
 * 运算符的约束、没有可行重载，编译不过。方向为何要这样表达，见 `io_traits<TChar, endl_t>`。
 * @note 本类型没有 `operator()`：单向操纵符的逻辑只需写一处，直接写在扩展点里即可，`os << endl`
 *       于是成为唯一入口，异常统一由插入运算符转交 `handle_exception`。标准的 `std::endl(os)`
 *       直接调用形式因此不存在。
 * @endif
 *
 * @lang{EN}
 * @brief The manipulator object that writes a newline and flushes the stream, used as
 *        `os << IOv2::endl`; its type is an empty tag, with all the logic in
 *        `io_traits<TChar, endl_t>::swrite`.
 *
 * The direction is encoded in the type itself: `io_traits<TChar, endl_t>` provides only
 * `swrite`, so `is >> endl` leaves the extraction operator unsatisfied -- no viable overload, and
 * it does not compile. See `io_traits<TChar, endl_t>` for why the direction is expressed this way.
 * @note This type has no `operator()`: a one-way manipulator needs its logic in one place only,
 *       so it lives in the extension point directly. That makes `os << endl` the sole entry and
 *       leaves exceptions to the insertion operator and `handle_exception`. The standard's
 *       `std::endl(os)` direct-call form therefore does not exist.
 * @endif
 */
inline constexpr struct endl_t {} endl{};

/**
 * @lang{ZH}
 * @brief `endl` 的扩展点特化：只提供 `swrite`，写出一个换行符并刷新本流。
 *
 * 操纵符的方向由**成员的有无**表达：这里只有 `swrite`，于是 `is >> endl` 不满足提取运算符的
 * 约束、没有可行重载。改用约束是不够的——`iostream` 同时满足两个流概念，若两侧都提供
 * 成员，两个方向都会调用成功。
 *
 * @note 本操纵符**不触碰格式标志**。强制刷新是经 `put()` 的 `force_flush` 参数传给
 *       `out_sentry` 的，而不是临时置位再复位 `unitbuf`：后者的读-改-写跨越了中间的
 *       `put()`，既非原子也非异常安全——并发下两个线程各自读到对方的中间态，其中一个会
 *       静默地不刷新；`ctype` facet 缺失等异常路径上则会把 `unitbuf` 永久遗留下来，此后
 *       每一次输出都被降级为逐字符刷新。顺带地，其它线程也不会再观察到本流的格式标志被
 *       临时改动。
 * @note 整个操纵符在一把 `io_mutex()` 之下完成：`locale` 的读取与随后的 `put()` 之间没有窗口，
 *       因此 `endl` 是原子的。`locale(loc)` setter 会 move-assign `m_locale`，而 `locale` 自身
 *       的移动赋值不加任何锁——锁外读取即为对其内部两个哈希表的数据竞争。`put()` 内部会在这把
 *       递归锁上重入，无碍。
 * @note 抛出在这把锁内被接住并交给 `handle_exception`：`operator<<` 那层 `catch` 在锁之外，
 *       只靠它的话置位会发生在解锁之后。掩码命中时这里同样会重抛，异常继续逃到运算符那层再处理
 *       一遍；`put()` 抛出的更是三处依次处理（`put()` 自己、这里、运算符）。都是无害的，
 *       `handle_exception` 是幂等的。
 * @param os 目标输出流。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `endl`: provides only `swrite`, which writes a
 *        newline and flushes the stream.
 *
 * A manipulator's direction is expressed by **which member exists**: only `swrite` is here, so
 * `is >> endl` leaves the extraction operator unsatisfied and there is no viable overload. A
 * constraint would not be enough -- `iostream` satisfies both stream concepts, so if both members
 * existed both directions would call successfully.
 *
 * @note This manipulator **does not touch the format flags**. The forced flush is handed to
 *       `out_sentry` through `put()`'s `force_flush` argument rather than by setting and
 *       clearing `unitbuf` around the call: that read-modify-write straddles the `put()` and
 *       is neither atomic nor exception-safe -- concurrently, two threads each observe the
 *       other's intermediate state and one silently skips its flush, while on an exception
 *       path (a missing `ctype` facet, say) `unitbuf` is left set for good, degrading every
 *       later write to a per-character flush. As a bonus, other threads no longer observe this
 *       stream's format flags being perturbed.
 * @note The whole manipulator runs under a single `io_mutex()`: there is no window between
 *       reading the locale and the `put()` that follows, so `endl` is atomic. The `locale(loc)`
 *       setter move-assigns `m_locale`, and locale's own move-assignment takes no lock
 *       whatsoever, so reading it outside that lock is a data race on its two internal hash
 *       maps. `put()` re-enters the same recursive mutex, which is harmless.
 * @note A throw is caught under this same lock and handed to `handle_exception`: `operator<<`'s
 *       own `catch` sits outside it, so relying on that one alone would set the state bits after
 *       the unlock. On a masked bit this rethrows in turn and the exception goes on to be handled
 *       once more by the operator; one thrown out of `put()` passes three handling points in a row
 *       (`put()` itself, here, and the operator). All of it is harmless: `handle_exception` is
 *       idempotent.
 * @param os The target output stream.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, endl_t>
{
    template <ostream_type T>
    static void swrite(T& os, const endl_t&)
    {
        std::lock_guard guard(os.io_mutex());
        try
        {
            auto mp = os.locale().template get<ctype<TChar>>();
            if (!mp)
                throw stream_error("endl fail: cannot get ctype facet");

            os.put(mp->widen('\n'), /*force_flush=*/true);
        }
        catch(...)
        {
            os.handle_exception(std::current_exception());
        }
    }
};

/**
 * @lang{ZH}
 * @brief 写出一个空字符的操纵符对象，用法为 `os << IOv2::ends`；类型是个空标签，逻辑全在
 *        `io_traits<TChar, ends_t>::swrite` 里。方向为何要用 `io_traits` 成员的有无来表达、
 *        又为何不留 `operator()`，见 `endl`。
 * @endif
 *
 * @lang{EN}
 * @brief The manipulator object that writes a null character, used as `os << IOv2::ends`; its
 *        type is an empty tag, with all the logic in `io_traits<TChar, ends_t>::swrite`. See
 *        `endl` for why the direction is expressed by which `io_traits` member exists and why
 *        there is no `operator()`.
 * @endif
 */
inline constexpr struct ends_t {} ends{};

/**
 * @lang{ZH}
 * @brief `ends` 的扩展点特化：只提供 `swrite`，写出一个空字符（`TChar()`），不强制刷新。
 *        方向为何这样表达，见 `io_traits<TChar, endl_t>`。
 * @note 无需显式加锁，也无需 `try`：`ostream_operators::put()` 自己持有 `io_mutex()`，并已将
 *       异常交由 `handle_exception` 处理。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `ends`: provides only `swrite`, which writes a null
 *        character (`TChar()`) without forcing a flush. See `io_traits<TChar, endl_t>` for why the
 *        direction is expressed this way.
 * @note Neither an explicit lock nor a `try` is needed: `ostream_operators::put()` holds
 *       `io_mutex()` itself and already routes exceptions through `handle_exception`.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, ends_t>
{
    template <ostream_type T>
    static void swrite(T& os, const ends_t&) { os.put(TChar()); }
};

/**
 * @lang{ZH}
 * @brief 刷新流的操纵符对象，用法为 `os << IOv2::flush`；类型是个空标签，逻辑全在
 *        `io_traits<TChar, flush_t>::swrite` 里。方向为何要用 `io_traits` 成员的有无来表达、
 *        又为何不留 `operator()`，见 `endl`。
 * @endif
 *
 * @lang{EN}
 * @brief The manipulator object that flushes the stream, used as `os << IOv2::flush`; its type is
 *        an empty tag, with all the logic in `io_traits<TChar, flush_t>::swrite`. See `endl` for
 *        why the direction is expressed by which `io_traits` member exists and why there is no
 *        `operator()`.
 * @endif
 */
inline constexpr struct flush_t {} flush{};

/**
 * @lang{ZH}
 * @brief `flush` 的扩展点特化：只提供 `swrite`，刷新本流。方向为何这样表达，见
 *        `io_traits<TChar, endl_t>`。
 * @note 无需显式加锁，也无需 `try`：`ostream_operators::flush()` 自己持有 `io_mutex()`，并已将
 *       异常交由 `handle_exception` 处理。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `flush`: provides only `swrite`, which flushes the
 *        stream. See `io_traits<TChar, endl_t>` for why the direction is expressed this way.
 * @note Neither an explicit lock nor a `try` is needed: `ostream_operators::flush()` holds
 *       `io_mutex()` itself and already routes exceptions through `handle_exception`.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, flush_t>
{
    template <ostream_type T>
    static void swrite(T& os, const flush_t&) { os.flush(); }
};

// https://github.com/gcc-mirror/gcc/blob/075ec330307c5b1fe5ed166a633c718c06b01437/libstdc%2B%2B-v3/include/bits/ostream.h#L80
// TODO: add quoted
// https://github.com/gcc-mirror/gcc/blob/075ec330307c5b1fe5ed166a633c718c06b01437/libstdc%2B%2B-v3/include/std/iomanip#L88
}