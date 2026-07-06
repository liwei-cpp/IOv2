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
#include <common/defs.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <optional>
#include <type_traits>
#include <variant>

#include <poll.h>
#include <unistd.h>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 封装标准 I/O 文件描述符的设备。
 *
 * 这个类模板通过文件描述符（`STDIN_FILENO`, `STDOUT_FILENO`, `STDERR_FILENO`）
 * 来创建一个 I/O 设备。它为标准输入提供了非阻塞读取和 EOF 处理，
 * 并为标准输出/错误提供了写入和刷新功能。
 *
 * @note 目前仅支持 Linux 系统。
 * @note 此类不是线程安全的，多线程并发由更高层次的代码处理。
 *
 * @warning 对于 stdin，此类使用底层的 POSIX `read()`，它会绕过 stdio 缓冲。
 * 请勿在 stdin 上将 `std_input_device` 与 C stdio 函数（`scanf`, `fgets`, `getchar` 等）
 * 混合使用，因为由于缓冲不一致，这可能会导致数据丢失或意外行为。
 *
 * @tparam ID 文件描述符。必须是 `STDIN_FILENO`、`STDOUT_FILENO` 或 `STDERR_FILENO` 之一。
 * @endif
 *
 * @lang{EN}
 * @brief A device that encapsulates standard I/O file descriptors.
 *
 * This class template uses a file descriptor (`STDIN_FILENO`, `STDOUT_FILENO`, `STDERR_FILENO`)
 * to create an I/O device. It provides non-blocking reads and EOF handling for standard input,
 * and write/flush capabilities for standard output/error.
 *
 * @note Currently, only Linux is supported.
 * @note This class is not thread-safe; multi-threading is handled at a higher level.
 *
 * @warning For stdin, this class uses low-level POSIX read() which bypasses
 * stdio buffering. Do NOT mix usage of std_input_device with C stdio functions
 * (scanf, fgets, getchar, etc.) on stdin, as this may cause data loss or
 * unexpected behavior due to buffering inconsistencies.
 *
 * @tparam ID The file descriptor. Must be one of `STDIN_FILENO`, `STDOUT_FILENO`, or `STDERR_FILENO`.
 * @endif
 */
template <int ID>
    requires ((ID == STDIN_FILENO) || (ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
class std_device
{
public:
    using char_type = char;

    std_device() = default;
    std_device(const std_device&) = delete;
    std_device& operator=(const std_device&) = delete;
    std_device(std_device&& other) noexcept
    {
        if constexpr (ID == STDIN_FILENO)
        {
            m_eof_hit = other.m_eof_hit;
            m_c = other.m_c;
            other.m_eof_hit = false;
            other.m_c.reset();
        }
    }
    std_device& operator=(std_device&& other) noexcept
    {
        if constexpr (ID == STDIN_FILENO)
        {
            if (this != &other)
            {
                m_eof_hit = other.m_eof_hit;
                other.m_eof_hit = false;
                m_c = other.m_c;
                other.m_c.reset();
            }
        }
        return *this;
    }

    /**
     * @lang{ZH}
     * @brief 析构函数，在销毁时刷新标准输出或标准错误流。
     * @endif
     *
     * @lang{EN}
     * @brief Destructor that flushes standard output or standard error upon destruction.
     * @endif
     */
    ~std_device()
    {
        if constexpr ((ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
        {
            try {
                dflush();
            } catch (...) { // NOLINT(bugprone-empty-catch)
                // Ignore exceptions in destructor to prevent std::terminate
            }
        }
    }

    /**
     * @lang{ZH}
     * @brief 检查标准输入流是否已到达文件末尾（EOF）。
     *
     * 采用**探测式**判断：若尚未确定 EOF、且内部没有已缓存的预读字节，会尝试从设备
     * 读取 1 个字节来确定末尾状态：
     * - 读到字节：说明尚未到 EOF；该字节被缓存在内部，并由**下一次 `dget()` 首先返回**，
     *   不会丢失、不会打乱顺序；本函数返回 `false`。
     * - 读到 0（EOF 或挂断）：置位 EOF 标志并返回 `true`。
     *
     * @warning 本函数**可能阻塞**：当流上暂时无数据、也尚未到达 EOF 时，它会一直等待，
     *          直到有字节到达或确认 EOF 为止。因此在交互式终端上调用会等待用户输入。
     * @note EOF 是**粘性**的：一旦探测到 EOF，后续调用立即返回 `true`（不再读取）。
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
     *   **returned first by the next `dget()`**, so it is neither lost nor reordered;
     *   returns `false`.
     * - 0 is read (EOF or hang-up): sets the EOF flag and returns `true`.
     *
     * @warning This function **may block**: when no data is currently available and
     *          EOF has not yet been reached, it waits until a byte arrives or EOF is
     *          confirmed. On an interactive terminal it therefore waits for user input.
     * @note EOF is **sticky**: once EOF is detected, subsequent calls return `true`
     *       immediately without reading further.
     * @return `true` if EOF has been reached.
     * @throw device_error If the underlying read or poll fails.
     * @endif
     */
    [[nodiscard]] bool deof()
        requires (ID == STDIN_FILENO)
    {
        if (m_eof_hit) return true;

        if (m_c.has_value()) return false;
        char_type ch = 0;
        const size_t ret = do_get(&ch, 1);
        if (ret != 0)
        {
            m_c = ch;
            return false;
        }
        return m_eof_hit;
    }

    /**
     * @lang{ZH}
     * @brief 从标准输入读取数据。
     *
     * 这是一个阻塞式读取操作，使用 `poll` 来等待数据可用，并能正确处理 `EINTR` 中断。
     * 若此前 `deof()` 探测时预读并缓存了 1 个字节，本函数会**首先返回该缓存字节**，
     * 再从设备读取其余数据，从而保证字节顺序不被打乱。
     * @param s 存储数据的缓冲区。
     * @param n 要读取的字节数。
     * @return 实际读取的字节数。如果到达 EOF，则返回 0。
     * @throw device_error 如果发生读取或轮询错误。
     * @note 遇到 EOF 时立即返回 0，符合 POSIX read() 语义。
     *       EOF 是粘性的，后续调用也会继续返回 0。
     * @endif
     *
     * @lang{EN}
     * @brief Reads data from standard input.
     *
     * This is a blocking read operation that uses `poll` to wait for data to become available
     * and correctly handles `EINTR` interrupts.
     * If a previous `deof()` probe read and cached one look-ahead byte, this function
     * **returns that cached byte first** and then reads the remainder from the device,
     * so byte ordering is preserved.
     * @param s The buffer to store the data.
     * @param n The number of bytes to read.
     * @return The number of bytes actually read. Returns 0 if EOF is reached.
     * @throw device_error If a read or poll error occurs.
     * @note Returns 0 immediately upon EOF, conforming to POSIX read() semantics.
     *       EOF is sticky, so subsequent calls also continue to return 0.
     * @endif
     */
    size_t dget(char* s, size_t n)
        requires (ID == STDIN_FILENO)
    {
        if (n == 0 || m_eof_hit) return 0;
        if (s == nullptr)
            throw device_error("std_device::dget fail: null buffer");

        if (m_c.has_value())
        {
            *s = m_c.value();
            if (n == 1)
            {
                m_c.reset();
                return 1;
            }
            // Read the remainder first; if do_get throws, m_c must still hold
            // the look-ahead byte so the next dget() redelivers it (no loss).
            const size_t got = do_get(s + 1, n - 1);
            m_c.reset();
            return 1 + got;
        }
        return do_get(s, n);
    }

    /**
     * @lang{ZH}
     * @brief 将数据写入标准输出或标准错误。
     * @param ch 要写入的数据。
     * @param n 要写入的字节数。
     * @throw device_error 如果写入失败。
     * @endif
     *
     * @lang{EN}
     * @brief Writes data to standard output or standard error.
     * @param ch The data to write.
     * @param n The number of bytes to write.
     * @throw device_error If the write fails.
     * @endif
     */
    void dput(const char* ch, size_t n)
        requires ((ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
    {
        if (n == 0) return;
        if (ch == nullptr)
            throw device_error("std_device::dput fail: null buffer");

        bool put_res = false;
        if constexpr (ID == STDOUT_FILENO)
            put_res = (std::fwrite(ch, sizeof(char), n, stdout) == n);
        else
            put_res = (std::fwrite(ch, sizeof(char), n, stderr) == n);

        if (!put_res)
        {
            std::clearerr(ID == STDOUT_FILENO ? stdout : stderr);
            throw device_error("std_device::dput fail: partial write");
        }
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
        requires ((ID == STDOUT_FILENO) || (ID == STDERR_FILENO))
    {
        bool flush_res = false;
        if constexpr (ID == STDOUT_FILENO)
            flush_res = (std::fflush(stdout) != EOF);
        else
            flush_res = (std::fflush(stderr) != EOF);

        if (!flush_res)
        {
            std::clearerr(ID == STDOUT_FILENO ? stdout : stderr);
            throw device_error("std_device::dflush fail: fflush error");
        }
    }

private:
    size_t do_get(char* s, size_t n)
    {
        assert((s != nullptr) && (n != 0));

        constexpr auto max_read = static_cast<size_t>(std::numeric_limits<ssize_t>::max());
        ssize_t ret = 0;
        while (true)
        {
            ret = read(ID, s, std::min(n, max_read));
            if (ret != -1) break;

            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                struct pollfd pfd{ .fd = ID, .events = POLLIN, .revents = 0 };
                if (poll(&pfd, 1, -1) == -1)
                {
                    if (errno == EINTR) continue;
                    throw device_error("std_device::do_get fail: poll error");
                }
                else if (pfd.revents & (POLLERR | POLLNVAL))
                    throw device_error("std_device::do_get fail: poll revents error");
                else if (pfd.revents & POLLIN) continue;
                else if (pfd.revents & POLLHUP)
                {
                    ret = 0;
                    break;
                }
                else
                    throw device_error("std_device::do_get fail: unexpected poll revents");
            }
            else
                throw device_error("std_device::do_get fail: read error");
        }

        if (ret == 0)
            m_eof_hit = true;
        return static_cast<size_t>(ret);
    }
private:
    [[no_unique_address]] std::conditional_t<ID == STDIN_FILENO, bool, std::monostate> m_eof_hit{};
    [[no_unique_address]] std::conditional_t<ID == STDIN_FILENO, std::optional<char_type>, std::monostate> m_c;
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
using std_input_device = std_device<STDIN_FILENO>;

/**
 * @lang{ZH}
 * @brief 标准输出设备的类型别名。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for the standard output device.
 * @endif
 */
using std_output_device = std_device<STDOUT_FILENO>;

/**
 * @lang{ZH}
 * @brief 标准错误设备的类型别名。
 * @endif
 *
 * @lang{EN}
 * @brief Type alias for the standard error device.
 * @endif
 */
using std_error_device = std_device<STDERR_FILENO>;

}
