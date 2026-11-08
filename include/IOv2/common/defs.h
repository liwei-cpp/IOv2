// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file defs.h
 * @lang{ZH}
 * 公共定义：各层共用的异常类型，以及 `IOV2_NO_UNIQUE_ADDRESS`、`SIMD_PADDING_BYTES`。
 * @endif
 * @lang{EN}
 * Common definitions: the exception types every layer uses, plus `IOV2_NO_UNIQUE_ADDRESS`
 * and `SIMD_PADDING_BYTES`.
 * @endif
 */

#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>

/**
 * @lang{ZH}
 * `[[no_unique_address]]` 的可移植写法。MSVC ABI（`cl.exe` 与 clang-cl）为保持二进制兼容，
 * 接受但忽略标准写法，只认 `[[msvc::no_unique_address]]`。
 * @endif
 *
 * @lang{EN}
 * Portable spelling of `[[no_unique_address]]`. To stay binary compatible, the MSVC ABI
 * (`cl.exe` and clang-cl) accepts but ignores the standard spelling and honours only
 * `[[msvc::no_unique_address]]`.
 * @endif
 */
#if defined(_MSC_VER)
#define IOV2_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define IOV2_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace IOv2
{
    /**
     * @lang{ZH}
     * 传给 glibc 字符串函数的缓冲在结尾空字符之后补上的字节数。glibc 的 AVX2 版 `wcscoll`、
     * `wcsxfrm` 等一次读 32 字节，短字符串也照读不误；缓冲后面不留这么多，Valgrind 就会误报
     * "Invalid read of size 32"。
     * @endif
     *
     * @lang{EN}
     * Bytes of padding after the terminating null in buffers handed to glibc's string
     * functions. Their AVX2 versions (`wcscoll`, `wcsxfrm`, ...) read 32 bytes at a time even
     * for short strings, and without this much room behind the data Valgrind reports a false
     * "Invalid read of size 32".
     * @endif
     */
    inline constexpr std::size_t SIMD_PADDING_BYTES = 32;

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
     * 设备层的错误。
     * @endif
     *
     * @lang{EN}
     * Errors from the device layer.
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
     * `dput()` 失败有两种报法。抛本类，表示前 `written()` 个字符已被下层接收，设备不再对它们
     * 负责；抛普通的 `device_error`，表示一个字符也没有被接收。说不清接收了多少的设备只能
     * 二选一，并承担相应后果：报多了，未写出的那部分会丢；报少了，已写出的那部分会重复。
     * @endif
     *
     * @lang{EN}
     * Thrown when a device's `dput()` fails partway: carries the number of characters the layer
     * below had accepted before the failure.
     *
     * A failing `dput()` reports in one of two ways. Throwing this class says the first
     * `written()` characters were accepted below and the device is no longer responsible for
     * them; throwing a plain `device_error` says none was accepted. A device that cannot tell
     * how much was accepted has to pick one and live with the outcome: reporting too many
     * loses what was not written, reporting too few repeats what was.
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
     * 转换器（编码转换、压缩、加密等）的错误。
     * @endif
     *
     * @lang{EN}
     * Errors from a converter (code conversion, compression, encryption, ...).
     * @endif
     */
    struct cvt_error : io_error
    {
        using io_error::io_error;
    };

    /**
     * @lang{ZH}
     * 已读到输入的末尾。名字沿用 C 与 C++ 标准库的 EOF（end of file）。
     * @endif
     *
     * @lang{EN}
     * The end of the input has been reached. The name follows EOF (end of file) in the C and
     * C++ standard libraries.
     * @endif
     */
    struct eof_error : io_error
    {
        eof_error() : io_error("end of file") {}
        using io_error::io_error;
    };

    /**
     * @lang{ZH}
     * 流操作的错误。
     * @endif
     *
     * @lang{EN}
     * Errors from stream operations.
     * @endif
     */
    struct stream_error : io_error
    {
        using io_error::io_error;
    };
}
