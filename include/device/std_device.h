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
#include <device/device_concepts.h>

#include <cstdio>
#include <cerrno>
#include <limits>
#include <poll.h>
#include <stdexcept>
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
    std_device(std_device&&) noexcept = default;
    std_device& operator=(std_device&&) noexcept = default;

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
     * @return 如果已触发 EOF，则为 `true`。
     * @endif
     *
     * @lang{EN}
     * @brief Checks if the standard input stream has reached the end-of-file (EOF).
     * @return `true` if EOF has been triggered.
     * @endif
     */
    [[nodiscard]] bool deof() const
        requires (ID == STDIN_FILENO)
    {
        return m_eof_hit;
    }

    /**
     * @lang{ZH}
     * @brief 从标准输入读取数据。
     *
     * 这是一个阻塞式读取操作，使用 `poll` 来等待数据可用，并能正确处理 `EINTR` 中断。
     * @param s 存储数据的缓冲区。
     * @param n 要读取的字节数。
     * @return 实际读取的字节数。如果到达 EOF，则返回 0。
     * @throw device_error 如果发生读取或轮询错误。
     * @endif
     *
     * @lang{EN}
     * @brief Reads data from standard input.
     *
     * This is a blocking read operation that uses `poll` to wait for data to become available
     * and correctly handles `EINTR` interrupts.
     * @param s The buffer to store the data.
     * @param n The number of bytes to read.
     * @return The number of bytes actually read. Returns 0 if EOF is reached.
     * @throw device_error If a read or poll error occurs.
     * @endif
     */
    size_t dget(char* s, size_t n)
        requires (ID == STDIN_FILENO)
    {
        if (n == 0 || m_eof_hit) return 0;
        if (s == nullptr && n > 0)
            throw device_error("std_device::dget fail: null buffer");

        constexpr size_t max_read = static_cast<size_t>(std::numeric_limits<ssize_t>::max());
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
                    throw device_error("std_device::dget fail: poll error");
                }
                if (pfd.revents & (POLLERR | POLLNVAL))
                    throw device_error("std_device::dget fail: poll revents error");
                // POLLHUP is intentionally not treated as EOF: the peer may have
                // closed while bytes remain buffered. Re-issue read(); it returns 0
                // only when both the buffer is drained and the peer is gone.
                continue;
            }
            throw device_error("std_device::dget fail: read error");
        }

        if (ret == 0)
            m_eof_hit = true;

        return static_cast<size_t>(ret);
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
        if (ch == nullptr && n > 0)
            throw device_error("std_device::dput fail: null buffer");

        bool put_res = false;
        if constexpr (ID == STDOUT_FILENO)
            put_res = (std::fwrite(ch, sizeof(char), n, stdout) == n);
        else
            put_res = (std::fwrite(ch, sizeof(char), n, stderr) == n);

        if (!put_res)
            throw device_error("std_device::dput fail: partial success.");
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
            throw device_error("std_device::dflush fail: fflush failed.");
    }

private:
    bool m_eof_hit = false;
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

}
