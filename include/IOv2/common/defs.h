// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file defs.h
 * @lang{ZH}
 * 公共定义：各层共用的异常类型，以及 `IOV2_NO_UNIQUE_ADDRESS`、`IOV2_HAS_CHRONO_TZDB`、
 * `SIMD_PADDING_BYTES`。
 * @endif
 * @lang{EN}
 * Common definitions: the exception types every layer uses, plus `IOV2_NO_UNIQUE_ADDRESS`,
 * `IOV2_HAS_CHRONO_TZDB` and `SIMD_PADDING_BYTES`.
 * @endif
 */

#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <version>

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

/**
 * @lang{ZH}
 * 标准库的 `<chrono>` 有没有时区数据库（`std::chrono::tzdb`、`time_zone`、`zoned_time`、
 * `locate_zone` 等）：有为 `1`，没有为 `0`，总有定义，可直接用于 `#if`。
 *
 * 为 `0` 时，`timeio` 去掉以 `time_zone` 或 `zoned_time` 为参数的接口；`%Z` 只认得
 * `base_ft<timeio>::s_unknown_zone` 与 `UTC`（`sys_time` 的 `%Z` 写的就是它）；`std::tm` 的
 * `%Z` 按"值供不出"退化为字面量，因为 `tm_zone` 里的缩写写出去读不回来。`%z` 不受影响。
 *
 * 只能是宏：数据库缺席时 `time_zone`、`zoned_time` 这些类型本身不存在，写出它们的声明
 * 在解析阶段就出错，`requires` 与 `if constexpr` 都去不掉，只能用 `#if`。
 *
 * 未定义时按标准库判断：libc++ 要求时区数据库已编入（`_LIBCPP_HAS_TIME_ZONE_DATABASE`）且
 * 打开了实验库（`-fexperimental-library`），19 之前的版本没有完整的时区支持——Apple 的 libc++
 * （Xcode 26）关掉了它；libstdc++ 与 MSVC STL 看 `__cpp_lib_chrono` 是否达到 `201907L`。
 * 也可以在包含 IOv2 之前自行定义为 `0`，强制当作没有。
 * @endif
 *
 * @lang{EN}
 * Whether the standard library's `<chrono>` has a time-zone database (`std::chrono::tzdb`,
 * `time_zone`, `zoned_time`, `locate_zone` and the rest): `1` if it does, `0` if not. Always
 * defined, so it can go straight into an `#if`.
 *
 * At `0`, `timeio` drops the interfaces that take a `time_zone` or a `zoned_time`; `%Z`
 * recognizes only `base_ft<timeio>::s_unknown_zone` and `UTC` (which is what a `sys_time`'s
 * `%Z` writes); and a `std::tm`'s `%Z` degrades to a literal as a specifier the value cannot
 * supply, since the abbreviation in `tm_zone` could be written but not read back. `%z` is
 * unaffected.
 *
 * It has to be a macro: with no database, types such as `time_zone` and `zoned_time` do not
 * exist at all, so a declaration naming them fails as it is parsed, and neither `requires`
 * nor `if constexpr` can take it away -- only `#if` can.
 *
 * Left undefined, it follows the standard library: libc++ needs the database built in
 * (`_LIBCPP_HAS_TIME_ZONE_DATABASE`) and the experimental library enabled
 * (`-fexperimental-library`), and has no complete time-zone support before version 19 --
 * Apple's libc++ (Xcode 26) leaves it out; libstdc++ and the MSVC STL are judged by
 * `__cpp_lib_chrono` reaching `201907L`. It may also be defined as `0` before including IOv2,
 * to have the database treated as absent.
 * @endif
 */
// A macro on purpose (see above), so not the constexpr constant macro-usage asks for.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#if !defined(IOV2_HAS_CHRONO_TZDB)
#if defined(_LIBCPP_VERSION)
#if defined(_LIBCPP_HAS_TIME_ZONE_DATABASE)
// LLVM 20 and later spell these as 0/1 macros that are always defined.
#if _LIBCPP_HAS_TIME_ZONE_DATABASE && _LIBCPP_HAS_EXPERIMENTAL_TZDB
#define IOV2_HAS_CHRONO_TZDB 1
#else
#define IOV2_HAS_CHRONO_TZDB 0
#endif
#elif _LIBCPP_VERSION < 190000 || defined(_LIBCPP_HAS_NO_TIME_ZONE_DATABASE) \
      || defined(_LIBCPP_HAS_NO_EXPERIMENTAL_TZDB)
#define IOV2_HAS_CHRONO_TZDB 0
#else
#define IOV2_HAS_CHRONO_TZDB 1
#endif
#elif defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
#define IOV2_HAS_CHRONO_TZDB 1
#else
#define IOV2_HAS_CHRONO_TZDB 0
#endif
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)

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
