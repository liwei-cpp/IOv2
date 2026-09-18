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
 * 基类。与 `ostream` 的差别都来自「设备是固定 fd 的进程级单例」：多了 `sync_with_stdio()` /
 * `synced_with_stdio()`、`reset()` 与（宽流）`code()` / `switch_code()`，换设备的 `detach()` /
 * `attach()` 则被 `= delete`。
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
 * @note 上面说的「退出时」只指 `exit()`（含 `main` 返回）：钩子是经 `__cxa_atexit` 登记的静态
 *       析构。`std::quick_exit`、`_exit`、未捕获的信号与 `std::abort` **一概不跑钩子**，本流
 *       缓冲里的字节全部丢失；同步模式下已交给 stdio 的字节也一并丢失，因为这些路径同样不
 *       冲刷 `FILE` 缓冲——与 `std::cout` 在这些路径上的行为逐字节相同（实测）。要在这类路径
 *       之前保住输出，请自己 `flush()`，并在 stdout 不是 tty 时再 `fflush(stdout)`（或
 *       `setvbuf(stdout, nullptr, _IONBF, 0)`）。
 *
 * @note **「退出阶段仍可用」只覆盖本库自己的东西**：流对象、它们的 locale 与 facet，以及
 *       facet 触到的进程级数据（`timeio` 的时区树、`messages` 的文本域表）都不在退出时析构。
 *       用户经 `locale(loc)` 装进来的自定义 facet 若持有会在退出时析构的全局量，`main` 返回后
 *       另一线程的一次插入、或更早构造的静态对象析构里的一次插入，仍会读到已销毁的对象——
 *       这一层本库无从保证，与 `std::cout` 相同。
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
 * adds `sync_with_stdio()` / `synced_with_stdio()`, `reset()` and, on the wide streams, `code()` /
 * `switch_code()`, while `detach()` / `attach()`, which would replace the device, are `= delete`.
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
 * @note "At exit" above means `exit()` (including returning from `main`) only: the hooks are
 *       static destructors registered through `__cxa_atexit`. `std::quick_exit`, `_exit`, an
 *       unhandled signal and `std::abort` run **no** hook, so every byte still in this
 *       stream's buffer is lost; in synchronized mode the bytes already handed to stdio are
 *       lost too, since those paths do not flush `FILE` buffers either -- byte for byte what
 *       `std::cout` does on the same paths (measured). To keep output ahead of such a path,
 *       `flush()` yourself and, when stdout is not a tty, `fflush(stdout)` as well (or
 *       `setvbuf(stdout, nullptr, _IONBF, 0)`).
 *
 * @note **"Still usable while the process exits" covers this library's own parts only**: the
 *       stream objects, their locales and facets, and the process-wide data those facets reach
 *       (the time-zone trie of `timeio`, the text-domain table of `messages`) are none of them
 *       destroyed at exit. A user facet installed through `locale(loc)` that holds a global with a
 *       destructor is outside that: an insertion from another thread after `main` returned, or
 *       from the destructor of a static object constructed earlier, still reads a destroyed
 *       object. This library cannot vouch for that layer, and neither can `std::cout`.
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
     * 从「自行缓冲」切回「同步」时，此前缓冲的字节要在这里交给 stdio：标志由每次插入的
     * 输出哨兵读取，只影响之后的插入，若不在这里搬一次，那批字节会排到下一次 `printf`
     * 之后，甚至留到进程退出。因此切到 `true` 时本函数取本流的 `io_mutex()`，在锁内翻标志，
     * 若此前为 `false` 则把本流缓冲搬进 stdio 缓冲一次——与哨兵是同一个操作，**不** `fflush`：
     * stdio 缓冲里可能还有 `printf` 的字节，何时落盘由 stdio 决定。锁内翻标志保证任何从本函数
     * 返回的调用者都能依赖「此前缓冲的字节已在 stdio 手里」，无论搬运是自己做的还是先到的
     * 那次做的。切到 `false` 只是一次原子交换，不取锁、不做 I/O。
     *
     * 那次搬运的失败按本库统一的方式报告：置状态位（`devfailbit` / `cvtfailbit`），
     * `exceptions()` 掩码含该位时才抛出。注意此时**模式已经切换成功**，失败的只是把先前缓冲的
     * 字节交给 stdio 这一步；与同步模式的插入一样，stdout 全缓冲时设备写失败要到 stdio 自己
     * 冲刷才暴露，这里看不到。已处于失败态的流不搬，也就不会因此多一个位。
     *
     * 查询当前状态请用 `synced_with_stdio()`：本函数的无参形式等于 `sync_with_stdio(true)`，
     * 会真的切换。
     *
     * @param sync `true` 为同步（默认），`false` 为自行缓冲。
     * @return 调用前的同步状态。
     * @note 切回同步只保证**此刻**已缓冲的字节到达 stdio。若在一次插入进行中（用户的
     *       `io_traits::swrite` 里）切回，该次插入尚未写出的部分仍按哨兵构造时的取值处理，
     *       到下一次插入结束才进入 stdio。
     * @endif
     *
     * @lang{EN}
     * @brief Switches whether this stream is synchronized with C stdio: synchronized
     * pushes this stream's buffer into the stdio buffer at the end of every insertion,
     * keeping the interleaving with `printf` and friends; unsynchronized buffers on its
     * own.
     *
     * Switching from own buffering back to synchronized hands the bytes buffered so far to
     * stdio here: the flag is read by each insertion's output sentry and so only governs the
     * insertions that follow, and without a hand-over at this point those bytes would surface
     * after the next `printf`, or not until the process exits. So switching to `true` takes
     * this stream's `io_mutex()`, flips the flag under it, and if it was `false` moves this
     * stream's buffer into stdio's buffer once -- the sentry's operation, with **no** `fflush`:
     * stdio's buffer may hold `printf`'s bytes as well, and when they land is stdio's call.
     * Flipping under the lock lets every caller that returns from here rely on the bytes
     * buffered before the call being in stdio's hands, whether this call moved them or the
     * one it waited for did. Switching to `false` is a single atomic exchange: no lock, no I/O.
     *
     * A failure of that hand-over is reported the way this library reports every other one: a
     * state bit is set (`devfailbit` / `cvtfailbit`) and it throws only when the
     * `exceptions()` mask includes that bit. Note that the mode **has** switched by then; what
     * failed is only handing the previously buffered bytes to stdio -- and, as with a
     * synchronized insertion, on a fully buffered stdout a device failure shows up only when
     * stdio itself flushes, not here. A stream already in a failed state is left alone, so
     * this cannot add a bit of its own.
     *
     * To ask for the current state use `synced_with_stdio()`: with no argument this one means
     * `sync_with_stdio(true)` and does switch.
     *
     * @param sync `true` for synchronized (the default), `false` for own buffering.
     * @return The synchronization state before the call.
     * @note Switching back only guarantees that the bytes buffered **at that moment** reach
     *       stdio. Switching back from inside an insertion (a user's `io_traits::swrite`)
     *       leaves the rest of that insertion governed by the value its sentry read on
     *       construction, so it reaches stdio at the end of the next insertion.
     * @endif
     */
    bool sync_with_stdio(bool sync = true)
    {
        if (!sync)
            return m_sync_with_stdio.exchange(false);

        // Flag and hand-over under one lock, so a caller that returns can rely on the
        // bytes buffered before the call being in stdio's hands.
        std::lock_guard guard(this->io_mutex());
        if (m_sync_with_stdio.exchange(true))
            return true;

        try
        {
            // The sentry's operation, not the stream-level flush(): no fflush of bytes
            // that are not this stream's.
            if (static_cast<bool>(*this)) m_streambuf.flush();
        }
        catch (...)
        {
            this->handle_exception(std::current_exception());
        }
        return false;
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
        return m_sync_with_stdio.load();
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
     * `new_code` 与 `code()` 相同时什么也不做——比较的是 `code()` 报出的**解析后**的名字与实参
     * 本身，所以 `""` 与别名（当前是 `zh_CN.UTF-8` 时给 `zh_CN.utf8`、当前是 `C` 时给 `POSIX`）
     * 都不会命中早退，会真的重建一次（结果相同，代价是一次 `newlocale`）。只是
     * `adjust(code_cvt_switch)` 的包装，走本流
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
     * @note 置 `cvtfailbit`：该名字不被 `newlocale()` 接受（含内嵌 NUL 的名字按全长拒绝，不在
     *       第一个 NUL 处截断）、编码转换状态不处于初始状态（有状态
     *       编码写出非 ASCII 之后），或已 tainted 转换器的预先恢复无法完成终结。
     *       置 `devfailbit`：转换器已 tainted，且预先恢复时旧设备冲刷失败。详见
     *       `cvt/code_cvt_stdio.h`。
     * @endif
     *
     * @lang{EN}
     * @brief Switches the encoding (locale) this stream uses to encode `wchar_t` into bytes.
     *
     * Does nothing when `new_code` equals `code()` -- the comparison is between the
     * **resolved** name `code()` reports and the argument itself, so `""` and aliases
     * (`zh_CN.utf8` while the current name is `zh_CN.UTF-8`, `POSIX` while it is `C`) miss the
     * early exit and do rebuild once, to the same result, at the cost of one `newlocale`.
     * A thin wrapper over
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
     * @note Sets `cvtfailbit`: the name is not accepted by `newlocale()` (a name with an
     *       embedded NUL is rejected at its full length, not cut at the first NUL), the encoding
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
     *       涉及调用方通过 `printf` 等 C stdio 接口写入的字节，**以及共用同一 `FILE` 的另一个
     *       标准流**（`cout` 与 `wcout` 共用 `stdout`，`cerr` 与 `clog` 共用 `stderr`）交出去
     *       但尚未落盘的字节：那些字节在这里一并被冲刷，失败却只记在本流的状态位上，另一个流
     *       仍是 `good()`。一个 `FILE` 只有一份缓冲，谁冲刷都会波及另一个，`std::cout` /
     *       `std::wcout` 上同样如此。
     *
     * @note 重新附接这一步在标准流上**没有可失败的操作**：装的是同一 fd 的缺省设备，不分配、
     *       不做 I/O、不重建 locale——实测五个标准流在全部分配失败且 fd 指向 `/dev/full` 时
     *       `reset()` 均 0 次分配、状态位全 0。围住它的 `handle_exception` 只是兜底：若将来这里
     *       真抛了什么（`cvtfailbit` / `otherfailbit`），转换器会停在未初始化状态，流不可用，
     *       须再次 `reset()`，`clear()` 不够。
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
     *       `printf`, **and bytes the other standard stream sharing that `FILE`** (`cout` and
     *       `wcout` share `stdout`, `cerr` and `clog` share `stderr`) has handed over but not
     *       yet landed: those are flushed here too, while the failure is recorded only on this
     *       stream's state bits and the other one stays `good()`. One `FILE` has one buffer,
     *       so whoever flushes it reaches the other stream as well -- the same holds for
     *       `std::cout` / `std::wcout`.
     *
     * @note Reattaching has **nothing that can fail** on a standard stream: it installs a
     *       default device on the same fd, allocates nothing, does no I/O and rebuilds no
     *       locale -- measured on all five standard streams with every allocation failing and
     *       the fd on `/dev/full`: zero allocations, no state bit. The `handle_exception`
     *       around it is only a backstop: should something ever throw there (`cvtfailbit` /
     *       `otherfailbit`), the converter is left uninitialized, the stream is unusable, and
     *       another `reset()` is required -- `clear()` is not enough.
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
    copyable_atomic<bool> m_sync_with_stdio{true};   ///< @lang{ZH} 为 true 时每次插入结束（输出哨兵析构）都把本流缓冲推进 stdio 缓冲；与进程退出时的刷新无关。哨兵在构造时读它一次并沿用到析构，故本标志只影响之后**开始**的插入——切回同步时那批已缓冲的字节由 `sync_with_stdio` 自己持锁搬进 stdio 缓冲。原子量，使标志的翻转与 `synced_with_stdio()` 的查询可与并发输出操作安全竞争。 @endif @lang{EN} When true, every insertion (the output sentry's destructor) pushes this stream's buffer into the stdio buffer; unrelated to the flush at process exit. A sentry reads it once on construction and uses that value through its destructor, so the flag governs the insertions that **start** afterwards -- what was already buffered when switching back to synchronized is moved into stdio's buffer by `sync_with_stdio` itself, under the lock. Atomic so that flipping the flag and querying it through `synced_with_stdio()` are safe against concurrent output operations. @endif
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
