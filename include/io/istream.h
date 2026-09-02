// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <common/copyable_mutex.h>
#include <cvt/cvt_concepts.h>
#include <device/device_concepts.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>
#include <io/traits/traits_base.h>
#include <io/utilities/istream_operators.h>
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
 * @brief 字符输入流：把一条 `istreambuf` 与一个 locale 组合成带格式化的输入接口。
 *
 * 对外接口大多来自基类——`ios_state` 提供状态位与异常掩码，`istream_operators` 提供输入操作，
 * `stream_common_operators` 提供 `tell()`/`attach()`/`detach()`/`locale()` 等。本类自身只持有两个
 * 成员，且按**分层顺序**声明：`m_streambuf` 在前、`m_locale` 在后。
 *
 * IOv2 的输入路径自下而上是设备 → 转换器管线 → 流缓冲区 → 流；locale 位于最上层，只参与格式化与
 * 解析，不参与字符搬运。声明顺序与这条分层一致，于是构造自下而上、析构自上而下：`m_locale` 先
 * 销毁、`m_streambuf` 后销毁，后者一路向下触发的收尾因此始终作用在一个仍然完整的下层上。基类
 * `ios_state` 比两个成员更早构造、更晚析构，状态位在成员的整个生命期内都可用。
 *
 * @note 这条顺序同时规定了依赖方向：**下层不得访问上层**。流缓冲区及其以下（转换器、设备）不得
 *       引用 locale——它们的析构在 `m_locale` 之后运行，那时 locale 已经不存在。字符处理归流缓冲
 *       区，格式化与解析归 locale，两者不重叠；反向依赖（流读取 locale）则始终成立。
 *
 * @tparam TDevice 底层设备类型，须满足 `io_device` 且支持读取。
 * @tparam TChar 字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Character input stream: combines an `istreambuf` and a locale into a formatted input
 *        interface.
 *
 * Most of the interface comes from the bases -- `ios_state` supplies the state bits and the
 * exception mask, `istream_operators` the input operations, and `stream_common_operators`
 * `tell()`/`attach()`/`detach()`/`locale()` and friends. This class itself holds only two members,
 * declared in **layer order**: `m_streambuf` first, `m_locale` second.
 *
 * The IOv2 input path runs bottom-up as device -> converter pipeline -> stream buffer -> stream;
 * the locale sits at the top and takes part only in formatting and parsing, never in moving
 * characters. The declaration order follows that layering, so construction runs bottom-up and
 * destruction top-down: `m_locale` is destroyed first and `m_streambuf` second, so the teardown
 * the latter triggers down the stack always runs against a lower stack that is still intact. The
 * `ios_state` base is constructed before both members and destroyed after them, so the state bits
 * stay available for the members' whole lifetime.
 *
 * @note The same order fixes the direction of dependency: **a lower layer must not reach up**. The
 *       stream buffer and everything below it (converters, device) must not refer to the locale --
 *       they are destroyed after `m_locale`, by which point it no longer exists. Character
 *       handling belongs to the stream buffer, formatting and parsing to the locale; the two do
 *       not overlap. The reverse dependency -- the stream reading the locale -- always holds.
 *
 * @tparam TDevice The underlying device type; must satisfy `io_device` and support reading.
 * @tparam TChar The character type.
 * @endif
 */
template <io_device TDevice, typename TChar>
    requires dev_cpt::support_get<TDevice>
class istream : public ios_state<TChar>
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
    istream()
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
    istream(TDevice dev)
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
    istream(TDevice dev, const TCreator& creator)
        requires (std::is_same_v<
                      typename decltype(istreambuf{std::declval<TDevice>(),
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
    istream(TDevice dev, IOv2::locale<char_type> loc)
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
    istream(TDevice dev, const TCreator& creator, IOv2::locale<char_type> loc)
        requires (std::is_same_v<
                      typename decltype(istreambuf{std::declval<TDevice>(),
                                              std::declval<const TCreator&>()})::char_type,
                      TChar>)
        : m_streambuf(std::move(dev), creator)
        , m_locale(std::move(loc)) {}

private:
    istream(const std::lock_guard<copyable_mutex<std::recursive_mutex>>&,
            const istream& other)
        : ios_state<TChar>(other)
        , istream_operators<TChar>(other)
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
     *          `operator=(const istream&)`。
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
     *          be using. See `operator=(const istream&)` for the concurrency contract.
     * @note The `noexcept` on move assignment is deliberate: copy assignment's strong guarantee
     *       depends on it (see the `static_assert` there). Taking the lock can formally throw,
     *       but for an already-constructed recursive mutex the only remaining cause is an
     *       exhausted recursion count, which this library treats as unrecoverable -- that is,
     *       it terminates.
     * @endif
     */
    istream(const istream& other) : istream(std::lock_guard{other.io_mutex()}, other) {}
    istream(istream&&) noexcept = default;

    istream& operator=(istream&& other) noexcept
    {
        if (this == &other) return *this;

        std::lock_guard guard(this->io_mutex());
        // NOLINTBEGIN(bugprone-use-after-move): each `std::move(other)` below binds to a
        // *different* base subobject, and every one of those base assignments touches only its
        // own members; `m_streambuf` and `m_locale` belong to no base at all. Nothing is read
        // after being moved from -- clang-tidy just cannot see that the operands are disjoint.
        ios_state<TChar>::operator=(std::move(other));
        istream_operators<TChar>::operator=(std::move(other));
        stream_common_operators::operator=(std::move(other));
        m_streambuf = std::move(other.m_streambuf);
        m_locale    = std::move(other.m_locale);
        // NOLINTEND(bugprone-use-after-move)
        return *this;
    }

    ~istream() = default;

    /**
     * @lang{ZH}
     * @brief 拷贝赋值；提供强异常保证：先整体拷进临时对象（可能抛出，此时目标尚未被触碰），
     *        再以全程 noexcept 的移动赋值提交。move-only 内核（如 `file_device`）上的拷贝必然
     *        抛出，故自赋值也要先挡掉。
     *
     * @warning 赋值整体替换 `m_streambuf`（内含转换器管线与一个读缓冲区）与 `m_locale`（内含两
     *          张哈希表），与 `detach()`/`attach()` 同属生命周期操作，区别在于它涉及**两个**操
     *          作数。两个操作数都受 `io_mutex()` 保护：拷贝构造持有源流的锁读出副本，随后的移动
     *          赋值持有目标流的锁完成替换。两把锁**先后获取、互不重叠**，因此任一线程在任一时刻
     *          至多持有一把流锁。仍由调用方负责的是：不得在其它线程仍可能使用某流时销毁它、或把
     *          它作为移动的源。另需注意，赋值由此参与调用方自身的锁定序——`sync(P); X = Q;` 与
     *          `sync(Q); Y = P;` 反向配对仍会死锁，这与 `sync` 一贯的表述一致。
     * @endif
     *
     * @lang{EN}
     * @brief Copy assignment with the strong exception guarantee: the copy is made into a
     *        temporary first (which may throw, with the destination still untouched), and the
     *        commit is a move assignment, noexcept throughout. A copy always throws on a
     *        move-only kernel (`file_device`), which is why self-assignment is short-circuited.
     *
     * @warning Assignment replaces `m_streambuf` (which holds the converter pipeline and a read
     *          buffer) and `m_locale` (which holds two hash tables) wholesale, and is a
     *          lifecycle operation just like `detach()`/`attach()`, except that it involves
     *          **two** operands. Both are covered by `io_mutex()`: the copy construction holds
     *          the source's lock while reading it out, and the move assignment that follows
     *          holds the destination's lock while replacing it. The two locks are taken **in
     *          sequence and never overlap**, so a thread holds at most one stream lock at any
     *          instant. What remains the caller's responsibility: never destroy, or move from, a
     *          stream another thread may still be using. Note also that assignment thereby joins
     *          the caller's own lock order -- `sync(P); X = Q;` paired with `sync(Q); Y = P;`
     *          still deadlocks, exactly as documented for `sync`.
     * @endif
     */
    istream& operator=(const istream& other)
    {
        static_assert(std::is_nothrow_move_assignable_v<istream<TDevice, TChar>>,
                      "the noexcept on move assignment must stay: it is what keeps the commit "
                      "step below from leaving *this half-updated");
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

// Only these two need a guide: TChar appears nowhere in their parameters, so the implicit guide
// cannot deduce it. The two overloads that take a locale deduce TChar from it through their own
// implicit guide, which inherits the constraint written on the constructor -- repeating that
// constraint here would just duplicate it, and the constructor's copy is the one that also covers
// explicitly-written template arguments.
template <io_device TDevice>
istream(TDevice) -> istream<TDevice, typename TDevice::char_type>;

template <io_device TDevice, cvt_creator TCreator>
istream(TDevice, const TCreator&)
    -> istream<TDevice,
               typename decltype(istreambuf{std::declval<TDevice>(),
                                            std::declval<const TCreator&>()})::char_type>;

// common manips
/**
 * @lang{ZH}
 * @brief 跳过空白的操纵符对象，用法为 `is >> IOv2::ws`；类型是个空标签，逻辑全在
 *        `io_traits<TChar, ws_t>::sread` 里。
 *
 * 方向被编码进类型本身：`io_traits<TChar, ws_t>` 只提供 `sread`，`os << ws` 因此不满足插入运算符
 * 的约束、没有可行重载，编译不过。方向为何要这样表达，见 `io_traits<TChar, ws_t>`。
 * @note 本类型没有 `operator()`：单向操纵符的逻辑只需写一处，直接写在扩展点里即可，`is >> ws`
 *       于是成为唯一入口，异常统一由提取运算符转交 `handle_exception`。标准的 `std::ws(is)`
 *       直接调用形式因此不存在。
 * @endif
 *
 * @lang{EN}
 * @brief The whitespace-skipping manipulator object, used as `is >> IOv2::ws`; its type is an
 *        empty tag, with all the logic in `io_traits<TChar, ws_t>::sread`.
 *
 * The direction is encoded in the type itself: `io_traits<TChar, ws_t>` provides only `sread`, so
 * `os << ws` leaves the insertion operator unsatisfied -- no viable overload, and it does not
 * compile. See `io_traits<TChar, ws_t>` for why the direction is expressed this way.
 * @note This type has no `operator()`: a one-way manipulator needs its logic in one place only,
 *       so it lives in the extension point directly. That makes `is >> ws` the sole entry and
 *       leaves exceptions to the extraction operator and `handle_exception`. The standard's
 *       `std::ws(is)` direct-call form therefore does not exist.
 * @endif
 */
inline constexpr struct ws_t {} ws{};

/**
 * @lang{ZH}
 * @brief `ws` 的扩展点特化：只提供 `sread`，跳过流中接下来的空白字符。
 *
 * 空白的跳过由输入哨兵完成：`in_sentry` 的形参名为 `noskip`，以 `noskip == false` 构造即为
 * 跳过空白。
 * @note 这与流上的 `skipws` 标志（及清除它的 `noskipws` 操纵符）**没有关系**：`in_sentry`
 *       不读该标志，只看构造实参；读标志的是提取运算符，由它把 `!skip` 传给哨兵。而 `ws`
 *       这条路径传的是字面量 `false`，因此 **`is >> noskipws >> ws` 照样跳空白**——这正是
 *       `ws` 应有的语义。
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
 * The skipping is done by the input sentry: its parameter is named `noskip`, and constructing
 * `in_sentry` with `noskip == false` is what skips whitespace.
 * @note This has **nothing to do with** the stream's `skipws` flag (or the `noskipws`
 *       manipulator that clears it): `in_sentry` never reads that flag, only its constructor
 *       argument; it is the extraction operator that reads the flag and passes `!skip` to the
 *       sentry. This path passes a literal `false`, so **`is >> noskipws >> ws` still skips
 *       whitespace** — which is exactly what `ws` is supposed to do.
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
struct io_traits<TChar, ws_t>
{
    template <istream_type T>
        requires (std::is_same_v<typename T::char_type, TChar>)
    static void sread(T& is, const ws_t&)
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
