// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file std_device.h
 * @lang{ZH}
 * 定义了 `std_device` 类，用于封装标准 I/O 流（stdin, stdout, stderr）。
 * 此设备提供了与标准输入、输出和错误流交互的统一接口。
 * @endif
 *
 * @lang{EN}
 * Defines the `std_device` class for wrapping standard I/O streams (stdin, stdout, stderr).
 * This device provides a unified interface for interacting with standard input, output, and error streams.
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/device/platform/std_device_backend.h>

#include <cstddef>
#include <cstdio>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 封装标准 I/O 文件描述符的设备。
 *
 * 这个类模板通过标准流的文件描述符（0、1、2）
 * 来创建一个 I/O 设备。它为标准输入提供了阻塞读取（fd 设了 `O_NONBLOCK` 时以 `poll` 等待，
 * `EINTR` 自动重试）和 EOF 处理，并为标准输出/错误提供了写入和刷新功能。
 *
 * @note 支持 POSIX（Linux、macOS）与 Windows；平台相关的读操作在 `device/platform/` 下的后端里。
 *       下文的 `O_NONBLOCK`、`poll`、`EINTR` 只适用于 POSIX，Windows 的差异见 Windows 后端。
 * @note 此类不是线程安全的，多线程并发由更高层次的代码处理。
 * @note 输出侧经 `fwrite` 写，**不处理 `EAGAIN`**：fd 设了 `O_NONBLOCK`（终端上它同时作用于
 *       fd 0/1/2）时，写满内核缓冲的那部分返回失败、字节丢失，流置 `devfailbit`。`std::cout`
 *       同样如此（实测）。
 * @note 终端处于 raw 模式且 `VMIN == 0` 时，阻塞 fd 上 `read` 超时返回 0，被当成（粘性的）EOF；
 *       设了 `O_NONBLOCK` 的 fd 则以 `poll(-1)` 等待，不受 `VTIME` 影响。两者行为不同。
 * @note macOS 的 `poll` 不支持终端以外的字符设备（如 `/dev/zero`），报 `POLLNVAL`：stdin 重定向自
 *       这类设备时，探测后的 `dget` 只返回缓存的那 1 个字节；fd 还设了 `O_NONBLOCK` 时，`read`
 *       遇 `EAGAIN` 后的等待抛 `device_error`。终端与管道不受影响（实测）。
 *
 * stdin 有两种读法，由 `stdio_mode()` 切换：缺省的 **fd 模式**直接 `read()` fd 0，绕过 stdio
 * 缓冲；**stdio 模式**在 `stdin` 的 `FILE` 锁内用 `fread` 读，与 C stdio 共用一份缓冲。标准输入
 * 流对象经 `sync_with_stdio()` 切换它：同步时用 stdio 模式。
 *
 * @warning fd 模式下请勿在 stdin 上将 `std_input_device` 与 C stdio 函数（`scanf`, `fgets`,
 * `getchar` 等）混合使用：stdio 预读进 `FILE` 缓冲的字节本设备看不到，反之亦然。
 *
 * @tparam ID 文件描述符。必须是 0、1 或 2（POSIX 上即 `STDIN_FILENO`、`STDOUT_FILENO`、
 *         `STDERR_FILENO`）；请用别名 `std_input_device`、`std_output_device`、`std_error_device`。
 * @endif
 *
 * @lang{EN}
 * @brief A device that encapsulates standard I/O file descriptors.
 *
 * This class template uses a standard stream's file descriptor (0, 1, 2)
 * to create an I/O device. It provides blocking reads (waiting with `poll` when the fd has
 * `O_NONBLOCK`, retrying on `EINTR`) and EOF handling for standard input, and write/flush
 * capabilities for standard output/error.
 *
 * @note Supports POSIX (Linux, macOS) and Windows; the platform-specific reads live in the
 *       backends under `device/platform/`. `O_NONBLOCK`, `poll` and `EINTR` below apply to
 *       POSIX only; see the Windows backend for how Windows differs.
 * @note This class is not thread-safe; multi-threading is handled at a higher level.
 * @note The output side writes with `fwrite` and does **not handle `EAGAIN`**: with
 *       `O_NONBLOCK` on the fd (on a terminal it applies to fds 0/1/2 alike), the part that
 *       would overfill the kernel buffer fails, its bytes are lost and the stream gets
 *       `devfailbit`. `std::cout` does the same (measured).
 * @note With the terminal in raw mode and `VMIN == 0`, a `read` on a blocking fd that times
 *       out returns 0 and is taken as (sticky) EOF, while an fd with `O_NONBLOCK` waits in
 *       `poll(-1)` and ignores `VTIME`. The two behave differently.
 * @note macOS's `poll` does not support character devices other than terminals (such as
 *       `/dev/zero`) and reports `POLLNVAL`: with stdin redirected from one, a `dget` after a
 *       probe returns only the cached byte, and if the fd also has `O_NONBLOCK`, the wait
 *       after an `EAGAIN` throws `device_error`. Terminals and pipes are unaffected (measured).
 *
 * Stdin is read in one of two ways, switched by `stdio_mode()`: the default **fd mode**
 * `read()`s fd 0 directly, bypassing stdio buffering; **stdio mode** reads with `fread` under
 * the lock of `stdin`'s `FILE`, sharing one buffer with C stdio. The standard input stream
 * objects switch it through `sync_with_stdio()`: synchronized means stdio mode.
 *
 * @warning In fd mode, do NOT mix std_input_device with C stdio functions (scanf, fgets,
 * getchar, etc.) on stdin: bytes stdio has read ahead into the `FILE` buffer are invisible to
 * this device, and the other way round.
 *
 * @tparam ID The file descriptor. Must be 0, 1 or 2 (on POSIX, `STDIN_FILENO`, `STDOUT_FILENO`,
 *         `STDERR_FILENO`); use the aliases `std_input_device`, `std_output_device`,
 *         `std_error_device`.
 * @endif
 */
template <int ID>
    requires ((ID == detail::std_in_fd) || (ID == detail::std_out_fd) || (ID == detail::std_err_fd))
class std_device
{
public:
    using char_type = char;

    std_device() = default;
    std_device(const std_device&) = delete;
    std_device& operator=(const std_device&) = delete;

    /**
     * @lang{ZH}
     * @brief 移动构造：接过 `other` 的全部状态，`other` 随之变为惰性。
     *
     * 搬走的除了 stdin 的 EOF 标记、预读字节与读法（`stdio_mode()`），还有「本对象销毁时冲刷该流」这份责任
     * （见 `m_active`）。被移走的对象析构时不再冲刷：一个流只有一份 stdio 缓冲，
     * 冲刷责任必须唯一，否则先死的那个中转副本会抢先做那次可能失败的 `fflush`、
     * 在析构函数里把错误吞掉，真正的持有者随后什么也报告不了。
     * @endif
     *
     * @lang{EN}
     * @brief Move construction: takes over everything `other` holds and leaves it inert.
     *
     * Besides stdin's EOF flag, lookahead byte and read mode (`stdio_mode()`), this hands
     * over the duty to flush the
     * stream when this object dies (see `m_active`). A moved-from object no longer flushes
     * in its destructor: one stream has one stdio buffer, so that duty has to be unique --
     * otherwise whichever intermediate copy dies first takes the one `fflush` that can
     * fail, swallows the error inside its destructor, and the real holder is left with
     * nothing to report.
     * @endif
     */
    std_device(std_device&& other) noexcept
        : m_active(std::exchange(other.m_active, false))
    {
        if constexpr (ID == detail::std_in_fd)
        {
            m_eof_hit = other.m_eof_hit;
            m_c = other.m_c;
            m_use_stdio = other.m_use_stdio;
            other.m_eof_hit = false;
            other.m_c.reset();
            other.m_use_stdio = false;
        }
    }

    /**
     * @lang{ZH}
     * @brief 移动赋值：同移动构造，本对象的全部状态换成 `other` 的，`other` 变为惰性。
     *
     * 纯转交，不做任何 I/O：本对象原先持有的冲刷责任随其它状态一并被覆盖，这里不会
     * 为它补一次冲刷——两个对象包的是同一个流，字节仍在 stdio 缓冲里，由下一次冲刷或
     * 进程退出送达；能报告失败的冲刷应由调用方在替换设备前显式 `dflush()`。
     * 自我移动不改变状态。
     * @endif
     *
     * @lang{EN}
     * @brief Move assignment: as move construction; this object's whole state becomes
     * `other`'s and `other` is left inert.
     *
     * A pure hand-over with no I/O: a flush duty this object held is overwritten along with
     * the rest of its state, and no flush is made up for it here -- both objects wrap the
     * same stream, the bytes are still in the stdio buffer, and the next flush or process
     * exit delivers them; a flush whose failure should be reported belongs to the caller,
     * as an explicit `dflush()` before the device is replaced. A self-move changes nothing.
     * @endif
     */
    std_device& operator=(std_device&& other) noexcept
    {
        if (this != &other)
        {
            m_active = std::exchange(other.m_active, false);
            if constexpr (ID == detail::std_in_fd)
            {
                m_eof_hit = other.m_eof_hit;
                other.m_eof_hit = false;
                m_c = other.m_c;
                other.m_c.reset();
                m_use_stdio = other.m_use_stdio;
                other.m_use_stdio = false;
            }
        }
        return *this;
    }

    /**
     * @lang{ZH}
     * @brief 析构函数：仍持有冲刷责任（未被移走）的标准输出 / 标准错误设备在销毁时刷新
     * 对应的流。冲刷失败被吞掉——析构函数无法报告；要拿到失败请在销毁前显式
     * `dflush()`。
     * @endif
     *
     * @lang{EN}
     * @brief Destructor: a standard output / standard error device that still holds the
     * flush duty (has not been moved from) flushes its stream on destruction. A flush
     * failure is swallowed -- a destructor cannot report it; call `dflush()` explicitly
     * before destruction to see it.
     * @endif
     */
    ~std_device()
    {
        if constexpr ((ID == detail::std_out_fd) || (ID == detail::std_err_fd))
        {
            if (!m_active) return;
            try {
                dflush();
            } catch (...) { // NOLINT(bugprone-empty-catch)
                // Ignore exceptions in destructor to prevent std::terminate
            }
        }
    }

    /**
     * @lang{ZH}
     * @brief 切换 stdin 的读法：`true` 为 stdio 模式（经 `stdin` 的 `FILE`），`false` 为 fd 模式
     * （直接 `read()` fd 0，缺省）。
     *
     * stdio 模式下 `dget` 像 `fread` 一样要读满 `n` 个字节或遇到 EOF 才返回，所以调用方只应向它要
     * 确实需要的字节数（`stdin_root_cvt` 同步时正是如此）；EOF 以 `FILE` 自己的粘性标志为准，
     * `clearerr(stdin)` 之后可以接着读。
     *
     * 切换本身不做 I/O。已缓存的预读字节（fd 模式下 `deof()` 探到的）不论哪种模式都由下一次
     * `dget()` 先交出；fd 模式记下的 EOF 在切到 stdio 模式时清除。切回 fd 模式时，`FILE` 缓冲里
     * 尚未读走的字节（包括 stdio 模式下 `deof()` 经 `ungetc` 退回的那个）本设备不再看得到——
     * 没有可移植的办法把它们取出来。
     * @endif
     *
     * @lang{EN}
     * @brief Switches how stdin is read: `true` for stdio mode (through `stdin`'s `FILE`),
     * `false` for fd mode (`read()` on fd 0 directly, the default).
     *
     * In stdio mode `dget`, like `fread`, returns only once it has read all `n` bytes or met
     * EOF, so a caller should ask it only for bytes it does need (as `stdin_root_cvt` does when
     * synchronized); EOF is the `FILE`'s own sticky flag, and reading can go on after a
     * `clearerr(stdin)`.
     *
     * The switch does no I/O. A cached lookahead byte (probed by `deof()` in fd mode) is handed
     * out first by the next `dget()` in either mode; an EOF recorded in fd mode is cleared on
     * switching to stdio mode. After switching back to fd mode, bytes not yet read from the
     * `FILE` buffer (the one `deof()` pushed back with `ungetc` in stdio mode included) are out
     * of this device's sight -- there is no portable way to take them out.
     * @endif
     */
    void stdio_mode(bool on) noexcept
        requires (ID == detail::std_in_fd)
    {
        if (on && !m_use_stdio)
            m_eof_hit = false;
        m_use_stdio = on;
    }

    /**
     * @lang{ZH}
     * @brief 查询 stdin 当前是否为 stdio 模式。
     * @endif
     *
     * @lang{EN}
     * @brief Tells whether stdin is currently in stdio mode.
     * @endif
     */
    [[nodiscard]] bool stdio_mode() const noexcept
        requires (ID == detail::std_in_fd)
    {
        return m_use_stdio;
    }

    /**
     * @lang{ZH}
     * @brief 检查标准输入流是否已到达文件末尾（EOF）。
     *
     * 采用**探测式**判断：若尚未确定 EOF、且内部没有已缓存的预读字节，会尝试从设备
     * 读取 1 个字节来确定末尾状态：
     * - 读到字节：说明尚未到 EOF；该字节被缓存在内部，并由**下一次 `dget()` 首先返回**
     *   （连同此刻已就绪的数据，但不为凑数等待），不会丢失、不会打乱顺序；本函数返回 `false`。
     * - 读到 0（EOF：文件读完、管道写端全部关闭、终端上一行开头的 `^D`）：置位 EOF 标志并
     *   返回 `true`。
     *
     * 终端**挂断**（pty 主端关闭）在 Linux 上不算 EOF：`read` 得 `EIO`、`poll` 报 `POLLERR`，按设备错误
     * 抛 `device_error`，流上是 `devfailbit`（`std::cin` 此时报 eof|fail）；阻塞 fd 上偶尔也会
     * 先读到 0 而得 EOF，取决于内核时序（实测）。macOS 上挂断即 EOF：`poll` 报 `POLLIN|POLLHUP`，
     * `read` 返回 0（实测）。
     *
     * @warning 本函数**可能阻塞**：当流上暂时无数据、也尚未到达 EOF 时，它会一直等待，
     *          直到有字节到达或确认 EOF 为止。因此在交互式终端上调用会等待用户输入。
     * @note EOF 是**粘性**的：一旦探测到 EOF，后续调用立即返回 `true`（不再读取）。
     * @note 上面描述的是 fd 模式。stdio 模式下（见 `stdio_mode()`）在 `FILE` 锁内读 1 个字节再
     *       `ungetc` 退回，字节留在 `FILE` 里，C stdio 函数接着也能读到；EOF 即 `feof(stdin)`，
     *       `clearerr(stdin)` 可以清除。
     * @return 如果已到达 EOF，则为 `true`。
     * @throw device_error 如果底层读取或轮询发生错误。
     * @endif
     *
     * @lang{EN}
     * @brief Checks whether the standard input stream has reached end-of-file (EOF).
     *
     * Uses a **probing** strategy: if EOF is not yet known and no look-ahead byte is
     * currently cached, it attempts to read 1 byte from the device to determine the
     * end state:
     * - A byte is read: not at EOF yet; the byte is cached internally and will be
     *   **returned first by the next `dget()`** (along with whatever is ready by then, but
     *   without waiting to fill up), so it is neither lost nor reordered; returns `false`.
     * - 0 is read (EOF: a file read to its end, every write end of a pipe closed, `^D` at the
     *   start of a terminal line): sets the EOF flag and returns `true`.
     *
     * On Linux a terminal **hang-up** (the pty master closed) is not EOF: `read` gets `EIO` and
     * `poll` reports `POLLERR`, which is thrown as `device_error` and shows as `devfailbit` on
     * the stream (`std::cin` reports eof|fail there); on a blocking fd a 0 is occasionally read
     * first and gives EOF, depending on kernel timing (measured). On macOS a hang-up is EOF:
     * `poll` reports `POLLIN|POLLHUP` and `read` returns 0 (measured).
     *
     * @warning This function **may block**: when no data is currently available and
     *          EOF has not yet been reached, it waits until a byte arrives or EOF is
     *          confirmed. On an interactive terminal it therefore waits for user input.
     * @note EOF is **sticky**: once EOF is detected, subsequent calls return `true`
     *       immediately without reading further.
     * @note The above describes fd mode. In stdio mode (see `stdio_mode()`) one byte is read
     *       under the `FILE` lock and pushed back with `ungetc`, so it stays in the `FILE` for
     *       C stdio functions to read too; EOF is `feof(stdin)`, which `clearerr(stdin)`
     *       clears.
     * @return `true` if EOF has been reached.
     * @throw device_error If the underlying read or poll fails.
     * @endif
     */
    [[nodiscard]] bool deof()
        requires (ID == detail::std_in_fd)
    {
        return m_use_stdio ? stdio_eof() : fd_eof();
    }

    /**
     * @lang{ZH}
     * @brief 从标准输入读取数据。
     *
     * 这是一个阻塞式读取操作，使用 `poll` 来等待数据可用，并能正确处理 `EINTR` 中断。
     * 若此前 `deof()` 探测时预读并缓存了 1 个字节，本函数会**首先返回该缓存字节**，并把
     * 设备上**此刻已就绪**的数据一并读出（零超时 `poll` 判定），从而保证字节顺序不被打乱、
     * 探针之后的整块读取仍是整块；但**不会**为了凑够 `n` 个字节而在设备暂时无数据时等待
     * ——那种情形下只返回这一个字节（短读），它正是调用方要的东西。
     * @param s 存储数据的缓冲区。
     * @param n 要读取的字节数。
     * @return 实际读取的字节数，可能少于 `n`。如果到达 EOF，则返回 0。
     * @throw device_error 如果发生读取或轮询错误。
     * @note 遇到 EOF 时立即返回 0，符合 POSIX read() 语义。
     *       EOF 是粘性的，后续调用也会继续返回 0。
     * @note 上面描述的是 fd 模式。stdio 模式下（见 `stdio_mode()`）在 `FILE` 锁内 `fread`，
     *       读满 `n` 个字节或遇到 EOF 才返回（缓存的预读字节仍先交出、单独返回）；EOF 即
     *       `feof(stdin)`，`clearerr(stdin)` 之后可以接着读。
     * @endif
     *
     * @lang{EN}
     * @brief Reads data from standard input.
     *
     * This is a blocking read operation that uses `poll` to wait for data to become available
     * and correctly handles `EINTR` interrupts.
     * If a previous `deof()` probe read and cached one look-ahead byte, this function
     * **returns that cached byte first** and reads along with it whatever the device has
     * **ready at that moment** (decided by a zero-timeout `poll`), so byte ordering is
     * preserved and a bulk read after a probe is still a bulk read; but it does **not** wait
     * to fill up `n` bytes while the device has nothing more yet -- in that case it returns
     * that one byte alone (a short read), which is exactly what the caller asked for.
     * @param s The buffer to store the data.
     * @param n The number of bytes to read.
     * @return The number of bytes actually read, possibly fewer than `n`. Returns 0 if EOF is
     *         reached.
     * @throw device_error If a read or poll error occurs.
     * @note Returns 0 immediately upon EOF, conforming to POSIX read() semantics.
     *       EOF is sticky, so subsequent calls also continue to return 0.
     * @note The above describes fd mode. In stdio mode (see `stdio_mode()`) it `fread`s under
     *       the `FILE` lock and returns only once it has all `n` bytes or meets EOF (a cached
     *       lookahead byte still comes first, on its own); EOF is `feof(stdin)`, and reading
     *       can go on after a `clearerr(stdin)`.
     * @endif
     */
    std::size_t dget(char* s, std::size_t n)
        requires (ID == detail::std_in_fd)
    {
        if (n == 0) return 0;
        if (s == nullptr)
            throw device_error("std_device::dget fail: null buffer");
        return m_use_stdio ? stdio_get(s, n) : fd_get(s, n);
    }

    /**
     * @lang{ZH}
     * @brief 把 `s[0..n)` 退回给 C stdio，使下一次读取（本设备的 `dget()`，或 `getchar()` 等）
     * 先按原顺序得到这些字节。
     *
     * 要么全部退回、返回 `true`，要么一个也不退、返回 `false`，调用方据此决定字节是否仍由自己
     * 保管。只有 stdio 模式（见 `stdio_mode()`）可能成功：fd 模式没有与 C stdio 共用的缓冲，
     * 退回没有意义；切换前 fd 模式探到的字节尚未交出时也拒绝，因为退回的字节与它谁先谁后
     * 说不清。stdio 模式下退回经 `ungetc`，在 `stdin` 的 `FILE` 锁内从最后一个字节起逐个进行：
     * POSIX 上标准只保证 1 个字节，多出的部分 glibc 与 BSD 只在内存不足时失败，其它 libc
     * 可能更早失败，半途失败时已退回的字节被重新读走；Windows 上总是返回 `false`——UCRT
     * 没有独立的回退区，空间不够时 `ungetc` 可能改写尚未读的字节而照样报告成功，`FILE`
     * 又不透明、无从事先查询。
     *
     * @note `ungetc` 会清除 `FILE` 的 EOF 标志，而标准没有重新置位它的接口：退回失败时字节都
     *       已撤回，但 EOF 标志可能已被清除。
     * @param s 要退回的字节。
     * @param n 字节数；为 0 时什么也不做，返回 `true`。
     * @return 全部退回为 `true`；没有退回为 `false`。
     * @throw device_error `s` 为空而 `n` 不为 0。
     * @endif
     *
     * @lang{EN}
     * @brief Hands `s[0..n)` back to C stdio, so the next read (this device's `dget()`, or
     * `getchar()` and the like) gets those bytes first, in order.
     *
     * Either every byte goes back and it returns `true`, or none does and it returns
     * `false`, which tells the caller whether the bytes are still its to keep. Only stdio
     * mode (see `stdio_mode()`) can succeed: fd mode shares no buffer with C stdio, so there
     * is nothing to hand back to, and a byte fd mode probed before a switch and not yet
     * handed out makes it refuse too, as which of it and the pushed bytes comes first would
     * be unclear. In stdio mode the bytes go back with `ungetc`, last byte first, under the
     * lock of `stdin`'s `FILE`: on POSIX the standard guarantees one byte, glibc and BSD fail
     * beyond that only when out of memory, other C libraries may fail sooner, and the bytes
     * already pushed are read off again when it fails partway; on Windows it always returns
     * `false` -- the UCRT keeps no separate pushback area, its `ungetc` may overwrite an
     * unread byte and still report success when there is no room, and `FILE` is opaque, so
     * the room cannot be asked for in advance.
     *
     * @note `ungetc` clears the `FILE`'s EOF flag, and the standard has no call to set it
     *       again: after a failed attempt every byte is taken back, but the EOF flag may have
     *       been cleared.
     * @param s The bytes to hand back.
     * @param n The number of bytes; 0 does nothing and returns `true`.
     * @return `true` if every byte went back; `false` if none did.
     * @throw device_error If `s` is null while `n` is not 0.
     * @endif
     */
    bool dunget(const char* s, std::size_t n)
        requires (ID == detail::std_in_fd)
    {
        if (n == 0) return true;
        if (s == nullptr)
            throw device_error("std_device::dunget fail: null buffer");
        // fd mode shares no buffer with C stdio; a byte fd mode probed before a switch
        // must still come first, so the order would be unclear.
        if (!m_use_stdio || m_c.has_value()) return false;
        return detail::std_stdin_file_unget(s, n);
    }

    /**
     * @lang{ZH}
     * @brief 将数据写入标准输出或标准错误。
     * @param ch 要写入的数据。
     * @param n 要写入的字节数。
     * @throw dput_error 写到一半失败：`written()` 是已被 stdio 接收的字节数（`fwrite` 的返回值），
     *        这些字节由 stdio 负责，不必重写。
     * @throw device_error 写之前就失败（`ch` 为空），一个字节也没有接收。
     * @endif
     *
     * @lang{EN}
     * @brief Writes data to standard output or standard error.
     * @param ch The data to write.
     * @param n The number of bytes to write.
     * @throw dput_error If the write fails partway: `written()` is the number of bytes stdio
     *        accepted (what `fwrite` returned); those are stdio's to deliver and need not be
     *        written again.
     * @throw device_error If it fails before writing (`ch` is null), with nothing accepted.
     * @endif
     */
    void dput(const char* ch, std::size_t n)
        requires ((ID == detail::std_out_fd) || (ID == detail::std_err_fd))
    {
        if (n == 0) return;
        if (ch == nullptr)
            throw device_error("std_device::dput fail: null buffer");

        std::size_t written = 0;
        if constexpr (ID == detail::std_out_fd)
            written = std::fwrite(ch, sizeof(char), n, stdout);
        else
            written = std::fwrite(ch, sizeof(char), n, stderr);

        if (written != n)
            throw dput_error("std_device::dput fail: partial write", written);
    }

    /**
     * @lang{ZH}
     * @brief 刷新标准输出或标准错误流。
     * @throw device_error 如果刷新失败。
     * @endif
     *
     * @lang{EN}
     * @brief Flushes the standard output or standard error stream.
     * @throw device_error If flushing fails.
     * @endif
     */
    void dflush()
        requires ((ID == detail::std_out_fd) || (ID == detail::std_err_fd))
    {
        bool flush_res = false;
        if constexpr (ID == detail::std_out_fd)
            flush_res = (std::fflush(stdout) != EOF);
        else
            flush_res = (std::fflush(stderr) != EOF);

        if (!flush_res)
            throw device_error("std_device::dflush fail: fflush error");
    }

private:
    // fd mode keeps its own state: EOF in m_eof_hit, a probed byte in m_c.
    std::size_t fd_get(char* s, std::size_t n)
        requires (ID == detail::std_in_fd)
    {
        if (m_eof_hit) return 0;
        if (m_c.has_value())
        {
            *s = m_c.value();
            if (n > 1 && detail::std_fd_readable_now(ID))
            {
                // Top up with what is already there, so a probe followed by a bulk
                // read still reads in bulk. Only when data is ready: waiting here
                // would block on a pipe whose peer sent that one byte, or on a
                // terminal where the user typed an empty line. If fd_read throws,
                // m_c must still hold the byte so the next dget() redelivers it.
                const std::size_t got = fd_read(s + 1, n - 1);
                m_c.reset();
                return 1 + got;
            }
            m_c.reset();
            return 1;
        }
        return fd_read(s, n);
    }

    bool fd_eof()
        requires (ID == detail::std_in_fd)
    {
        if (m_eof_hit) return true;
        if (m_c.has_value()) return false;

        char_type ch = 0;
        if (fd_read(&ch, 1) == 0) return true;      // fd_read has set m_eof_hit
        m_c = ch;
        return false;
    }

    std::size_t fd_read(char* s, std::size_t n)
        requires (ID == detail::std_in_fd)
    {
        const std::size_t ret = detail::std_fd_read(ID, s, n);
        if (ret == 0)
            m_eof_hit = true;
        return ret;
    }

    // stdio mode keeps its state in the FILE: a probed byte goes back with ungetc,
    // EOF is feof(stdin). m_c here can only be a byte fd mode probed before the
    // switch; it is still due first, and alone, as fd readiness says nothing now.
    std::size_t stdio_get(char* s, std::size_t n)
        requires (ID == detail::std_in_fd)
    {
        if (m_c.has_value())
        {
            *s = m_c.value();
            m_c.reset();
            return 1;
        }
        return detail::std_stdin_file_read(s, n);
    }

    bool stdio_eof()
        requires (ID == detail::std_in_fd)
    {
        if (m_c.has_value()) return false;
        return detail::std_stdin_file_eof();
    }
private:
    IOV2_NO_UNIQUE_ADDRESS std::conditional_t<ID == detail::std_in_fd, bool, std::monostate> m_eof_hit{};
    IOV2_NO_UNIQUE_ADDRESS std::conditional_t<ID == detail::std_in_fd, std::optional<char_type>, std::monostate> m_c;
    IOV2_NO_UNIQUE_ADDRESS std::conditional_t<ID == detail::std_in_fd, bool, std::monostate> m_use_stdio{}; ///< @lang{ZH} stdin 是否为 stdio 模式（见 `stdio_mode()`）。 @endif @lang{EN} Whether stdin is in stdio mode (see `stdio_mode()`). @endif
    bool m_active = true;   ///< @lang{ZH} 本对象是否仍持有该流的收尾冲刷责任：缺省构造为 true，随移动转交给目标、源对象变 false。析构函数只在为 true 时冲刷（fd 1 / 2）；stdin 上只是状态标记。 @endif @lang{EN} Whether this object still holds the duty to flush the stream when it dies: true on default construction, handed to the target by a move, false on the moved-from object. The destructor flushes (fd 1 / 2) only while it is true; on stdin it is a state marker only. @endif
};

/**
 * @lang{ZH}
 * @brief 标准输入设备的类型别名。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for the standard input device.
 * @endif
 */
using std_input_device = std_device<detail::std_in_fd>;

/**
 * @lang{ZH}
 * @brief 标准输出设备的类型别名。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for the standard output device.
 * @endif
 */
using std_output_device = std_device<detail::std_out_fd>;

/**
 * @lang{ZH}
 * @brief 标准错误设备的类型别名。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for the standard error device.
 * @endif
 */
using std_error_device = std_device<detail::std_err_fd>;

}
