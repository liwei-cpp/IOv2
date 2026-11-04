// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file in_impl.h
 * @lang{ZH}
 * 定义标准输入流的实现模板 `stdin_api`，以及两个标准输入流对象 `cin`（`char`）与 `wcin`
 * （`wchar_t`）。
 *
 * `stdin_api` 不由 `istream` 派生，而是同样把一条 `ichannel`（其下依次是转换器管线与固定 fd
 * 的设备 `std_input_device`）与一个 `locale` 组合起来，对外接口来自 `ios_state`（状态位
 * 与异常掩码）、`istream_operators`（输入操作）与 `std_stream_common_operators`（`tell()` /
 * `locale()` 等）三个基类。与 `istream` 的差别都来自「设备是固定 fd 的进程级单例」：多了
 * `sync_with_stdio()` / `synced_with_stdio()`、`reset()` 与（宽流）`code()` / `switch_code()`，
 * `adjust()` 收到 `stdin_sync` 时改走 `sync_with_stdio()`，换设备的 `detach()` / `attach()`
 * 则被 `= delete`。
 *
 * 两个流对象经 `sing_temp` 成为进程级单例，退出钩子为空——与 `std::cin` 一样，退出时不析构，
 * 退出阶段既有引用仍然有效。构造时各自 `tie()` 到同字符类型的输出流（`cin` → `cout`，
 * `wcin` → `wcout`）。
 *
 * @note 「退出阶段仍可用」只覆盖本库自己的流对象、locale、facet 与 facet 触到的进程级数据；
 *       用户经 `locale(loc)` 装进来的自定义 facet 若持有会在退出时析构的全局量，不在此列。
 *       详见 `out_impl.h` 的同名说明。
 *
 * @note **`wcin` 不接受状态依赖的编码**（如 ISO-2022-JP）：环境变量指向这样的 locale 时，
 *       `wcin` 改用 `"C"`，不报错——此时 `ESC $ B` 之类的移位序列按 ASCII 字符读出，
 *       `code()` 报 `"C"`；`switch_code()` 切换到这样的编码会失败。与宽输出流一致，见
 *       `out_impl.h` 与 `cvt/code_cvt_stdio.h`。
 *
 * @note 一般不直接包含本头文件，而是包含 `IOv2/io/objects/objects.h`：入口那里还有一次切换全部
 *       八个标准流的 `sync_with_stdio()` 与 `ws` / `endl` 等操纵符，并说明了本系列头文件不带来
 *       哪些能力。
 * @endif
 *
 * @lang{EN}
 * Defines `stdin_api`, the implementation template behind the standard input streams, along
 * with the two standard input stream objects `cin` (`char`) and `wcin` (`wchar_t`).
 *
 * `stdin_api` does not derive from `istream`; it combines, in the same way, an `ichannel`
 * (below which sit the converter pipeline and the fixed-fd device `std_input_device`)
 * with a `locale`, and takes its interface from three bases -- `ios_state` (the state bits and
 * the exception mask), `istream_operators` (the input operations) and
 * `std_stream_common_operators` (`tell()` / `locale()` and friends). Every difference from
 * `istream` follows from the device being a fixed fd owned by a process-wide singleton: it
 * adds `sync_with_stdio()` / `synced_with_stdio()`, `reset()` and, on the wide stream, `code()` /
 * `switch_code()`; its `adjust()` hands a `stdin_sync` to `sync_with_stdio()`; and `detach()` /
 * `attach()`, which would replace the device, are `= delete`.
 *
 * Both stream objects are process-wide singletons through `sing_temp` with an empty exit
 * hook -- like `std::cin` they are not destroyed at exit, so existing references stay valid
 * once exit begins. Each ties itself at construction to the output stream of the same
 * character type (`cin` to `cout`, `wcin` to `wcout`).
 *
 * @note "Still usable while the process exits" covers this library's own stream objects,
 *       locales, facets and the process-wide data those facets reach; a user facet installed
 *       through `locale(loc)` that holds a global with a destructor is outside that. See the
 *       note of the same name in `out_impl.h`.
 *
 * @note **`wcin` takes no state-dependent encoding** (ISO-2022-JP, say): when the
 *       environment names such a locale, `wcin` uses `"C"` instead, without an error --
 *       shift sequences such as `ESC $ B` then read as ASCII characters, and `code()`
 *       reports `"C"`; `switch_code()` to such an encoding fails. The same as the wide output
 *       streams; see `out_impl.h` and `cvt/code_cvt_stdio.h`.
 *
 * @note Prefer including `IOv2/io/objects/objects.h` over this header: the entry point also
 *       brings the `sync_with_stdio()` that switches all eight standard streams at once and
 *       the `ws` / `endl` manipulators, and documents what this family of headers does not
 *       bring in.
 * @endif
 */
#pragma once
#include <IOv2/common/iov2_export.h>
#include <IOv2/common/sing_temp.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/stdin_root_cvt.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iochannel.h>
#include <IOv2/io/iochannel_iterator.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/utilities/istream_operators.h>
#include <IOv2/io/utilities/stream_common_operators.h>
#include <IOv2/locale/locale.h>

#include <clocale>
#include <cstdio>
#include <exception>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

namespace IOv2
{
namespace detail
{
/**
 * @lang{ZH}
 * @brief 标准输入流共用的对象锁：每个 `stdin_api`（`cin`、`wcin`）都以它作为 `io_mutex()`。
 *
 * 它们读的是同一个 fd 0，共用一把锁，一个流的操作会等另一个流正在进行的操作结束。
 * 永不析构，与 `cin`、`wcin` 本身一样（二者在退出时不析构，退出过程中仍可能被使用）。
 * 与八个标准流同理，「一份」的前提是单一模块，或所有模块都用 `IOV2_SHARED`；见
 * `IOv2/io/objects/objects.h` 的 `@note`。
 * @endif
 *
 * @lang{EN}
 * @brief The object lock the standard input streams share: every `stdin_api` (`cin`, `wcin`)
 * has it as its `io_mutex()`.
 *
 * They read the same fd 0, so they share a lock, and an operation on one waits for an
 * operation under way on another. It is never destroyed, like `cin` and `wcin` themselves
 * (they are not destroyed at exit and may still be used while the program exits). As with
 * the eight standard streams, "one" rests on a single module, or every module built with
 * `IOV2_SHARED`; see the `@note` in `IOv2/io/objects/objects.h`.
 * @endif
 */
#if defined(IOV2_SHARED)
IOV2_API copyable_mutex<std::recursive_mutex>& stdin_io_mutex();   // defined in iov2_objects.cpp
#else
inline copyable_mutex<std::recursive_mutex>& stdin_io_mutex()
{
    // Never deleted on purpose: see above.
    static auto* const m = new copyable_mutex<std::recursive_mutex>(); // NOLINT(cppcoreguidelines-owning-memory)
    return *m;
}
#endif
}

/**
 * @lang{ZH}
 * @brief 标准输入流的哨兵：在 `in_sentry` 之外，操作结束时把偷看、压回的字符退回下层。
 *
 * 析构时调用流的 `hand_back_peeked()`。每个读操作都构造哨兵，读取都在它的作用域内，所以
 * 操作结束时流的读缓冲里没有留下字符，C stdio 与另一个标准输入流接着读得到它们。构造失败时
 * 析构不执行，字符留在读缓冲里，本流下次照样先读到。
 * @tparam TStream 标准输入流类型。
 * @endif
 *
 * @lang{EN}
 * @brief The sentry of the standard input streams: on top of `in_sentry`, it hands the
 * characters peeked at or put back to the layer below when the operation ends.
 *
 * Its destructor calls the stream's `hand_back_peeked()`. Every read operation builds a sentry
 * and reads within its scope, so when an operation ends nothing is left in the stream's read
 * buffer, and C stdio and the other standard input stream read on from there. When
 * construction fails the destructor does not run, and the characters stay in the read buffer
 * for this stream to read first as usual.
 * @tparam TStream The standard input stream type.
 * @endif
 */
template <typename TStream>
class stdin_in_sentry
{
public:
    /// @lang{ZH} 见 `in_sentry` 的构造函数，调用方的要求也相同。 @endif @lang{EN} See the constructor of `in_sentry`, which places the same demands on the caller. @endif
    stdin_in_sentry(TStream& is, bool noskip)
        : m_is(is), m_sentry(is, noskip)
    {}

    ~stdin_in_sentry() { m_is.hand_back_peeked(); }

    stdin_in_sentry(const stdin_in_sentry&) = delete;
    stdin_in_sentry& operator=(const stdin_in_sentry&) = delete;
    stdin_in_sentry(stdin_in_sentry&&) = delete;
    stdin_in_sentry& operator=(stdin_in_sentry&&) = delete;

private:
    TStream& m_is;
    in_sentry<TStream, false> m_sentry;
};

/**
 * @lang{ZH}
 * @brief `is_in_sentry_impl` 的偏特化：`stdin_in_sentry` 也是输入哨兵。
 * @endif
 *
 * @lang{EN}
 * @brief Partial specialization of `is_in_sentry_impl`: `stdin_in_sentry` is an input sentry too.
 * @endif
 */
template <typename TStream>
struct is_in_sentry_impl<stdin_in_sentry<TStream>>
{
    constexpr static bool value = true;
};

/**
 * @lang{ZH}
 * @brief 标准输入流（`cin` / `wcin`）的实现模板。
 *
 * 把一条 `ichannel`（其下是转换器管线与根转换器 `stdin_root_cvt`，最底层是 fd 0 上的
 * `std_input_device`）与一个 `locale` 组合起来，对外接口来自 `ios_state`、
 * `istream_operators` 与 `std_stream_common_operators` 三个基类。与 `istream` 的差别见
 * 文件头：多了 `sync_with_stdio()` / `synced_with_stdio()`、`reset()` 与（宽流）`code()` /
 * `switch_code()`，`adjust()` 截下 `stdin_sync`，`detach()` / `attach()` 被删除。
 *
 * 不可复制；具体的流类型（`cin_t`、`wcin_t`）同时派生自 `sing_temp`，作为进程级单例使用。
 *
 * @note 所有标准输入流共用一把对象锁（`detail::stdin_io_mutex()`）：`cin` 与 `wcin` 读同一个
 *       fd 0，一个流的操作会等另一个流正在进行的操作结束，两个线程分别用它们也不会交错。锁是
 *       递归的，同一线程在一次操作里再用另一个流不会自锁。但由此多出一种死锁：线程 A 在 `cin`
 *       的一次提取里（如用户的 `io_traits::sread` 中）写 `wcout`，同时线程 B 在 `wcout` 的一次
 *       插入里读 `wcin`——A 持输入锁等 `wcout`，B 持 `wcout` 等输入锁。在一个流的操作里使用
 *       另一个标准流时请留意。
 *
 * @tparam TDevice 设备类型，必须是 `std_input_device`。
 * @tparam TChar 字符类型（`char` 或 `wchar_t`）。
 * @endif
 *
 * @lang{EN}
 * @brief The implementation template behind the standard input streams (`cin` / `wcin`).
 *
 * Combines an `ichannel` (below which sit the converter pipeline and the root converter
 * `stdin_root_cvt`, with `std_input_device` on fd 0 at the bottom) with a `locale`,
 * and takes its interface from three bases: `ios_state`, `istream_operators` and
 * `std_stream_common_operators`. The differences from `istream` are listed in the file
 * header: it adds `sync_with_stdio()` / `synced_with_stdio()`, `reset()` and, on the wide
 * stream, `code()` / `switch_code()`; its `adjust()` catches `stdin_sync`; and `detach()` /
 * `attach()` are deleted.
 *
 * Not copyable; the concrete stream types (`cin_t`, `wcin_t`) also derive from `sing_temp`
 * and are used as process-wide singletons.
 *
 * @note All standard input streams share one object lock (`detail::stdin_io_mutex()`): `cin`
 *       and `wcin` read the same fd 0, so an operation on one waits for an operation under way
 *       on the other, and two threads using them do not interleave. The lock is recursive, so
 *       one thread using the other stream inside an operation does not deadlock itself. It
 *       does add one deadlock: thread A, inside an extraction on `cin` (in a user
 *       `io_traits::sread`, say), writes `wcout`, while thread B, inside an insertion on
 *       `wcout`, reads `wcin` -- A holds the input lock waiting for `wcout`, B holds `wcout`
 *       waiting for the input lock. Take care when one standard stream is used inside an
 *       operation on another.
 *
 * @tparam TDevice The device type; must be `std_input_device`.
 * @tparam TChar The character type (`char` or `wchar_t`).
 * @endif
 */
template <io_device TDevice, typename TChar>
    requires std::is_same_v<TDevice, std_input_device>
class stdin_api : public ios_state<TChar>
                , public istream_operators<TChar>
                , public std_stream_common_operators
{
    friend istream_operators<TChar>;
    friend std_stream_common_operators;

public:
    using device_type = TDevice;
    using char_type = TChar;
    using in_sentry_type = stdin_in_sentry<stdin_api>;
    using in_iter_type = ichannel_iterator<ichannel<device_type, char_type>>;
    friend in_sentry_type;
    friend in_sentry<stdin_api, false>;

public:
    /**
     * @lang{ZH}
     * @brief 默认构造：在 fd 0 上建立一条不带额外转换层的输入通道，初始为同步模式。
     * @endif
     *
     * @lang{EN}
     * @brief Default constructor: builds an input channel on fd 0 with no extra converter
     * layer, starting in synchronized mode.
     * @endif
     */
    stdin_api()
        : ios_state<TChar>(&detail::stdin_io_mutex())
        , m_channel(stdin_root_cvt{})
    {}

    /**
     * @lang{ZH}
     * @brief 以转换器创建器构造：在 fd 0 的根转换器之上叠加 `creator` 创建的转换层，
     * 初始为同步模式。
     * @tparam TCreator 转换器创建器类型。
     * @param creator 用于在根转换器之上构建转换器管线的创建器（如 `code_cvt_stdio_creator`）。
     * @endif
     *
     * @lang{EN}
     * @brief Constructs with a converter creator: stacks the layers `creator` builds on top
     * of the root converter on fd 0, starting in synchronized mode.
     * @tparam TCreator The converter-creator type.
     * @param creator The creator that builds the converter pipeline on top of the root
     *        converter (such as `code_cvt_stdio_creator`).
     * @endif
     */
    template <cvt_creator TCreator>
    stdin_api(const TCreator& creator)
        : ios_state<TChar>(&detail::stdin_io_mutex())
        , m_channel(creator.create(stdin_root_cvt{}))
    {}

    ~stdin_api() = default;
    stdin_api(const stdin_api&) = delete;
    stdin_api& operator=(const stdin_api&) = delete;
    stdin_api(stdin_api&&) = delete;
    stdin_api& operator=(stdin_api&&) = delete;

public:
    /**
     * @lang{ZH}
     * @brief 切换本流是否与 C stdio 同步：同步时经 `stdin` 的 `FILE` 按需读、与 C stdio 共用
     * 缓冲，不同步时直接读 fd 0、自带读缓冲。
     *
     * 同步标志只有一份，所有标准输入流共用（在 `detail::stdin_shared_imp` 里，见
     * `stdin_root_cvt`）：切换一个流就切换了全部，`cin`、`wcin` 的 `synced_with_stdio()` 总是一致。
     * 切换只翻这个标志与设备的读法（`std_device::stdio_mode()`），不重建 iochannel：缓冲、解码器、
     * 设备都留在原处。切到同步时，非同步模式下已预读进缓冲的
     * 字节照常先交出，交完才按需读，**不丢输入**；切到不同步时，下一次缓冲空了就整块读 fd，
     * 此前 C stdio 预读进 `FILE` 缓冲、尚未读走的字节本流不再看得到（在已有 I/O 之后切换，
     * 标准同样只给出实现定义的效果）。一次提取进行中（用户 `io_traits::sread` 里）切换只影响
     * 之后向设备要字节的方式。与当前模式相同的调用什么也不做，也不取锁，不会等另一线程里
     * 阻塞着的读；模式不同时要取本流的锁，会等另一线程里正在进行的读结束，若它阻塞着就一直
     * 等到有输入。
     *
     * @warning 同步时**不往前多读，每次只向 `FILE` 要本次操作还缺的字节**（格式化提取与
     *          `get` / `getline` 因逐字符探分隔符而逐字节；`getline` 存满缓冲时还要多看一个字符，
     *          才能判断它是分隔符还是溢出；`read(buf, n)` 按需分次读：`cin` 每次至多 2048 字节，
     *          `wcin` 按还缺的字符数要字节），C stdio 预读进 `FILE` 的字节本流接着读得到，
     *          反之亦然。每次操作结束时本流把偷看、压回的字符退回下层，格式化提取探到的分隔符
     *          `getchar()` / `fgets()` 接着读得到，另一个标准输入流也读得到；`wcin` 的字符重新
     *          编码成字节退回。退回是尽力而为的，失败不报告：`FILE` 不收时（不同步，Windows，
     *          或字节缓冲里还有字节）字节进共用的字节缓冲，C stdio 看不到而另一个流读得到；
     *          内存不足，或 `wcin` 的字符无法编码时，字符留在本流的读缓冲里。这些情形都
     *          **不丢字节**，只是跨边界不可见。到达 EOF 后 `FILE` 的
     *          EOF 标志是粘性的：`clear()` 之后仍读不到后来的输入，需 `reset()` 或
     *          `clearerr(stdin)`。
     *
     * 查询当前状态请用 `synced_with_stdio()`：本函数的无参形式等于 `sync_with_stdio(true)`，
     * 会真的切换。
     *
     * 失败按本库统一的方式报告：置状态位，`exceptions()` 掩码含该位时才抛出。翻标志本身不会
     * 失败；（`wchar_t`）只有转换器已 tainted 时先做的自动恢复可能失败，此时标志未动、
     * 本次没有切换，失败只记在本流上（别的标准输入流什么也没发生，也就没有可报告的）。这依赖
     * 根之上的每一层都先做完自己的事再往下传：根是最后一站，它翻转之后不再有可失败的步骤。
     * 自定义的层若先把行为传给下层、之后才抛出，切换已经生效，本流却报告失败。
     *
     * @param sync `true` 为同步（默认），`false` 为自带缓冲。
     * @return 调用前的同步状态；失败时同步状态未改变，返回的就是当前状态。
     * @endif
     *
     * @lang{EN}
     * @brief Switches whether this stream is synchronized with C stdio: synchronized reads
     * on demand through `stdin`'s `FILE`, sharing its buffer with C stdio; unsynchronized
     * reads fd 0 directly through its own buffer.
     *
     * There is one synchronization flag, shared by every standard input stream (it lives in
     * `detail::stdin_shared_imp`; see `stdin_root_cvt`): switching one stream switches them all,
     * and `cin` and `wcin` always agree on `synced_with_stdio()`. Switching flips that flag and
     * the device's read mode (`std_device::stdio_mode()`) alone, and does not rebuild the
     * iochannel: buffers, decoder and device all stay. Switching to synchronized hands out first, as usual, the bytes an
     * unsynchronized stream had read ahead into its buffer, and only then reads on demand, so
     * **no input is lost**; switching to unsynchronized reads a whole buffer off the fd the
     * next time it runs empty, and bytes C stdio had read ahead into the `FILE` buffer and
     * nobody has read yet are then out of this stream's sight (after I/O has taken place, the
     * standard too leaves such a switch implementation-defined). Switching in the middle of an
     * extraction (inside a user `io_traits::sread`) only changes how the device is asked for
     * bytes from then on. A call asking for the current mode does nothing and takes no lock,
     * so it does not wait for a read blocked in another thread; one asking for the other mode
     * takes this stream's lock and waits for a read under way in another thread, which, if
     * blocked, ends only when input arrives.
     *
     * @warning Synchronized, **nothing is read ahead: each read asks the `FILE` for just what
     *          the current operation is still short of** (formatted extraction and `get` /
     *          `getline` go byte by byte because they probe for the delimiter one character at
     *          a time; with its buffer full, `getline` still looks at one more character to
     *          tell a delimiter from an overflow; `read(buf, n)` reads in as many steps as it
     *          takes: `cin` at most 2048 bytes at a time, `wcin` asking for as many bytes as it
     *          is short of characters), and bytes C stdio has read ahead into the `FILE` are
     *          there for this stream to read on, and the other way round. When an operation
     *          ends, this stream hands the characters it peeked at or had put back to the layer
     *          below, so `getchar()` / `fgets()` read on from the delimiter a formatted
     *          extraction peeked at, and so does the other standard input stream; `wcin`'s
     *          characters go back encoded into bytes again. Handing back is best effort and a
     *          failure is not reported: where the `FILE` does not take them (unsynchronized, on
     *          Windows, or with bytes still in the byte buffer) the bytes go into the shared
     *          byte buffer, out of C stdio's sight but there for the other stream; out of
     *          memory, or with a `wcin` character that cannot be encoded, the characters stay in
     *          this stream's read buffer. **No byte is lost** in any of these; it is only
     *          invisible across the boundary.
     *          Once EOF is reached, the `FILE`'s EOF flag is sticky: after `clear()` later input
     *          is still not read, which takes `reset()` or `clearerr(stdin)`.
     *
     * To ask for the current state use `synced_with_stdio()`: with no argument this one means
     * `sync_with_stdio(true)` and does switch.
     *
     * A failure is reported the way this library reports every other one: a state bit is
     * set, and it throws only when the `exceptions()` mask includes that bit. Flipping the flag
     * cannot fail; (`wchar_t`) only the automatic recovery of a tainted converter, done first,
     * can, and then the flag is untouched and nothing is switched, and the failure is recorded
     * on this stream alone (nothing happened to the other standard input streams, so there is
     * nothing to report on them). This relies on every layer above the root finishing its own
     * work before passing the behavior down: the root is the last stop, and once it has
     * flipped nothing that can fail follows. A custom layer that passes it down first and
     * throws afterwards leaves the switch in effect while this stream reports a failure.
     *
     * @param sync `true` for synchronized (the default), `false` for own buffering.
     * @return The synchronization state before the call; on failure the state is unchanged,
     *         so that is also the current one.
     * @endif
     */
    bool sync_with_stdio(bool sync = true)
    {
        // Nothing to switch: return before the lock, which a blocked read may hold for long.
        if (detail::stdin_shared_imp.synced() == sync)
            return sync;

        std::lock_guard guard(this->io_mutex());
        const bool old_sync_state = detail::stdin_shared_imp.synced();
        if (old_sync_state == sync)
            return old_sync_state;

        // Only the shared flag flips, which the root does at the end of the adjust, for every
        // standard input stream: buffers, decoder and device all stay. A tainted wide
        // converter recovers first, and that can fail -- before the flag is touched.
        // m_channel's adjust, not this stream's: that one comes back here.
        try {
            m_channel.adjust(stdin_sync{sync});
        } catch (...) {
            this->handle_exception(std::current_exception());
        }
        return old_sync_state;
    }

    /**
     * @lang{ZH}
     * @brief 查询本流当前是否与 C stdio 同步。
     *
     * 纯读，不切换任何东西——`sync_with_stdio()` 不是 getter，它的无参形式等于
     * `sync_with_stdio(true)`。
     *
     * @return 当前的同步状态。
     * @endif
     *
     * @lang{EN}
     * @brief Asks whether this stream is currently synchronized with C stdio.
     *
     * A pure read that switches nothing -- `sync_with_stdio()` is not a getter; with no
     * argument it means `sync_with_stdio(true)`.
     *
     * @return The current synchronization state.
     * @endif
     */
    [[nodiscard]] bool synced_with_stdio() const noexcept
    {
        return detail::stdin_shared_imp.synced();
    }

    /**
     * @lang{ZH}
     * @brief 调整底层编码转换的行为；`stdin_sync{s}` 等同于 `sync_with_stdio(s)`。
     *
     * 同步状态在根转换器的标志与本流的 `synced_with_stdio()` 两处，`stdin_sync` 若直接送到根上
     * 只改前者，二者失配，之后 `sync_with_stdio()` 会因查询值未变而早退。所以本流自己提供
     * `adjust`：`std_stream_common_operators` 的版本是受保护的，连限定名也调不到；其余行为
     * 照旧交给它。`stdin_sync` 是 `final` 的，不会有同时携带别的行为的组合类型被这里截走。
     *
     * @param acc 要应用的转换行为设置。
     * @endif
     *
     * @lang{EN}
     * @brief Adjusts the behavior of the underlying encoding conversion; `stdin_sync{s}` is
     * the same as `sync_with_stdio(s)`.
     *
     * The synchronization state lives both in the root converter's flag and in this stream's
     * `synced_with_stdio()`, and a `stdin_sync` sent straight to the root would change only the
     * former, leaving the two apart so that a later `sync_with_stdio()` returns early on the
     * unchanged query value. So this stream has an `adjust` of its own: the one in
     * `std_stream_common_operators` is protected, out of reach even by a qualified name; any
     * other behavior goes there as before. `stdin_sync` is `final`, so no combined type carrying
     * another behavior too is caught here.
     *
     * @param acc The conversion-behavior settings to apply.
     * @endif
     */
    void adjust(this auto& self, const cvt_behavior& acc)
    {
        if (const auto* s = dynamic_cast<const stdin_sync*>(&acc); s)
            self.sync_with_stdio(s->synced);
        else
            self.std_stream_common_operators::adjust(acc);   // through self: see stdout_api::adjust
    }

    /**
     * @lang{ZH}
     * @brief 已删除：标准流不能取出设备。
     *
     * 普通流（`stream_common_operators`）上公开的换设备接口在标准流上删除：本流的设备是
     * 固定的 fd 0，取出去就再也装不回来，换进去等于给一个进程级单例改写底层来源。需要
     * 「在同一 fd 上从头开始」请用 `reset()`（它走的是 iochannel 那一层的 `attach()`，装一个
     * 同 fd 的缺省设备）。
     * `std_stream_common_operators` 不向标准流提供这两个函数（公开版本受 `requires (!is_std)`
     * 约束），`cin.IOv2::std_stream_common_operators::detach()` 这样的限定名调用同样编译不过。
     * @endif
     *
     * @lang{EN}
     * @brief Deleted: a standard stream cannot hand out its device.
     *
     * The device-replacing interface that is public on the ordinary streams
     * (`stream_common_operators`) is deleted on the standard streams: this stream's device is
     * the fixed fd 0, taking it out leaves no way to put it back, and putting another one in
     * rewrites the source of a process-wide singleton. To start over on the same fd use
     * `reset()`, which goes through the `attach()` one layer down, in the iochannel, with a
     * default device on the same fd. `std_stream_common_operators` does not offer either one
     * to the standard streams (the public versions are constrained with `requires (!is_std)`),
     * so a qualified call such as `cin.IOv2::std_stream_common_operators::detach()` does not
     * compile either.
     * @endif
     */
    std::pair<device_type, std::exception_ptr> detach() = delete;

    /**
     * @lang{ZH}
     * @brief 已删除：标准流不能换入设备。理由与替代做法见 `detach()`。
     * @endif
     *
     * @lang{EN}
     * @brief Deleted: a standard stream cannot take another device. See `detach()` for why,
     * and for what to use instead.
     * @endif
     */
    void attach(device_type&&) = delete;

    /**
     * @lang{ZH}
     * @brief 在同一 fd 上继续：清状态位与异常掩码，丢弃已缓冲但未消费的输入，重新附接
     * 设备并重新初始化转换器。`stdin` 是普通文件时也不会回到开头。同步模式不往前多读，
     * 本流尚未读走的字节（例如本行余下的部分，在 `stdin` 的 `FILE` 缓冲或 fd 上）照旧在那里；
     * 但本流已经取出、还没交给调用方的字节同样丢弃：`wcin` 遇到解码错误后已取出、尚未交出的字节（同步时已还给 C stdio
     * 的除外，见下），退回时进了字节缓冲的字符，以及此前不同步时
     * 整块预读进缓冲、切到同步后还没交出的字节（可达一整块缓冲）。偷看的字符在操作结束时
     * 已经退回：同步时它归 C stdio，`reset()` 丢不掉（`cin >> n` 读 `12x34` 后，`x` 仍在
     * `FILE` 里）；不同步时它在字节缓冲里，随缓冲丢弃。
     *
     * 字节缓冲与设备由所有标准输入流共用（见 `stdin_root_cvt`）：丢掉的已预读字节对 `cin`、
     * `wcin` 都丢掉，`wcin.reset()` 也会丢掉 `cin` 预读的字节。
     *
     * 供需要放弃已缓冲但未消费的输入的场合使用。它**不能**用来丢掉本行剩余的内容：同步模式下
     * 它至多丢掉偷看的那个字符，不同步模式下则可能连预读进来的后续几行一起丢掉；提取失败后要
     * 丢掉本行剩余，请用 `clear()` 加 `ignore(std::numeric_limits<std::ptrdiff_t>::max(), '\n')`。
     * 它**不是**出错后的必经之路：解码失败后 `clear()` 即可继续，解码器跳过坏序列的首字节、
     * 从下一个字节接着解码（块读与逐字符读跳过的一样多），`switch_code()` 也随之可用。
     * 例外是输入在一个字符中间到达 EOF：那半个字符无法完成转换，此后每次读取都置
     * `cvtfailbit`，不会到达 eof，`clear()` 不够。不同步时（以及 Windows 上）它留在本流的缓冲里，
     * 须 `reset()` 丢掉；同步时它已经还给 C stdio（见 `stdin_root_cvt`），不再归本流所有，
     * `reset()` 丢不掉，须由 C stdio 读走（如 `getchar()`）。
     * 不要在一次提取进行中（用户 `io_traits::sread` 里）重入调用：不会崩，但本次提取之后
     * 已缓冲的输入随之丢弃。
     *
     * 到达过的 EOF 也一并忘掉，之后可以接着读：不同步时它记在设备里，随重新附接清掉；同步时
     * 它是 `stdin` 的 `FILE` 上的粘性标志，本函数为此调用 `clearerr(stdin)`（同时清掉错误标志）。
     *
     * 复位的范围只有状态位、异常掩码，以及缓冲与转换器的内部状态。格式状态（含 `skipws`）、
     * `width()`、`precision()`、`fill()`、locale、`sync_with_stdio()`、`tie()` 与
     * `switch_code()` 选定的编码都**保持原样**（`std::basic_ios::clear` 同样不动格式状态）；
     * 拿它在单元测试用例之间复位时要留意这一点：上个用例留下的 `noskipws` 不会被清掉。
     *
     * @note 重新附接这一步在标准流上**没有可失败的操作**：装的是同一 fd 的缺省设备，不分配、
     *       不做 I/O、不重建 locale（转换状态的「重建」只是 `mbstate_t` 复位）——实测全部八个标准流在
     *       全部分配失败且 fd 指向 `/dev/full` 时 `reset()` 均 0 次分配、状态位全 0。围住它的
     *       `handle_exception` 只是兜底：若将来这里真抛了什么，转换器会停在未初始化状态，流不可用，
     *       须再次 `reset()`，`clear()` 不够。丢弃缓冲这一步在 fd 0 上同样不会失败（stdin 不可定位，
     *       重定位那步的异常在 `iochannel::detach` 里就被有意吞掉了），置 `devfailbit` 的那条路在这里
     *       走不到；除输出侧多一步旧设备 `dflush()` 外，形状与 `stdout_api::reset()` 一致。
     * @endif
     *
     * @lang{EN}
     * @brief Carries on on the same fd: clears the state bits and the exception mask,
     * drops input that was buffered but not yet consumed, reattaches the device and
     * re-initializes the converter. A `stdin` that is a regular file does not rewind.
     * Synchronized mode reads nothing ahead, so bytes this stream has not yet read (the rest
     * of the line, say, in `stdin`'s `FILE` buffer or on the fd) are still there; but bytes
     * this stream has already taken and not yet handed to the caller are dropped too: the
     * bytes `wcin` had taken past a decoding error (except those already handed back to C
     * stdio when synchronized, see below), characters that went into the byte buffer when
     * handed back, and what an earlier unsynchronized mode
     * read ahead into the buffer and the switch to synchronized has not handed out yet (up to
     * a whole buffer). A character peeked at has been handed back by the end of the
     * operation: synchronized it is C stdio's, so `reset()` cannot drop it (after `cin >> n`
     * reads `12x34` the `x` is still in the `FILE`); unsynchronized it is in the byte buffer
     * and goes with it.
     *
     * The byte buffer and the device are shared by every standard input stream (see
     * `stdin_root_cvt`): bytes read ahead that this drops are dropped for `cin` and `wcin`
     * alike, so `wcin.reset()` drops what `cin` read ahead too.
     *
     * For the cases that want to abandon input that was buffered but not yet consumed. It
     * does **not** discard the rest of a line: in synchronized mode it drops at most the peeked
     * character, and in unsynchronized mode it may drop following lines read ahead as well;
     * to discard the rest of a line after a failed extraction, use `clear()` followed by
     * `ignore(std::numeric_limits<std::ptrdiff_t>::max(), '\n')`. It is **not**
     * the required step after a failure: after a decode failure `clear()` is enough to
     * carry on -- the decoder skips the first byte of the bad sequence and decodes on from
     * the next one (block and character-at-a-time reads skip the same), and `switch_code()`
     * is available again as well. The exception is input that reaches EOF in the middle of a
     * character: that half character cannot be completed, so every later read sets
     * `cvtfailbit` and none reaches eof, and `clear()` is not enough. Unsynchronized (and on
     * Windows) it stays in this stream's buffer and `reset()` drops it; synchronized it has
     * gone back to C stdio (see `stdin_root_cvt`) and is no longer this stream's, so
     * `reset()` cannot drop it and C stdio has to take it (`getchar()`, say). Do not re-enter
     * it from inside an extraction (a user
     * `io_traits::sread`): nothing crashes, but the input buffered beyond that extraction
     * is discarded with it.
     *
     * An EOF reached is forgotten as well, so reading can go on: unsynchronized it is kept
     * in the device and goes with the reattach; synchronized it is the sticky flag on
     * `stdin`'s `FILE`, for which this function calls `clearerr(stdin)` (clearing the error
     * flag along with it).
     *
     * What is reset is the state bits, the exception mask, and the internal state of the
     * buffer and the converter -- nothing else. The format flags (`skipws` among them),
     * `width()`, `precision()`, `fill()`, the locale, `sync_with_stdio()`, `tie()` and the
     * encoding chosen with `switch_code()` all **stay as they are**
     * (`std::basic_ios::clear` likewise leaves the format state alone); a unit test
     * resetting between cases has to keep that in mind, as the `noskipws` left behind by
     * the previous case is still set.
     *
     * @note Reattaching has **nothing that can fail** on a standard stream: it installs a
     *       default device on the same fd, allocates nothing, does no I/O and rebuilds no
     *       locale ("rebuilding" the conversion state is an `mbstate_t` reset) -- measured on
     *       all eight standard streams with every allocation failing and the fd on
     *       `/dev/full`: zero allocations, no state bit. The `handle_exception` around it is
     *       only a backstop: should something ever throw there, the converter is left
     *       uninitialized, the stream is unusable, and another `reset()` is required --
     *       `clear()` is not enough. Dropping the buffer cannot fail on fd 0 either (stdin is
     *       not positionable, and the exception from that reposition is swallowed on purpose
     *       in `iochannel::detach`), so the `devfailbit` path is unreachable here; apart from
     *       the output side's extra `dflush()` of the old device, the shape is the same as
     *       `stdout_api::reset()`.
     * @endif
     */
    void reset()
    {
        std::lock_guard guard(this->io_mutex());
        this->clear();
        this->exceptions(ios_defs::goodbit);

        auto detached = m_channel.detach();

        try { m_channel.attach(); }
        catch (...) { this->handle_exception(std::current_exception()); }

        // Synchronized, the EOF is FILE's sticky flag rather than the device's.
        if (detail::stdin_shared_imp.synced())
            std::clearerr(stdin);

        if (detached.second) this->handle_exception(detached.second);
    }

    /**
     * @lang{ZH}
     * @brief 本流当前把字节解码成 `wchar_t` 时使用的编码（locale）名。
     *
     * 返回的是转换器**实际持有**的那个 locale 自己报出的名字，不是当初传进去的实参：
     * 以 `""` 切换过的话，得到的是那一刻查环境得到的具体名字，不是 `""`；别名也按平台
     * 规范化（glibc 把 `"POSIX"` 报成 `"C"`）。不做 I/O。返回值可再交给 `switch_code()`
     * 得到同一个编码。
     *
     * 只是 `retrieve(code_cvt_access)` 的包装，走本流通用的加锁与错误处理：失败（只在转换器
     * 内核已被移出时发生，正常生命周期不可达）经 `handle_exception` 置 `otherfailbit`、返回空串，
     * `exceptions()` 掩码含该位时抛出。
     *
     * @return 当前编码名。
     * @endif
     *
     * @lang{EN}
     * @brief The name of the encoding (locale) this stream currently uses to decode bytes
     * into `wchar_t`.
     *
     * The name the locale the converter **actually holds** reports for itself, not the
     * argument it was given: after a switch to `""` this is the concrete name the environment
     * resolved to at that moment, not `""`, and aliases come back platform-normalized (glibc
     * reports `"POSIX"` as `"C"`). Does no I/O. The result can be handed back to
     * `switch_code()` to get the same encoding.
     *
     * A thin wrapper over `retrieve(code_cvt_access)`, so it shares the stream's locking and
     * error handling: a failure (only when the converter's kernel has been moved out, which a
     * live stream never reaches) goes through `handle_exception`, sets `otherfailbit` and yields
     * an empty string; it throws when the `exceptions()` mask includes that bit.
     *
     * @return The current encoding name.
     * @endif
     */
    std::string code()
        requires std::is_same_v<TChar, wchar_t>
    {
        code_cvt_access acc;
        this->retrieve(acc);
        return acc.code;
    }

    /**
     * @lang{ZH}
     * @brief 切换本流把字节解码成 `wchar_t` 时使用的编码（locale）。
     *
     * `new_code` 与 `code()` 相同时什么也不做——比较的是 `code()` 报出的**解析后**的名字与实参
     * 本身，所以 `""` 与别名（当前是 `zh_CN.UTF-8` 时给 `zh_CN.utf8`、当前是 `C` 时给 `POSIX`）
     * 都不会命中早退，会真的重建一次（结果相同，代价是一次 `newlocale`）。只是
     * `adjust(code_cvt_switch)` 的包装，走本流
     * 通用的加锁与错误处理：失败按状态位报告，`exceptions()` 掩码含该位时才抛出；失败时编码、
     * 已缓冲的字节都没有改变（`code_cvt_stdio::adjust` 把所有可能失败的步骤都放在提交之前）。
     * 解码失败之后不必先 `reset()`，`clear()` 后照常可用。缓冲里还没解码的字节（包括输入在
     * 字符中间到达 EOF 后留着的那半个字符）留在原处，切换后按新编码解码。
     *
     * @param new_code 新的编码名，须为 `newlocale()` 接受的 locale 名。`""` 按 POSIX 规则
     *        查环境（`LC_ALL` > `LC_CTYPE` > `LANG` > `"C"`）：查在此刻发生，切换成功后
     *        `code()` 报的是查到的具体名字，不是 `""`。环境变量指向不存在的 locale 时，`""`
     *        会失败（置 `cvtfailbit`），而启动时的同一环境是宽松地回退到 `"C"`；要回到启动时
     *        的编码，传 `IOv2::initial_locale_name(LC_CTYPE)`。
     * @return 调用前的编码名；失败时它仍是当前编码名。判断成败请在进入前保证 `good()`，之后查
     *         `cvt_fail()`；或者直接比较 `code()` 与目标——本函数不设 `good()` 门槛，状态位已置时
     *         照常切换、位不变。
     * @note 置 `cvtfailbit`：该名字不被 `newlocale()` 接受（含内嵌 NUL 的名字按全长拒绝，不在
     *       第一个 NUL 处截断），或该编码是状态依赖的。
     *       详见 `cvt/code_cvt_stdio.h`。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the encoding (locale) this stream uses to decode bytes into `wchar_t`.
     *
     * Does nothing when `new_code` equals `code()` -- the comparison is between the
     * **resolved** name `code()` reports and the argument itself, so `""` and aliases
     * (`zh_CN.utf8` while the current name is `zh_CN.UTF-8`, `POSIX` while it is `C`) miss the
     * early exit and do rebuild once, to the same result, at the cost of one `newlocale`.
     * A thin wrapper over
     * `adjust(code_cvt_switch)`, so it shares the stream's locking and error handling: a
     * failure is reported through the state bits and throws only when the `exceptions()` mask
     * includes the bit; on failure the encoding and the buffered bytes are unchanged
     * (`code_cvt_stdio::adjust` puts every step that can fail before the commit). A decode
     * failure needs no `reset()` first; after `clear()` it works as usual. Bytes in the buffer
     * not decoded yet (the half character left when input reached EOF in the middle of one
     * among them) stay where they are and decode by the new encoding after the switch.
     *
     * @param new_code The new encoding name; must be a locale name `newlocale()` accepts.
     *        `""` means "look at the environment" per POSIX (`LC_ALL` > `LC_CTYPE` > `LANG` >
     *        `"C"`); the lookup happens at this moment, and once the switch succeeds `code()`
     *        reports the concrete name it resolved to, not `""`. When the environment names a
     *        locale that does not exist, `""` fails (setting `cvtfailbit`), whereas startup
     *        falls back leniently to `"C"` in the same environment; to return to the startup
     *        encoding, pass `IOv2::initial_locale_name(LC_CTYPE)`.
     * @return The encoding name before the call; on failure that is still the current one.
     *         To tell the two apart enter with `good()` and check `cvt_fail()` afterwards, or
     *         compare `code()` with the target: this function has no `good()` gate, so with a
     *         state bit already set it switches as usual and leaves the bits alone.
     * @note Sets `cvtfailbit`: the name is not accepted by `newlocale()` (a name with an
     *       embedded NUL is rejected at its full length, not cut at the first NUL), or the
     *       encoding is state-dependent.
     *       See `cvt/code_cvt_stdio.h`.
     * @endif
     */
    std::string switch_code(const std::string& new_code)
        requires std::is_same_v<TChar, wchar_t>
    {
        // One lock over the query and the switch, so concurrent switches each return the
        // code the one before them set.
        std::lock_guard guard(this->io_mutex());
        auto res = code();
        if (res != new_code)
        {
            code_cvt_switch acc(new_code);
            this->adjust(acc);
        }
        return res;
    }

private:
    /**
     * @lang{ZH}
     * @brief 接手退回字符的转换器类型：标准流管线的最外层。`cin` 是根转换器本身，`wcin` 是根
     * 之上的解码层。
     * @endif
     *
     * @lang{EN}
     * @brief The type of the converter that takes characters handed back: the outermost layer
     * of a standard stream's pipeline. On `cin` that is the root itself, on `wcin` the decoding
     * layer above it.
     * @endif
     */
    using hand_back_cvt = std::conditional_t<std::is_same_v<TChar, char>,
                                             stdin_root_cvt,
                                             code_cvt_stdio<stdin_root_cvt>>;

    /**
     * @lang{ZH}
     * @brief 把读缓冲里偷看、压回的字符退回转换器（`hand_back_cvt::unget()`）。
     *
     * 尽力而为：没有 `m_cvt`，或退回失败时，字符留在读缓冲里，本流下次照样先读到，只是别人
     * 看不到。这种失败**不报告**：此时操作已经成功，置位会让 `if (cin >> x)` 为假，`clear()`
     * 加 `ignore()` 反而丢掉这些字符。
     * @endif
     *
     * @lang{EN}
     * @brief Hands the characters peeked at or put back in the read buffer to the converter
     * (`hand_back_cvt::unget()`).
     *
     * Best effort: without `m_cvt`, or when handing back fails, the characters stay in the read
     * buffer, and this stream still reads them first, only nobody else sees them. Such a failure
     * is **not reported**: the operation has succeeded by then, so a state bit would make
     * `if (cin >> x)` false, and `clear()` plus `ignore()` would then discard these characters.
     * @endif
     */
    void hand_back_peeked() noexcept
    {
        if (m_cvt == nullptr) return;
        try
        {
            m_channel.hand_back([this](auto first, auto last)
            {
                m_cvt->unget(first, last);
                return true;
            });
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            // Best effort, see above: the characters are still in the read buffer.
        }
    }

private:
    /**
     * @lang{ZH}
     * @brief 本流的输入通道：转换器管线、根转换器 `stdin_root_cvt` 与 fd 0 上的设备。
     * @endif
     *
     * @lang{EN}
     * @brief This stream's input channel: the converter pipeline, the root converter
     * `stdin_root_cvt` and the device on fd 0.
     * @endif
     */
    ichannel<device_type, char_type>      m_channel;

    /**
     * @lang{ZH}
     * @brief 本流的 locale，经 `std_stream_common_operators` 的 `locale()` 存取。
     * @endif
     *
     * @lang{EN}
     * @brief This stream's locale, accessed through `locale()` in
     * `std_stream_common_operators`.
     * @endif
     */
    IOv2::locale<char_type>                 m_locale;

    /**
     * @lang{ZH}
     * @brief 接手退回字符的转换器，构造时取一次。管线不是 `hand_back_cvt`（自定义创建器构造的
     * 流）时为空，本流不退回。
     *
     * 转换器在 `runtime_cvt` 指向的堆上，`attach`、`detach`、`adjust` 都不换掉它，本流又不可
     * 拷贝、不可移动，所以这个指针一直有效。须声明在 `m_channel` 之后。
     * @endif
     *
     * @lang{EN}
     * @brief The converter that takes characters handed back, found once at construction. Null
     * when the pipeline is not a `hand_back_cvt` (a stream built with a custom creator), and this
     * stream then does not hand back.
     *
     * The converter lives on the heap behind `runtime_cvt`, `attach`, `detach` and `adjust` do not
     * replace it, and this stream is neither copyable nor movable, so the pointer stays valid. It
     * must be declared after `m_channel`.
     * @endif
     */
    hand_back_cvt*                          m_cvt = m_channel.template cvt_target<hand_back_cvt>();
};

/**
 * @lang{ZH}
 * @brief 标准输入流 `cin`（`char`）的类型。
 *
 * 进程级单例（`sing_temp`），构造函数私有，经全局引用 `cin` 使用。退出钩子为空——与
 * `std::cin` 一样退出时不析构。构造时 `tie()` 到 `cout`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the standard input stream `cin` (`char`).
 *
 * A process-wide singleton (`sing_temp`) with a private constructor, used through the global
 * reference `cin`. Its exit hook is empty -- like `std::cin`, it is not destroyed at exit.
 * It ties itself to `cout` at construction.
 * @endif
 */
class cin_t : public stdin_api<std_input_device, char>
            , public sing_temp<cin_t>
{
    using BT = stdin_api<std_input_device, char>;
    friend sing_temp<cin_t>;

private:
    cin_t()
        : BT()
        , sing_temp<cin_t>([](cin_t*) noexcept {})   // never destroyed at exit, like std::cin
    {
        tie(&cout);
    }
};

#if defined(IOV2_SHARED)
extern IOV2_API cin_t& cin;   // defined in iov2_objects.cpp
#else
/// @cond INTERNAL
inline cin_t::init _cin_init;
/// @endcond
/**
 * @lang{ZH}
 * @brief 标准输入流对象（`char`），读 fd 0。
 *
 * `IOV2_SHARED` 下是 `libiov2.so` 里唯一一份的引用（定义在 `iov2_objects.cpp`），头文件模式下
 * 是本程序内的 `inline` 变量。可在哪些静态对象的构造 / 析构里使用，见 `objects.h`。
 * @endif
 *
 * @lang{EN}
 * @brief The standard input stream object (`char`), reading fd 0.
 *
 * Under `IOV2_SHARED` a reference to the single copy inside `libiov2.so` (defined in
 * `iov2_objects.cpp`); in header-only mode an `inline` variable of the program itself. For
 * which static objects may use it from their constructors and destructors, see `objects.h`.
 * @endif
 */
inline cin_t&      cin = _cin_init.get();
#endif

/**
 * @lang{ZH}
 * @brief 宽字符标准输入流 `wcin`（`wchar_t`）的类型。
 *
 * 进程级单例（`sing_temp`），构造函数私有，经全局引用 `wcin` 使用。启动时以
 * `initial_locale_name(LC_CTYPE)` 选定的编码解码字节（`code_cvt_stdio`）。退出钩子为空——
 * 与 `std::wcin` 一样退出时不析构。构造时 `tie()` 到 `wcout`。
 * @endif
 *
 * @lang{EN}
 * @brief The type of the wide standard input stream `wcin` (`wchar_t`).
 *
 * A process-wide singleton (`sing_temp`) with a private constructor, used through the global
 * reference `wcin`. It decodes bytes (`code_cvt_stdio`) in the encoding
 * `initial_locale_name(LC_CTYPE)` picks at startup. Its exit hook is empty -- like
 * `std::wcin`, it is not destroyed at exit. It ties itself to `wcout` at construction.
 * @endif
 */
class wcin_t : public stdin_api<std_input_device, wchar_t>
             , public sing_temp<wcin_t>
{
    using BT = stdin_api<std_input_device, wchar_t>;
    friend sing_temp<wcin_t>;

private:
    wcin_t()
        : BT(code_cvt_stdio_creator(IOv2::initial_locale_name(LC_CTYPE)))
        , sing_temp<wcin_t>([](wcin_t*) noexcept {})   // never destroyed at exit, like std::wcin
    {
        tie(&wcout);
    }
};

#if defined(IOV2_SHARED)
extern IOV2_API wcin_t& wcin;   // defined in iov2_objects.cpp
#else
/// @cond INTERNAL
inline wcin_t::init _wcin_init;
/// @endcond
/**
 * @lang{ZH}
 * @brief 宽字符标准输入流对象（`wchar_t`），读 fd 0。
 *
 * `IOV2_SHARED` 下是 `libiov2.so` 里唯一一份的引用（定义在 `iov2_objects.cpp`），头文件模式下
 * 是本程序内的 `inline` 变量。可在哪些静态对象的构造 / 析构里使用，见 `objects.h`。
 * @endif
 *
 * @lang{EN}
 * @brief The wide standard input stream object (`wchar_t`), reading fd 0.
 *
 * Under `IOV2_SHARED` a reference to the single copy inside `libiov2.so` (defined in
 * `iov2_objects.cpp`); in header-only mode an `inline` variable of the program itself. For
 * which static objects may use it from their constructors and destructors, see `objects.h`.
 * @endif
 */
inline wcin_t&      wcin = _wcin_init.get();
#endif
}
