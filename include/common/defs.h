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
    constexpr static size_t SIMD_PADDING_BYTES = 32;

    /**
     * @lang{ZH}
     * 设备相关错误的异常类。
     * @endif
     *
     * @lang{EN}
     * Exception class for device-related errors.
     * @endif
     */
    struct device_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
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
    struct cvt_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    /**
     * @lang{ZH}
     * 文件末尾（EOF）异常类。
     * @endif
     *
     * @lang{EN}
     * Exception class for End-Of-File (EOF).
     * @endif
     */
    struct eof_error : std::exception
    {
        using std::exception::exception;
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
    struct stream_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
}
