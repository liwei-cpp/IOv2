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
    ostream(TDevice dev, IOv2::locale<char_type> loc)
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
    ostream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

    /**
     * @lang{ZH}
     * @brief 拷贝构造、移动构造与移动赋值。
     * @warning 与拷贝赋值一样，这三者都是**不同步**的生命周期操作，不持有 `io_mutex()`；
     *          移动还会把源流置于移后状态。并发契约详见 `operator=(const ostream&)`。
     * @endif
     *
     * @lang{EN}
     * @brief Copy construction, move construction and move assignment.
     * @warning Like copy assignment, all three are **unsynchronized** lifecycle operations that
     *          do not hold `io_mutex()`; a move additionally leaves the source moved-from. See
     *          `operator=(const ostream&)` for the concurrency contract.
     * @endif
     */
    ostream(const ostream&) = default;
    ostream(ostream&&) = default;
    ostream& operator=(ostream&&) = default;
    ~ostream() = default;

    /**
     * @lang{ZH}
     * @brief 拷贝赋值；提供强异常保证：先整体拷进临时对象（可能抛出，此时目标尚未被触碰），
     *        再以全程 noexcept 的移动赋值提交。move-only 内核（如 `file_device`）上的拷贝必然
     *        抛出，故自赋值也要先挡掉。
     *
     * @warning 本操作**不做线程同步**：与本流的其它操作（`tell`/`seek`/格式化 I/O 等均持有
     *          `io_mutex()`）不同，赋值不获取任何锁——它整体替换 `m_streambuf`（内含转换器
     *          管线，设备即由其持有）与 `m_locale`（内含两张哈希表），与 `detach()`/`attach()`
     *          同属生命周期操作。区别在于赋值涉及**两个**操作数，**两者都要独占**：调用期间
     *          源与目标上都不得有其它线程进行操作（读、写、attach/detach 或再次赋值），否则
     *          行为未定义。需要在并发环境下更换流的内容，请由调用方自行串行化。
     * @endif
     *
     * @lang{EN}
     * @brief Copy assignment with the strong exception guarantee: the copy is made into a
     *        temporary first (which may throw, with the destination still untouched), and the
     *        commit is a move assignment, noexcept throughout. A copy always throws on a
     *        move-only kernel (`file_device`), which is why self-assignment is short-circuited.
     *
     * @warning This operation is **not synchronized**: unlike the stream's other operations
     *          (`tell`/`seek`/formatted I/O, which all hold `io_mutex()`), assignment takes no
     *          lock -- it replaces `m_streambuf` (which holds the converter pipeline, and the
     *          device through it) and `m_locale` (which holds two hash tables) wholesale, and
     *          is a lifecycle operation just like `detach()`/`attach()`. The difference is
     *          that assignment involves **two** operands and **both** must be exclusively
     *          owned: no other thread may operate on either the source or the destination
     *          (reading, writing, attach/detach, or another assignment) while it runs, or the
     *          behavior is undefined. Serialize in the caller when stream contents must be
     *          replaced concurrently.
     * @endif
     */
    ostream& operator=(const ostream& other)
    {
        static_assert(std::is_nothrow_move_assignable_v<ostream<TDevice, TChar>>,
                      "copy assignment's strong guarantee requires a noexcept move assignment");
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
 * @brief `endl` 操纵符的类型。
 *
 * 方向被编码进类型本身：只有 `operator<<` 接受本类型，对应的 `operator>>` 重载在
 * `io/utilities/istream_operators.h` 中被删除。方向为何必须编码进类型，见 `in_manip`。
 *
 * 保留 `operator()` 是为了与标准的 `std::endl(os)` 直接调用形式对齐。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `endl` manipulator.
 *
 * The direction is encoded in the type itself: only `operator<<` accepts this type, and the
 * matching `operator>>` overload is deleted in `io/utilities/istream_operators.h`. See
 * `in_manip` for why the direction must live in the type.
 *
 * `operator()` is kept to match the standard's `std::endl(os)` direct-call form.
 * @endif
 */
struct _Endl : out_manip
{
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
 * @note 取 facet 这一段的异常在此处就地交给 `handle_exception`，而不是留给 `operator<<`。
 *       直接调用形式 `IOv2::endl(os)` 绕过运算符，若不在此处理，facet 缺失就会既不置失败位
 *       也不受异常掩码约束地抛到调用方，流反而报告 `good()`。随后的 `put()` 自己已经处理
 *       异常，不再重复包裹。
 * @param os 目标输出流。
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
 * @note Exceptions from the facet lookup are handed to `handle_exception` right here rather
 *       than left to `operator<<`. The direct-call form `IOv2::endl(os)` bypasses the operator,
 *       so without this a missing facet would escape to the caller with no failure bit set and
 *       no regard for the exception mask, leaving the stream reporting `good()`. The `put()`
 *       that follows already handles its own exceptions and is not wrapped again.
 * @param os The target output stream.
 * @endif
 */
    template <ostream_type T>
    void operator () (T& os) const
    {
        using TChar = typename T::char_type;

        TChar nl;
        try
        {
            std::lock_guard guard(os.io_mutex());
            auto mp = os.locale().template get<ctype<TChar>>();
            if (!mp)
                throw stream_error("endl fail: cannot get ctype facet");
            nl = mp->widen('\n');
        }
        catch (...)
        {
            os.handle_exception(std::current_exception());
            return;
        }

        os.put(nl, /*force_flush=*/true);
    }
};

/**
 * @lang{ZH}
 * @brief 写出一个换行符并刷新本流的操纵符对象。
 *
 * 用法为 `os << IOv2::endl`；亦支持标准的直接调用形式 `IOv2::endl(os)`。
 * @endif
 *
 * @lang{EN}
 * @brief The manipulator object that writes a newline and flushes the stream.
 *
 * Use as `os << IOv2::endl`; the standard's direct-call form `IOv2::endl(os)` also works.
 * @endif
 */
inline constexpr _Endl endl{};

/**
 * @lang{ZH}
 * @brief `ends` 操纵符的类型。方向为何必须编码进类型，见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `ends` manipulator. See `in_manip` for why the direction must live in
 *        the type.
 * @endif
 */
struct _Ends : out_manip
{
    /**
     * @lang{ZH}
     * @brief 写出一个空字符（`TChar()`），不强制刷新。
     * @note 无需显式加锁，也无需 `try`：`put()` 自己的 `out_sentry` 会获取 `io_mutex()`，
     *       并且 `put()` 已将异常交由 `handle_exception` 处理。
     * @param os 目标输出流。
     * @endif
     *
     * @lang{EN}
     * @brief Writes a null character (`TChar()`) without forcing a flush.
     * @note Neither an explicit lock nor a `try` is needed: `put()`'s own `out_sentry` acquires
     *       `io_mutex()`, and `put()` already routes exceptions through `handle_exception`.
     * @param os The target output stream.
     * @endif
     */
    template <ostream_type T>
    void operator () (T& os) const
    {
        using TChar = typename T::char_type;
        os.put(TChar());
    }
};

/**
 * @lang{ZH}
 * @brief 写出一个空字符的操纵符对象。用法为 `os << IOv2::ends` 或 `IOv2::ends(os)`。
 * @endif
 *
 * @lang{EN}
 * @brief The manipulator object that writes a null character. Use as `os << IOv2::ends` or
 *        `IOv2::ends(os)`.
 * @endif
 */
inline constexpr _Ends ends{};

/**
 * @lang{ZH}
 * @brief `flush` 操纵符的类型。方向为何必须编码进类型，见 `in_manip`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `flush` manipulator. See `in_manip` for why the direction must live in
 *        the type.
 * @endif
 */
struct _Flush : out_manip
{
    /**
     * @lang{ZH}
     * @brief 刷新本流。
     * @note 无需显式加锁，也无需 `try`：`out_flusher::flush()` 自己持有 `io_mutex()`，并已
     *       将异常交由 `handle_exception` 处理。
     * @param os 目标输出流。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes the stream.
     * @note Neither an explicit lock nor a `try` is needed: `out_flusher::flush()` holds
     *       `io_mutex()` itself and already routes exceptions through `handle_exception`.
     * @param os The target output stream.
     * @endif
     */
    template <ostream_type T>
    void operator () (T& os) const
    {
        os.flush();
    }
};

/**
 * @lang{ZH}
 * @brief 刷新流的操纵符对象。用法为 `os << IOv2::flush` 或 `IOv2::flush(os)`。
 * @endif
 *
 * @lang{EN}
 * @brief The manipulator object that flushes the stream. Use as `os << IOv2::flush` or
 *        `IOv2::flush(os)`.
 * @endif
 */
inline constexpr _Flush flush{};

// https://github.com/gcc-mirror/gcc/blob/075ec330307c5b1fe5ed166a633c718c06b01437/libstdc%2B%2B-v3/include/bits/ostream.h#L80
// TODO: add quoted
// https://github.com/gcc-mirror/gcc/blob/075ec330307c5b1fe5ed166a633c718c06b01437/libstdc%2B%2B-v3/include/std/iomanip#L88
}