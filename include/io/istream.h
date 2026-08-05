#pragma once
#include <common/copyable_mutex.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <io/ostream.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/stream_common_operators.h>

#include <mutex>
#include <type_traits>
#include <utility>

namespace IOv2
{
template <io_device TDevice, typename TChar>
class istream : public ios_base<TChar>
              , public io_state_and_exp
              , public istream_operators<TChar>
              , public stream_common_operators
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using in_sentry_type = in_sentry<istream<device_type, char_type>, false>;
    using in_iter_type = istreambuf_iterator<istreambuf<TDevice, TChar>>;

    friend in_sentry_type;
    friend istream_operators<TChar>;
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
    istream()
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
    istream(TDevice dev)
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
    istream(TDevice dev, const TCreator& creator)
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
    istream(TDevice dev, IOv2::locale<char_type> loc)
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
    istream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

    /**
     * @lang{ZH}
     * @brief 拷贝构造、移动构造与移动赋值。
     * @warning 与拷贝赋值一样，这三者都是**不同步**的生命周期操作，不持有 `io_mutex()`；
     *          移动还会把源流置于移后状态。并发契约详见 `operator=(const istream&)`。
     * @endif
     *
     * @lang{EN}
     * @brief Copy construction, move construction and move assignment.
     * @warning Like copy assignment, all three are **unsynchronized** lifecycle operations that
     *          do not hold `io_mutex()`; a move additionally leaves the source moved-from. See
     *          `operator=(const istream&)` for the concurrency contract.
     * @endif
     */
    istream(const istream&) = default;
    istream(istream&&) = default;
    istream& operator=(istream&&) = default;
    ~istream() = default;

    /**
     * @lang{ZH}
     * @brief 拷贝赋值；提供强异常保证：先整体拷进临时对象（可能抛出，此时目标尚未被触碰），
     *        再以全程 noexcept 的移动赋值提交。move-only 内核（如 `file_device`）上的拷贝必然
     *        抛出，故自赋值也要先挡掉。
     *
     * @warning 本操作**不做线程同步**：与本流的其它操作（`tell`/`seek`/格式化 I/O 等均持有
     *          `io_mutex()`）不同，赋值不获取任何锁——它整体替换 `m_streambuf`（内含转换器
     *          管线与一个 `std::deque` 读缓冲区）与 `m_locale`（内含两张哈希表），与
     *          `detach()`/`attach()` 同属生命周期操作。区别在于赋值涉及**两个**操作数，
     *          **两者都要独占**：调用期间源与目标上都不得有其它线程进行操作（读、写、
     *          attach/detach 或再次赋值），否则行为未定义。需要在并发环境下更换流的内容，
     *          请由调用方自行串行化。
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
     *          lock -- it replaces `m_streambuf` (which holds the converter pipeline and a
     *          `std::deque` read buffer) and `m_locale` (which holds two hash tables)
     *          wholesale, and is a lifecycle operation just like `detach()`/`attach()`. The
     *          difference is that assignment involves **two** operands and **both** must be
     *          exclusively owned: no other thread may operate on either the source or the
     *          destination (reading, writing, attach/detach, or another assignment) while it
     *          runs, or the behavior is undefined. Serialize in the caller when stream
     *          contents must be replaced concurrently.
     * @endif
     */
    istream& operator=(const istream& other)
    {
        static_assert(std::is_nothrow_move_assignable_v<istream<TDevice, TChar>>,
                      "copy assignment's strong guarantee requires a noexcept move assignment");
        if (this != &other)
        {
            istream tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

private:
    istreambuf<TDevice, TChar> m_streambuf;
    IOv2::locale<char_type> m_locale;
};

template <io_device TDevice>
istream(TDevice) -> istream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
istream(TDevice, const TCreator&)
    -> istream<TDevice,
               typename decltype(istreambuf{std::declval<TDevice>(),
                                            std::declval<const TCreator&>()})::char_type>;

template <io_device TDevice, typename TChar>
    requires (std::is_same_v<typename TDevice::char_type, TChar>)
istream(TDevice, locale<TChar>) -> istream<TDevice, TChar>;

template <io_device TDevice, cvt_creator TCreator, typename TChar>
    requires (std::is_same_v<
                  typename decltype(istreambuf{std::declval<TDevice>(),
                                               std::declval<const TCreator&>()})::char_type,
                  TChar>)
istream(TDevice, const TCreator&, locale<TChar>) -> istream<TDevice, TChar>;

// common manips
/**
 * @lang{ZH}
 * @brief 跳过空白的操纵符对象，用法为 `is >> IOv2::ws`；类型是个空标签，逻辑全在
 *        `io_traits<TChar, _Ws>::sread` 里。
 *
 * 方向被编码进类型本身：`io_traits<TChar, _Ws>` 只提供 `sread`，`os << ws` 因此不满足插入运算符
 * 的约束、没有可行重载，编译不过。方向为何要这样表达，见 `io_traits<TChar, _Ws>`。
 * @note 本类型没有 `operator()`：单向操纵符的逻辑只需写一处，直接写在扩展点里即可，`is >> ws`
 *       于是成为唯一入口，异常统一由提取运算符转交 `handle_exception`。标准的 `std::ws(is)`
 *       直接调用形式因此不存在。
 * @endif
 *
 * @lang{EN}
 * @brief The whitespace-skipping manipulator object, used as `is >> IOv2::ws`; its type is an
 *        empty tag, with all the logic in `io_traits<TChar, _Ws>::sread`.
 *
 * The direction is encoded in the type itself: `io_traits<TChar, _Ws>` provides only `sread`, so
 * `os << ws` leaves the insertion operator unsatisfied -- no viable overload, and it does not
 * compile. See `io_traits<TChar, _Ws>` for why the direction is expressed this way.
 * @note This type has no `operator()`: a one-way manipulator needs its logic in one place only,
 *       so it lives in the extension point directly. That makes `is >> ws` the sole entry and
 *       leaves exceptions to the extraction operator and `handle_exception`. The standard's
 *       `std::ws(is)` direct-call form therefore does not exist.
 * @endif
 */
inline constexpr struct _Ws {} ws{};

/**
 * @lang{ZH}
 * @brief `ws` 的扩展点特化：只提供 `sread`，跳过流中接下来的空白字符。
 *
 * 空白的跳过由输入哨兵完成：以 `noskipws == false` 构造 `in_sentry` 即为跳过空白。
 *
 * 操纵符的方向由**成员的有无**表达：这里只有 `sread`，于是 `os << ws` 不满足插入运算符的约束、
 * 没有可行重载。改用约束是不够的——`iostream` 同时满足两个流概念，若两侧都提供成员，
 * 两个方向都会调用成功。
 * @note 锁由本成员自己取——流形式不加锁，而这里需要把哨兵罩在锁内。`catch` 也必须在同一把锁内：
 *       `operator>>` 那层 `catch` 在锁之外，只靠它的话失败路径上的置位会发生在解锁之后。掩码命中
 *       时异常仍会逃到运算符那层再处理一次，`handle_exception` 是幂等的。
 * @endif
 *
 * @lang{EN}
 * @brief Extension-point specialization for `ws`: provides only `sread`, which skips the
 *        whitespace characters that follow in the stream.
 *
 * The skipping is done by the input sentry: constructing `in_sentry` with `noskipws == false` is
 * what skips whitespace.
 *
 * A manipulator's direction is expressed by **which member exists**: only `sread` is here, so
 * `os << ws` leaves the insertion operator unsatisfied and there is no viable overload. A
 * constraint would not be enough -- `iostream` satisfies both stream concepts, so if both members
 * existed both directions would call successfully.
 * @note The lock is taken here: the stream form is never locked by the operator, and the sentry
 *       has to run under one. The `catch` has to be under that same lock: `operator>>`'s own
 *       `catch` sits outside it, so relying on that one alone would set the state bits after the
 *       unlock. On a masked rethrow the exception still reaches the operator and is handled a
 *       second time; `handle_exception` is idempotent.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, _Ws>
{
    template <istream_type T>
    static void sread(T& is, const _Ws&)
    {
        std::lock_guard guard(is.io_mutex());
        try
        {
            typename T::in_sentry_type cerb(is, false);
        }
        catch(...)
        {
            is.handle_exception(std::current_exception());
        }
    }
};
}