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
 * @brief `ws` 操纵符的类型。
 *
 * 方向被编码进类型本身：只有 `operator>>` 接受本类型，对应的 `operator<<` 重载已被删除。
 * 方向为何必须编码进类型、而非仅靠 `istream_type` / `ostream_type` 约束，见 `in_manip`。
 *
 * 保留 `operator()` 是为了与标准的 `std::ws(is)` 直接调用形式对齐。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the `ws` manipulator.
 *
 * The direction is encoded in the type itself: only `operator>>` accepts this type, and the
 * matching `operator<<` overload is deleted. See `in_manip` for why the direction must live in
 * the type rather than in the `istream_type` / `ostream_type` constraints alone.
 *
 * `operator()` is kept to match the standard's `std::ws(is)` direct-call form.
 * @endif
 */
struct _Ws : in_manip
{
    /**
     * @lang{ZH}
     * @brief 跳过流中接下来的空白字符。
     *
     * 空白的跳过由输入哨兵完成：以 `noskipws == false` 构造 `in_sentry` 即为跳过空白。
     * @note 那把锁以 `defer_lock` 构造后交给哨兵，因此 `catch` 中的 `handle_exception` 仍在
     *       持锁状态下运行——失败位的更新与本次操作处于同一个临界区内。消费本操纵符的
     *       `operator>>` 外面还有一层 `catch`，但它在锁之外，只是兜底。
     * @param is 目标输入流。
     * @endif
     *
     * @lang{EN}
     * @brief Skips the whitespace characters that follow in the stream.
     *
     * The skipping is done by the input sentry: constructing `in_sentry` with `noskipws == false`
     * is what skips whitespace.
     * @note The lock is constructed `defer_lock` and handed to the sentry, so `handle_exception`
     *       in the `catch` still runs while holding it -- the failbit update lands in the same
     *       critical section as the operation itself. The `operator>>` that consumes this
     *       manipulator has a `catch` of its own, but that one runs outside the lock and is only
     *       a backstop.
     * @param is The target input stream.
     * @endif
     */
    template <istream_type T>
    void operator () (T& is) const
    {
        using sentry_type = typename T::in_sentry_type;
        std::unique_lock lk(is.io_mutex(), std::defer_lock);
        try
        {
            sentry_type cerb(is, false, lk);
        }
        catch(...)
        {
            is.handle_exception(std::current_exception());
        }
    }
};

/**
 * @lang{ZH}
 * @brief 跳过空白的操纵符对象。用法为 `is >> IOv2::ws`；亦支持 `IOv2::ws(is)`。
 * @endif
 *
 * @lang{EN}
 * @brief The whitespace-skipping manipulator object. Use as `is >> IOv2::ws`; `IOv2::ws(is)`
 *        also works.
 * @endif
 */
inline constexpr _Ws ws{};
}