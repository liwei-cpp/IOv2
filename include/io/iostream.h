#pragma once
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <io/io_base.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>
#include <io/utilities/istream_operators.h>
#include <io/utilities/ostream_operators.h>
#include <io/utilities/stream_common_operators.h>
#include <locale/locale.h>

#include <exception>
#include <mutex>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 双向字符流：把一条 `streambuf` 与一个 locale 组合成带格式化的输入输出接口。
 *
 * 对外接口大多来自基类——`ios_state` 提供状态位与异常掩码，`istream_operators` 与
 * `ostream_operators` 分别提供输入与输出操作，`out_flusher` 携带 tie 刷新用的多态 `try_flush()`，
 * `stream_common_operators` 提供 `tell()`/`attach()`/`detach()`/`locale()` 等。本类自身只持有两个
 * 成员，且按**分层顺序**声明：`m_streambuf` 在前、`m_locale` 在后。
 *
 * IOv2 的读写路径是设备 ↔ 转换器管线 ↔ 流缓冲区 ↔ 流；locale 位于最上层，只参与格式化与解析，
 * 不参与字符搬运。声明顺序与这条分层一致，于是构造自下而上、析构自上而下：`m_locale` 先销毁、
 * `m_streambuf` 后销毁，`~root_cvt` 把残留缓冲冲刷进设备这一步因此始终作用在一个仍然完整的下层
 * 上。基类 `ios_state` 比两个成员更早构造、更晚析构，状态位在成员的整个生命期内都可用。
 *
 * @note 这条顺序同时规定了依赖方向：**下层不得访问上层**。流缓冲区及其以下（转换器、设备）不得
 *       引用 locale——析构期的那次冲刷跑在 `m_locale` 之后，那时 locale 已经不存在。字符处理归流
 *       缓冲区，格式化与解析归 locale，两者不重叠；反向依赖（流读取 locale）则始终成立。
 *
 * @tparam TDevice 底层设备类型，须满足 `io_device` 且同时支持读取与写入。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Bidirectional character stream: combines a `streambuf` and a locale into a formatted
 *        input/output interface.
 *
 * Most of the interface comes from the bases -- `ios_state` supplies the state bits and the
 * exception mask, `istream_operators` and `ostream_operators` the input and output operations,
 * `out_flusher` the polymorphic `try_flush()` used by tie, and `stream_common_operators`
 * `tell()`/`attach()`/`detach()`/`locale()` and friends. This class itself holds only two members,
 * declared in **layer order**: `m_streambuf` first, `m_locale` second.
 *
 * The IOv2 read/write path is device <-> converter pipeline <-> stream buffer <-> stream; the
 * locale sits at the top and takes part only in formatting and parsing, never in moving
 * characters. The declaration order follows that layering, so construction runs bottom-up and
 * destruction top-down: `m_locale` is destroyed first and `m_streambuf` second, so the step where
 * `~root_cvt` flushes what is left in the buffer to the device always runs against a lower stack
 * that is still intact. The `ios_state` base is constructed before both members and destroyed
 * after them, so the state bits stay available for the members' whole lifetime.
 *
 * @note The same order fixes the direction of dependency: **a lower layer must not reach up**. The
 *       stream buffer and everything below it (converters, device) must not refer to the locale --
 *       that destructor-time flush runs after `m_locale`, by which point it no longer exists.
 *       Character handling belongs to the stream buffer, formatting and parsing to the locale; the
 *       two do not overlap. The reverse dependency -- the stream reading the locale -- always holds.
 *
 * @tparam TDevice The underlying device type; must satisfy `io_device` and support both reading
 *         and writing.
 * @tparam TChar The character type.
 * @endif
 */
template <io_device TDevice, typename TChar>
    requires (dev_cpt::support_get<TDevice> && dev_cpt::support_put<TDevice>)
class iostream : public ios_state<TChar>
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
     * @note 本重载要求 `TDevice::char_type` 与 `TChar` 一致：不带转换器时转换管线不改变字符
     *       类型。约束为何落在构造函数上而不是推导指引上，见带 locale 的重载。
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
     * @note This overload requires `TDevice::char_type` to match `TChar`: with no converter the
     *       pipeline does not change the character type. See the locale-taking overload for why
     *       the constraint sits on the constructor rather than on a deduction guide.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs.
     * @endif
     */
    iostream()
        requires (std::is_same_v<typename TDevice::char_type, TChar>
                  && std::is_default_constructible_v<TDevice>)
        : m_streambuf(TDevice()) {}

    /**
     * @lang{ZH}
     * @brief 以 @p dev 建立流。
     * @param dev 底层设备。
     * @throw device_error 若设备无法完成转换器初始化所需的查询；理由见默认构造函数。
     * @note 字符类型的约束与默认构造函数相同。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over @p dev.
     * @param dev The underlying device.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs; see the default constructor for why this is not reported as state.
     * @note The character-type constraint is the default constructor's.
     * @endif
     */
    iostream(TDevice dev)
        requires (std::is_same_v<typename TDevice::char_type, TChar>)
        : m_streambuf(std::move(dev)) {}

    /**
     * @lang{ZH}
     * @brief 以 @p dev 建立流，并由 @p creator 构造转换器。
     * @tparam TCreator 转换器创建器类型。
     * @param dev     底层设备。
     * @param creator 转换器创建器。
     * @throw device_error 若设备无法完成转换器初始化所需的查询；理由见默认构造函数。
     * @note 本重载要求**转换管线产出的** `char_type` 与 `TChar` 一致；写法与理由见同时带
     *       creator 与 locale 的那个重载。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a stream over @p dev with the converter built by @p creator.
     * @tparam TCreator The converter-creator type.
     * @param dev     The underlying device.
     * @param creator The converter creator.
     * @throw device_error If the device cannot answer the queries the converter initialization
     *        needs; see the default constructor for why this is not reported as state.
     * @note This overload requires the `char_type` the **converter pipeline produces** to match
     *       `TChar`; see the creator-plus-locale overload for the same constraint and why.
     * @endif
     */
    template <cvt_creator TCreator>
    iostream(TDevice dev, const TCreator& creator)
        requires (std::is_same_v<
                      typename decltype(streambuf{std::declval<TDevice>(),
                                              std::declval<const TCreator&>()})::char_type,
                      TChar>)
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
    iostream(TDevice dev, IOv2::locale<char_type> loc)
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
    iostream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        requires (std::is_same_v<
                      typename decltype(streambuf{std::declval<TDevice>(),
                                              std::declval<const TCreator&>()})::char_type,
                      TChar>)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

private:
    iostream(const std::lock_guard<copyable_mutex<std::recursive_mutex>>&,
             const iostream& other)
        : ios_state<TChar>(other)
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
    iostream(iostream&&) noexcept = default;

    iostream& operator=(iostream&& other) noexcept
    {
        if (this == &other) return *this;

        std::lock_guard guard(this->io_mutex());
        ios_state<TChar>::operator=(std::move(other));
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
     * @warning 赋值整体替换 `m_streambuf`（内含转换器管线——方向状态即在其中——与一个读缓冲区）
     *          与 `m_locale`（内含两张哈希表），与 `detach()`/`attach()` 同属生命周期操作，区
     *          别在于它涉及**两个**操作数。两个操作数都受 `io_mutex()` 保护：拷贝构造持有源流
     *          的锁读出副本，随后的移动赋值持有目标流的锁完成替换。两把锁**先后获取、互不重
     *          叠**，因此任一线程在任一时刻至多持有一把流锁。仍由调用方负责的是：不得在其它线
     *          程仍可能使用某流时销毁它、或把它作为移动的源。另需注意，赋值由此参与调用方自身
     *          的锁定序——`sync(P); X = Q;` 与 `sync(Q); Y = P;` 反向配对仍会死锁，这与 `sync`
     *          一贯的表述一致。
     * @endif
     *
     * @lang{EN}
     * @brief Copy assignment with the strong exception guarantee: the copy is made into a
     *        temporary first (which may throw, with the destination still untouched), and the
     *        commit is a move assignment, noexcept throughout. A copy always throws on a
     *        move-only kernel (`file_device`), which is why self-assignment is short-circuited.
     *
     * @warning Assignment replaces `m_streambuf` (which holds the converter pipeline, the
     *          direction state included, and a read buffer) and `m_locale` (which holds two hash
     *          tables) wholesale, and is a lifecycle operation just like `detach()`/`attach()`,
     *          except that it involves **two** operands. Both are covered by `io_mutex()`: the
     *          copy construction holds the source's lock while reading it out, and the move
     *          assignment that follows holds the destination's lock while replacing it. The two
     *          locks are taken **in sequence and never overlap**, so a thread holds at most one
     *          stream lock at any instant. What remains the caller's responsibility: never
     *          destroy, or move from, a stream another thread may still be using. Note also that
     *          assignment thereby joins the caller's own lock order -- `sync(P); X = Q;` paired
     *          with `sync(Q); Y = P;` still deadlocks, exactly as documented for `sync`.
     * @endif
     */
    iostream& operator=(const iostream& other)
    {
        static_assert(std::is_nothrow_move_assignable_v<iostream<TDevice, TChar>>,
                      "the noexcept on move assignment must stay: it is what keeps the commit "
                      "step below from leaving *this half-updated");
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
     * 与本流的其它操作一样，本函数持有 `io_mutex()`。这不是可有可无的：切换方向会重定位转换
     * 器、清空读缓冲区并翻转转换器的方向标志，而 `in_sentry` / `out_sentry` 在**持有同一把
     * 锁**的前提下调用的正是同一批底层函数。若此处不加锁，同一份状态就存在一条加锁、一条不加
     * 锁的访问路径，两者并发即为数据竞争——不只是标志撕裂，而是那个缓冲区会被一边
     * `clear()`、一边 `sgetc()` 读取。
     * @note 流处于失败状态时直接返回、不触碰缓冲区，与 `tell()` 的做法一致。
     * @return 流自身的引用。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the underlying buffer to the put direction.
     *
     * Like every other operation on this stream, this holds `io_mutex()`. That is not optional:
     * switching direction repositions the converter, clears the read buffer and flips the
     * converter's direction flag -- and `in_sentry` / `out_sentry` call those very same
     * underlying functions **while holding that same lock**. Without it here, one piece of
     * state would have both a locked and an unlocked access path, and running them concurrently
     * is a data race -- not merely a torn flag, but that buffer being `clear()`ed on one side
     * while `sgetc()` reads it on the other.
     * @note Returns without touching the buffer when the stream is in a failed state, matching
     *       what `tell()` does.
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

// Only these two need a guide: TChar appears nowhere in their parameters, so the implicit guide
// cannot deduce it. The two overloads that take a locale deduce TChar from it through their own
// implicit guide, which inherits the constraint written on the constructor -- repeating that
// constraint here would just duplicate it, and the constructor's copy is the one that also covers
// explicitly-written template arguments.
template <io_device TDevice>
iostream(TDevice) -> iostream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
iostream(TDevice, const TCreator&)
    -> iostream<TDevice,
                typename decltype(streambuf{std::declval<TDevice>(),
                                            std::declval<const TCreator&>()})::char_type>;

}
