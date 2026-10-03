// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file defs.h
 * @lang{ZH}
 * 公共定义文件。
 * @endif
 * @lang{EN}
 * Common definitions file.
 * @endif
 */

#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>

namespace IOv2
{
    /**
     * @lang{ZH}
     * glibc 的 AVX2 优化字符串函数（例如 wcsxfrm, wcscoll）使用 SIMD 指令，
     * 即使在处理较小的字符串时也会一次读取 32 字节。
     * 当分配的缓冲区小于 32 字节时，这会导致 Valgrind 报告 "Invalid read of size 32"。
     * 为了避免这些误报，我们添加了填充，以确保传递给这些函数的缓冲区至少比实际数据大 32 字节。
     * @endif
     *
     * @lang{EN}
     * glibc's AVX2-optimized string functions (e.g., wcsxfrm, wcscoll) use SIMD instructions
     * that read 32 bytes at a time, even when processing smaller strings. This can cause
     * Valgrind to report "Invalid read of size 32" when the allocated buffer is smaller than
     * 32 bytes. To avoid these false positives, we add padding to ensure buffers passed to
     * these functions are at least 32 bytes larger than the actual data.
     * @endif
     */
    static constexpr std::size_t SIMD_PADDING_BYTES = 32;

    /**
     * @lang{ZH}
     * IO 相关异常的基类。
     * @endif
     *
     * @lang{EN}
     * Base class for IO-related exceptions.
     * @endif
     */
    struct io_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    /**
     * @lang{ZH}
     * 设备相关错误的异常类。
     * @endif
     *
     * @lang{EN}
     * Exception class for device-related errors.
     * @endif
     */
    struct device_error : io_error
    {
        using io_error::io_error;
    };

    /**
     * @lang{ZH}
     * 设备的 `dput()` 写到一半失败时抛出的异常：带出失败前已被下层接收的字符个数。
     *
     * `dput()` 失败有两种报法。抛本类，表示前 `written()` 个字符已被下层接收，设备不再
     * 对它们负责；抛普通的 `device_error`，表示一个字符也没有被接收。`root_cvt` 据此只
     * 保留未被接收的部分，恢复后的冲刷恰好把它们写出一次。说不清接收了多少的设备只能二选一，
     * 并承担相应后果：报多了，未写出的那部分会丢；报少了，已写出的那部分会重复。
     * @endif
     *
     * @lang{EN}
     * Thrown when a device's `dput()` fails partway: carries the number of characters the
     * layer below had accepted before the failure.
     *
     * A failing `dput()` reports in one of two ways. Throwing this class says the first
     * `written()` characters were accepted below and the device is no longer responsible
     * for them; throwing a plain `device_error` says none was accepted. `root_cvt` keeps only
     * what was not accepted, so the flush after recovery writes it out exactly once. A device
     * that cannot tell how much was accepted has to pick one and live with the outcome:
     * reporting too many loses what was not written, reporting too few repeats what was.
     * @endif
     */
    struct dput_error : device_error
    {
        dput_error(const std::string& what, std::size_t written)
            : device_error(what), m_written(written) {}

        [[nodiscard]] std::size_t written() const noexcept { return m_written; }

    private:
        std::size_t m_written;
    };

    /**
     * @lang{ZH}
     * 转换过程（如编码转换、压缩、加密）中错误的异常类。
     * @endif
     *
     * @lang{EN}
     * Exception class for errors during conversion processes (e.g., code conversion, compression, encryption).
     * @endif
     */
    struct cvt_error : io_error
    {
        using io_error::io_error;
    };

    /**
     * @lang{ZH}
     * 输入流结束（EOF）异常类。
     * 表示已到达输入流的末尾。为了与现有 C++ 标准语义保持一致，采用了 EOF (End-Of-File) 这一名称。
     * @endif
     *
     * @lang{EN}
     * Exception class for End-Of-File (EOF).
     * This class indicates that the end of the input stream has been reached.
     * The name EOF is used for consistency with existing C++ standards.
     * @endif
     */
    struct eof_error : io_error
    {
        eof_error() : io_error("end of file") {}
        using io_error::io_error;
    };

    /**
     * @lang{ZH}
     * 流操作错误的异常类。
     * @endif
     *
     * @lang{EN}
     * Exception class for stream operation errors.
     * @endif
     */
    struct stream_error : io_error
    {
        using io_error::io_error;
    };
}
