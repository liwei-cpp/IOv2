// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file out_impl.h
 * @lang{ZH}
 * 定义标准输出流的实现模板 `stdout_api`，以及六个标准输出流对象：`cout` / `cerr` / `clog`
 * （`char`）与 `wcout` / `wcerr` / `wclog`（`wchar_t`）。
 *
 * `stdout_api` 不由 `ostream` 派生，而是同样把一条 `ostreambuf`（其下依次是转换器管线与固定 fd
 * 的设备 `std_device<STDOUT_FILENO>` 或 `std_device<STDERR_FILENO>`）与一个 `locale` 组合起来，
 * 对外接口来自 `ios_state`（状态位与异常掩码）、`out_flusher`（tie 刷新用的多态 `try_flush()`）、
 * `ostream_operators`（输出操作）与 `stream_common_operators`（`tell()` / `locale()` 等）四个
 * 基类。与 `ostream` 的差别都来自「设备是固定 fd 的进程级单例」：多了 `sync_with_stdio()`、
 * `reset()` 与（宽流）`code()` / `switch_code()`，换设备的 `detach()` / `attach()` 则被
 * `= delete`。
 *
 * 六个流对象经 `sing_temp` 成为进程级单例，退出钩子只 `try_flush()`、不析构——与 `std::cout`
 * 一样，退出阶段既有引用仍然有效。`cerr` / `wcerr` 构造时另外 `tie()` 到 `cout` / `wcout` 并置
 * `ios_defs::unitbuf`，三个 `char` 流与三个宽流共用 fd 1 / fd 2。
 *
 * @warning **退出时的刷新是「尽力而为」的**：钩子走 `out_flusher::try_flush()`，以
 *          `try_to_lock` 至多试三次取本流的 `io_mutex()`，取不到就放弃，那一批已缓冲的字节
 *          丢掉。这是有意的取舍——若改成阻塞取锁，另一个线程持锁不放（在用户的
 *          `io_traits::swrite` 里等网络、等一个本该由正在退出的线程唤醒的条件变量，或
 *          `tie` 成环）就会让 `exit()` 永不返回；glibc 的 `_IO_cleanup` 用
 *          `_IO_flush_all_lockp(0)` 不取 FILE 锁，正是同一个理由。要确保某一批输出一定到达
 *          设备，请在退出前自己 `flush()`，那时还有调用栈可以报告失败。
 *
 * @note 一般不直接包含本头文件，而是包含 `IOv2/io/objects/objects.h`：入口那里还有一次切换全部
 *       八个标准流的 `sync_with_stdio()` 与 `endl` / `ends` / `flush` 等操纵符，并说明了本系列
 *       头文件不带来哪些能力。
 * @endif
 *
 * @lang{EN}
 * Defines `stdout_api`, the implementation template behind the standard output streams, along
 * with the six standard output stream objects: `cout` / `cerr` / `clog` (`char`) and
 * `wcout` / `wcerr` / `wclog` (`wchar_t`).
 *
 * `stdout_api` does not derive from `ostream`; it combines, in the same way, an `ostreambuf`
 * (below which sit the converter pipeline and the fixed-fd device `std_device<STDOUT_FILENO>`
 * or `std_device<STDERR_FILENO>`) with a `locale`, and takes its interface from four bases --
 * `ios_state` (the state bits and the exception mask), `out_flusher` (the polymorphic
 * `try_flush()` used by tie), `ostream_operators` (the output operations) and
 * `stream_common_operators` (`tell()` / `locale()` and friends). Every difference from
 * `ostream` follows from the device being a fixed fd owned by a process-wide singleton: it
 * adds `sync_with_stdio()`, `reset()` and, on the wide streams, `code()` / `switch_code()`,
 * while `detach()` / `attach()`, which would replace the device, are `= delete`.
 *
 * All six stream objects are process-wide singletons through `sing_temp` whose exit hook only
 * calls `try_flush()` and never destroys them -- like `std::cout`, existing references stay
 * valid once exit begins. `cerr` / `wcerr` additionally tie themselves to `cout` / `wcout` at
 * construction and set `ios_defs::unitbuf`; the three `char` streams and the three wide
 * streams share fd 1 / fd 2 pairwise.
 *
 * @warning **The flush at exit is best-effort.** The hook goes through
 *          `out_flusher::try_flush()`, which takes this stream's `io_mutex()` with
 *          `try_to_lock` for at most three attempts and gives up otherwise, dropping whatever
 *          was buffered. That is the deliberate trade-off: with a blocking lock, another
 *          thread holding it and not letting go -- parked inside a user's
 *          `io_traits::swrite` on a socket or on a condition variable the exiting thread was
 *          supposed to signal, or a `tie` cycle -- would keep `exit()` from ever returning.
 *          glibc's `_IO_cleanup` uses `_IO_flush_all_lockp(0)`, which takes no FILE lock, for
 *          the same reason. To be sure a particular batch of output reaches the device,
 *          `flush()` it yourself before exiting, while there is still a call stack to report
 *          a failure on.
 *
 * @note Prefer including `IOv2/io/objects/objects.h` over this header: the entry point also
 *       brings the `sync_with_stdio()` that switches all eight standard streams at once and
 *       the `endl` / `ends` / `flush` manipulators, and documents what this family of headers
 *       does not bring in.
 * @endif
 */
#pragma once
#include <IOv2/common/copyable_atomic.h>
#include <IOv2/common/iov2_export.h>
#include <IOv2/common/sing_temp.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/device/device_concepts.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/streambuf.h>
#include <IOv2/io/streambuf_iterator.h>
#include <IOv2/io/utilities/ostream_operators.h>
#include <IOv2/io/utilities/stream_common_operators.h>
#include <IOv2/locale/locale.h>

#include <clocale>
#include <exception>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

#include <unistd.h>

namespace IOv2
{
template <typename T, io_device TDevice, typename TChar>
class stdout_api : public ios_state<TChar>
                 , public out_flusher<T>
                 , public ostream_operators<TChar>
                 , public stream_common_operators
{
public:
    using device_type = TDevice;
    using char_type = TChar;
    using out_sentry_type = out_sentry<T, false, true>;
    using out_iter_type = ostreambuf_iterator<ostreambuf<device_type, char_type>>;

    friend out_sentry_type;
    friend out_flusher<T>;
    friend ostream_operators<TChar>;
    friend stream_common_operators;

public:
    stdout_api()
        : m_streambuf(device_type{}) {}

    template <cvt_creator TCreator>
    stdout_api(const TCreator& creator)
        : m_streambuf(device_type{}, creator) {}

public:
    /**
     * @lang{ZH}
     * @brief 切换本流是否与 C stdio 同步：同步时每次插入结束都把本流缓冲推进 stdio 缓冲，
     * 以保持与 `printf` 等的交错顺序；不同步时本流自行缓冲。
     *
     * 只是一次原子交换，不会失败；与并发的插入操作安全竞争。
     *
     * @param sync `true` 为同步（默认），`false` 为自行缓冲。
     * @return 调用前的同步状态。
     * @endif
     *
     * @lang{EN}
     * @brief Switches whether this stream is synchronized with C stdio: synchronized
     * pushes this stream's buffer into the stdio buffer at the end of every insertion,
     * keeping the interleaving with `printf` and friends; unsynchronized buffers on its
     * own.
     *
     * A single atomic exchange, cannot fail; safe against concurrent insertions.
     *
     * @param sync `true` for synchronized (the default), `false` for own buffering.
     * @return The synchronization state before the call.
     * @endif
     */
    bool sync_with_stdio(bool sync = true) noexcept
    {
        return m_sync_with_stdio.exchange(sync);
    }

    /**
     * @lang{ZH}
     * @brief 本流当前把 `wchar_t` 编码成字节时使用的编码（locale）名。
     *
     * 返回的是转换器**实际持有**的那个 locale 自己报出的名字，不是当初传进去的实参：
     * 以 `""` 切换过的话，得到的是那一刻查环境得到的具体名字，不是 `""`；别名也按平台
     * 规范化（glibc 把 `"POSIX"` 报成 `"C"`）。不做 I/O。返回值可再交给 `switch_code()`
     * 得到同一个编码。
     *
     * 只是 `retrieve(code_cvt_access)` 的包装，走本流通用的加锁与错误处理：失败（只在转换器
     * 内核已被移出时发生，正常生命周期不可达）经 `handle_exception` 置 `cvtfailbit`、返回空串，
     * `exceptions()` 掩码含该位时抛出。
     *
     * @return 当前编码名。
     * @endif
     *
     * @lang{EN}
     * @brief The name of the encoding (locale) this stream currently uses to encode
     * `wchar_t` into bytes.
     *
     * The name the locale the converter **actually holds** reports for itself, not the
     * argument it was given: after a switch to `""` this is the concrete name the environment
     * resolved to at that moment, not `""`, and aliases come back platform-normalized (glibc
     * reports `"POSIX"` as `"C"`). Does no I/O. The result can be handed back to
     * `switch_code()` to get the same encoding.
     *
     * A thin wrapper over `retrieve(code_cvt_access)`, so it shares the stream's locking and
     * error handling: a failure (only when the converter's kernel has been moved out, which a
     * live stream never reaches) goes through `handle_exception`, sets `cvtfailbit` and yields
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
     * @brief 切换本流把 `wchar_t` 编码成字节时使用的编码（locale）。
     *
     * `new_code` 与 `code()` 相同时什么也不做。只是 `adjust(code_cvt_switch)` 的包装，走本流
     * 通用的加锁与错误处理：失败按状态位报告，`exceptions()` 掩码含该位时才抛出；失败时编码
     * 不切换。若转换器进入本函数时未 tainted，`code_cvt_stdio::adjust` 把所有可能失败的步骤都
     * 放在提交之前，已缓冲的字节也原样保留。若转换器已 tainted，则切换前须先重新附接同一 fd；
     * 这一步会终结旧转换器流并冲刷旧设备，冲刷失败时那批字节已经丢了。
     *
     * @param new_code 新的编码名，须为 `newlocale()` 接受的 locale 名。`""` 按 POSIX 规则
     *        查环境（`LC_ALL` > `LC_CTYPE` > `LANG` > `"C"`）：查在此刻发生，切换成功后
     *        `code()` 报的是查到的具体名字，不是 `""`。
     * @return 调用前的编码名；失败时它仍是当前编码名。判断成败请在进入前保证 `good()`，之后查
     *         `cvt_fail()` / `dev_fail()`；或者直接比较 `code()` 与目标——本函数不设 `good()` 门槛，
     *         状态位已置时照常切换、位不变。
     * @note 置 `cvtfailbit`：该名字不被 `newlocale()` 接受、编码转换状态不处于初始状态（有状态
     *       编码写出非 ASCII 之后），或已 tainted 转换器的预先恢复无法完成终结。
     *       置 `devfailbit`：转换器已 tainted，且预先恢复时旧设备冲刷失败。详见
     *       `cvt/code_cvt_stdio.h`。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the encoding (locale) this stream uses to encode `wchar_t` into bytes.
     *
     * Does nothing when `new_code` equals `code()`. A thin wrapper over
     * `adjust(code_cvt_switch)`, so it shares the stream's locking and error handling: a
     * failure is reported through the state bits and throws only when the `exceptions()` mask
     * includes the bit; on failure the encoding is not switched. If the converter is not
     * tainted on entry, `code_cvt_stdio::adjust` puts every step that can fail before the
     * commit and the buffered bytes stay as they were. A tainted converter must first be
     * reattached to the same fd; that step finalizes the old converter stream and flushes the
     * old device, and if that flush fails those bytes are already gone.
     *
     * @param new_code The new encoding name; must be a locale name `newlocale()` accepts.
     *        `""` means "look at the environment" per POSIX (`LC_ALL` > `LC_CTYPE` > `LANG` >
     *        `"C"`); the lookup happens at this moment, and once the switch succeeds `code()`
     *        reports the concrete name it resolved to, not `""`.
     * @return The encoding name before the call; on failure that is still the current one.
     *         To tell the two apart enter with `good()` and check `cvt_fail()` / `dev_fail()`
     *         afterwards, or compare `code()` with the target: this function has no `good()`
     *         gate, so with a state bit already set it switches as usual and leaves the bits
     *         alone.
     * @note Sets `cvtfailbit`: the name is not accepted by `newlocale()`, the encoding
     *       conversion state is not in its initial state (after a stateful encoding has
     *       written non-ASCII), or preliminary recovery cannot finalize a tainted converter.
     *       Sets `devfailbit`: the converter was tainted and flushing the old device during
     *       the preliminary recovery failed. See `cvt/code_cvt_stdio.h`.
     * @endif
     */
    std::string switch_code(const std::string& new_code)
        requires std::is_same_v<TChar, wchar_t>
    {
        auto res = code();
        if (res != new_code)
        {
            code_cvt_switch acc(new_code);
            this->adjust(acc);
        }
        return res;
    }

    /**
     * @lang{ZH}
     * `stream_common_operators` 的换设备接口在标准流上删除：本流的设备是固定的 fd 1 / fd 2，
     * 取出去就再也装不回来，换进去等于给一个进程级单例改写底层目标。需要「在同一 fd 上重新
     * 开始」请用 `reset()`（它走的是 streambuf 那一层的 `attach()`，装一个同 fd 的缺省设备）。
     * @endif
     *
     * @lang{EN}
     * The device-replacing interface of `stream_common_operators` is deleted on the standard
     * streams: this stream's device is the fixed fd 1 / fd 2, taking it out leaves no way to
     * put it back, and putting another one in rewrites the target of a process-wide singleton.
     * To start over on the same fd use `reset()`, which goes through the `attach()` one layer
     * down, in the streambuf, with a default device on the same fd.
     * @endif
     */
    std::pair<device_type, std::exception_ptr> detach() = delete;
    void attach(device_type&&) = delete;

    /**
     * @lang{ZH}
     * @brief 在同一 fd 上继续：清状态位与异常掩码，把已缓冲的输出写出到 fd，重新附接设备
     * 并重新初始化转换器。不做重定位，fd 是普通文件时也不会回到开头。
     *
     * 供需要清掉状态位与异常掩码、让缓冲与转换器回到初始态后继续用的场合使用。它**不是**
     * 出错后的必经之路：编码失败置 `cvtfailbit` 后 `clear()` 即可继续（转换器会自行重新附接
     * 同一 fd，见 `code_cvt_stdio`），与 `std::wcout` 的用法相同。
     *
     * 复位的范围只有状态位、异常掩码，以及缓冲与转换器的内部状态。格式状态、`width()`、
     * `precision()`、`fill()`、locale、`sync_with_stdio()`、`tie()`、`unitbuf` 与
     * `switch_code()` 选定的编码都**保持原样**（`std::basic_ios::clear` 同样不动格式状态）；
     * 拿它在单元测试用例之间复位时要留意这一点：上个用例留下的 `hex` 或 `unitbuf` 不会被清掉。
     *
     * 已缓冲的那批字节是交给旧设备写出的；写不出去（fd 是 `/dev/full`、管道已断、tty 已挂断）
     * 才丢失，此时不抛出，而是置 `devfailbit`：字节已经丢了，而 `reset()` 没有返回值，状态位是
     * 唯一能报告这件事的通道。这一位不妨碍继续用，`clear()` 之后照常插入。
     * 旧设备的 `dflush()` 在其析构前显式调用，因此 stdio 自身缓冲区的失败也经同一通道报告；
     * 若转换器清理与设备冲刷均失败，保留较早发生的转换器清理错误。
     * @note `dflush()` 冲刷的是整个 `stdout` / `stderr` 的 `FILE` 缓冲，因此报告的失败也可能
     *       涉及调用方通过 `printf` 等 C stdio 接口写入的字节。
     *
     * @note 换设备本身失败（`cvtfailbit` / `otherfailbit`，例如宽流重建编码转换状态失败）是另一
     *       回事：那时转换器没有初始化完，流不可用，须再次 `reset()`，`clear()` 不够。
     * @note 实现上是 `detach()` 加 `attach()` 两步，而不是一次 `attach()`：`streambuf::attach()`
     *       会在第一步把旧设备的冲刷失败重抛出来，第二步（初始化转换器）因此不执行，转换器停在
     *       `io_status::neutral`，`clear()` 也救不回——详见 `stream_common_operators::attach()`
     *       上的 `@warning`。`detach()` 是 `noexcept` 的，把那个错误作为返回值交出来，之后的
     *       `attach()` 面对的是空缓冲，没有东西可重抛。
     * @endif
     *
     * @lang{EN}
     * @brief Carries on on the same fd: clears the state bits and the exception mask,
     * writes the buffered output out to the fd, reattaches the device and re-initializes
     * the converter. Nothing is repositioned: an fd that is a regular file does not rewind.
     *
     * For the cases that want the state bits and the exception mask cleared and the buffer
     * and the converter back in their initial state before carrying on. It is **not** the
     * required step after a failure: once an encoding failure has set `cvtfailbit`,
     * `clear()` is enough to carry on (the converter reattaches the same fd by itself,
     * see `code_cvt_stdio`), just as with `std::wcout`.
     *
     * What is reset is the state bits, the exception mask, and the internal state of the
     * buffer and the converter -- nothing else. The format flags, `width()`, `precision()`,
     * `fill()`, the locale, `sync_with_stdio()`, `tie()`, `unitbuf` and the encoding chosen
     * with `switch_code()` all **stay as they are** (`std::basic_ios::clear` likewise leaves
     * the format state alone); a unit test resetting between cases has to keep that in mind,
     * as the `hex` or `unitbuf` left behind by the previous case is still set.
     *
     * The buffered bytes are written out through the old device. They are lost only if that
     * write fails (the fd is `/dev/full`, the pipe is gone, the tty hung up), which does not
     * throw but sets `devfailbit`: the bytes are gone and `reset()` returns nothing, so a
     * state bit is the only channel left to report it. That bit does not stand in the way:
     * after `clear()` insertions work as usual.
     * The old device's `dflush()` is called explicitly before its destructor, so failures in
     * stdio's own buffer are reported through the same channel. If converter cleanup and the
     * device flush both fail, the earlier converter-cleanup error is preserved.
     * @note `dflush()` flushes the whole `stdout` / `stderr` `FILE` buffer, so the reported
     *       failure may also involve bytes written by the caller through C stdio such as
     *       `printf`.
     *
     * @note Failing to replace the device is a different matter (`cvtfailbit` /
     *       `otherfailbit`, e.g. a wide stream unable to rebuild its conversion state):
     *       the converter is then left uninitialized, the stream is unusable, and another
     *       `reset()` is required -- `clear()` is not enough.
     * @note This is implemented as `detach()` plus `attach()`, not as one `attach()`:
     *       `streambuf::attach()` rethrows the old device's flush failure in its first step, so
     *       its second step (initializing the converter) does not run and the converter is left
     *       in `io_status::neutral`, which `clear()` cannot recover -- see the `@warning` on
     *       `stream_common_operators::attach()`. `detach()` is `noexcept` and hands that error
     *       back as a value, so the `attach()` that follows faces an empty buffer and has
     *       nothing to rethrow.
     * @endif
     */
    void reset()
    {
        std::lock_guard guard(this->io_mutex());
        this->clear();
        this->exceptions(ios_defs::goodbit);

        auto detached = m_streambuf.detach();

        try { detached.first.dflush(); }
        catch (...)
        {
            if (!detached.second)
                detached.second = std::current_exception();
        }

        try { m_streambuf.attach(); }
        catch (...) { this->handle_exception(std::current_exception()); }

        if (detached.second) this->handle_exception(detached.second);
    }

protected:
    ostreambuf<device_type, char_type> m_streambuf;
    IOv2::locale<char_type> m_locale;
    copyable_atomic<bool> m_sync_with_stdio{true};   ///< @lang{ZH} 为 true 时每次插入结束（输出哨兵析构）都把本流缓冲推进 stdio 缓冲；与进程退出时的刷新无关。原子量，使 `sync_with_stdio()` 可与并发输出操作安全竞争。 @endif @lang{EN} When true, every insertion (the output sentry's destructor) pushes this stream's buffer into the stdio buffer; unrelated to the flush at process exit. Atomic so `sync_with_stdio()` is safe against concurrent output operations. @endif
};

/// cout
class cout_t : public stdout_api<cout_t, std_device<STDOUT_FILENO>, char>
             , public sing_temp<cout_t>
{
    using BT = stdout_api<cout_t, std_device<STDOUT_FILENO>, char>;
    friend sing_temp<cout_t>;

private:
    cout_t()
        : sing_temp<cout_t>([](cout_t* p) noexcept { p->try_flush(); })
    {}

    cout_t(const cout_t&) = delete;
    cout_t& operator=(const cout_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API cout_t& cout;   // defined in iov2_objects.cpp
#else
inline cout_t::init _cout_init;
inline cout_t&      cout = _cout_init.get();
#endif

/// cerr
class cerr_t : public stdout_api<cerr_t, std_device<STDERR_FILENO>, char>
             , public sing_temp<cerr_t>
{
    using BT = stdout_api<cerr_t, std_device<STDERR_FILENO>, char>;
    friend sing_temp<cerr_t>;

private:
    cerr_t()
        : BT()
        , sing_temp<cerr_t>([](cerr_t* p) noexcept { p->try_flush(); })
    {
        tie(&cout);
        setf(ios_defs::unitbuf);
    }

    cerr_t(const cerr_t&) = delete;
    cerr_t& operator=(const cerr_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API cerr_t& cerr;   // defined in iov2_objects.cpp
#else
inline cerr_t::init _cerr_init;
inline cerr_t&      cerr = _cerr_init.get();
#endif

/// clog
class clog_t : public stdout_api<clog_t, std_device<STDERR_FILENO>, char>
             , public sing_temp<clog_t>
{
    using BT = stdout_api<clog_t, std_device<STDERR_FILENO>, char>;
    friend sing_temp<clog_t>;

private:
    clog_t()
        : sing_temp<clog_t>([](clog_t* p) noexcept { p->try_flush(); })
    {}

    clog_t(const clog_t&) = delete;
    clog_t& operator=(const clog_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API clog_t& clog;   // defined in iov2_objects.cpp
#else
inline clog_t::init _clog_init;
inline clog_t&      clog = _clog_init.get();
#endif

/// wcout
class wcout_t : public stdout_api<wcout_t, std_device<STDOUT_FILENO>, wchar_t>
              , public sing_temp<wcout_t>
{
    using BT = stdout_api<wcout_t, std_device<STDOUT_FILENO>, wchar_t>;
    friend sing_temp<wcout_t>;

private:
    wcout_t()
        : BT(code_cvt_stdio_creator(IOv2::locale<char>::initial_locale_name(LC_CTYPE)))
        , sing_temp<wcout_t>([](wcout_t* p) noexcept { p->try_flush(); })
    {}

    wcout_t(const wcout_t&) = delete;
    wcout_t& operator=(const wcout_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API wcout_t& wcout;   // defined in iov2_objects.cpp
#else
inline wcout_t::init _wcout_init;
inline wcout_t&      wcout = _wcout_init.get();
#endif

/// wcerr
class wcerr_t : public stdout_api<wcerr_t, std_device<STDERR_FILENO>, wchar_t>
              , public sing_temp<wcerr_t>
{
    using BT = stdout_api<wcerr_t, std_device<STDERR_FILENO>, wchar_t>;
    friend sing_temp<wcerr_t>;

private:
    wcerr_t()
        : BT(code_cvt_stdio_creator(IOv2::locale<char>::initial_locale_name(LC_CTYPE)))
        , sing_temp<wcerr_t>([](wcerr_t* p) noexcept { p->try_flush(); })
    {
        tie(&wcout);
        setf(ios_defs::unitbuf);
    }

    wcerr_t(const wcerr_t&) = delete;
    wcerr_t& operator=(const wcerr_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API wcerr_t& wcerr;   // defined in iov2_objects.cpp
#else
inline wcerr_t::init _wcerr_init;
inline wcerr_t&      wcerr = _wcerr_init.get();
#endif

/// wclog
class wclog_t : public stdout_api<wclog_t, std_device<STDERR_FILENO>, wchar_t>
              , public sing_temp<wclog_t>
{
    using BT = stdout_api<wclog_t, std_device<STDERR_FILENO>, wchar_t>;
    friend sing_temp<wclog_t>;

private:
    wclog_t()
        : BT(code_cvt_stdio_creator(IOv2::locale<char>::initial_locale_name(LC_CTYPE)))
        , sing_temp<wclog_t>([](wclog_t* p) noexcept { p->try_flush(); })
    {}

    wclog_t(const wclog_t&) = delete;
    wclog_t& operator=(const wclog_t&) = delete;
};

#if defined(IOV2_SHARED)
extern IOV2_API wclog_t& wclog;   // defined in iov2_objects.cpp
#else
inline wclog_t::init _wclog_init;
inline wclog_t&      wclog = _wclog_init.get();
#endif
}
