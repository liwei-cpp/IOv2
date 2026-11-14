// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file collate_details.h
 * @lang{ZH}
 * 定义了 `collate_conf` 类，这是 `collate<CharT>` facet 的底层实现。
 * 该类经由 `facet/platform/collate_backend.h` 中的平台后端（POSIX 上为 `strcoll_l`/`strxfrm_l`
 * 与宽字符 `wcscoll_l`/`wcsxfrm_l`，Windows 上为 `CompareStringEx`/`LCMapStringEx`），
 * 提供基于 locale 的字符串比较与排序键变换功能。
 * @endif
 *
 * @lang{EN}
 * Defines the `collate_conf` class, the underlying implementation for the `collate<CharT>` facet.
 * This class provides locale-aware string comparison and collation-key transformation
 * through the platform backend in `facet/platform/collate_backend.h`: `strcoll_l`/`strxfrm_l`
 * and the wide `wcscoll_l`/`wcsxfrm_l` on POSIX, `CompareStringEx`/`LCMapStringEx` on Windows.
 * @endif
 */
#pragma once
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/defs.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/facet/facet_common.h>
#include <IOv2/facet/platform/collate_backend.h>

#include <compare>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>

namespace IOv2
{
template <typename CharT> class collate;
template <typename CharT> class collate_conf;

/**
 * @lang{ZH}
 * @brief `collate<CharT>` facet 的底层实现类。
 *
 * 经由平台后端（见 `facet/platform/collate_backend.h`）实现基于 locale 的字符串比较
 * 与排序键变换：`char` 与 `char8_t` 走窄字符路径，`wchar_t` 与 `char32_t` 走宽字符路径。
 * 所有输入均为以空字符结尾的字符串；按内嵌空字符分段由 `collate` facet 负责。
 *
 * @note 此类不是线程安全的，多线程并发由更高层次的代码处理。
 *
 * @tparam CharT 字符类型，支持 `char`、`wchar_t`、`char8_t` 和 `char32_t`（仅限 `wchar_t`
 *         为 UTF-32 的平台，由 `requires` 子句在编译期强制验证）。
 * @endif
 *
 * @lang{EN}
 * @brief Underlying implementation class for the `collate<CharT>` facet.
 *
 * Implements locale-aware string comparison and collation-key transformation
 * through the platform backend (see `facet/platform/collate_backend.h`): `char` and
 * `char8_t` take the narrow path, `wchar_t` and `char32_t` the wide one. Every input
 * is a null-terminated string; splitting a sequence at embedded null characters is
 * the job of the `collate` facet.
 *
 * @note This class is not thread-safe; multi-threading is handled at a higher level.
 *
 * @tparam CharT The character type. Supports `char`, `wchar_t`, `char8_t`, and
 *               `char32_t` (only on platforms where `wchar_t` is UTF-32, enforced at
 *               compile time by the `requires` clause).
 * @endif
 */
template <typename CharT>
    requires std::is_same_v<CharT, char> || std::is_same_v<CharT, char8_t> ||
             std::is_same_v<CharT, wchar_t> ||
             (std::is_same_v<CharT, char32_t> && wchar_t_is_utf32)
class collate_conf<CharT> : public ft_basic<collate<CharT>>
{
    // char8_t goes to the backend as UTF-8 bytes, char32_t as a UTF-32 wchar_t.
    using c_char = std::conditional_t<sizeof(CharT) == 1, char, wchar_t>;

    static const c_char* to_c(const CharT* s) noexcept
    {
        return reinterpret_cast<const c_char*>(s);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    static c_char* to_c(CharT* s) noexcept
    {
        return reinterpret_cast<c_char*>(s);          // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，初始化 `collate_conf` 并绑定到指定的 locale。
     *
     * 当 `CharT` 为 `char8_t` 时，额外验证 `m_inter_locale` 读窄字符串所用的编码是否为
     * UTF-8（`detail::collate_is_utf8`：POSIX 上看 `CODESET`，Windows 上看 `LC_COLLATE` 的
     * 代码页）。`collate<char8_t>` 内部经由窄字符路径处理，只有该编码为 UTF-8 时才能正确
     * 解析 UTF-8 字节输入。
     *
     * 不用 `mbrtoc32` 探测：UCRT 的 `mbrtoc32` 不论 locale 一律按 UTF-8 解码，macOS 的 libc
     * 则根本没有它。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @throw io_error 若 C 库无法实例化 `name`，或 `CharT` 为 `char8_t` 且 inter locale 的编码不是 UTF-8。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that initializes `collate_conf` and binds it to the specified locale.
     *
     * When `CharT` is `char8_t`, additionally verifies that the encoding `m_inter_locale`
     * reads narrow strings in is UTF-8 (`detail::collate_is_utf8`: `CODESET` on POSIX, the
     * code page of `LC_COLLATE` on Windows). `collate<char8_t>` routes internally through
     * the narrow path, which parses UTF-8 byte input correctly only in that case.
     *
     * Not a probe with `mbrtoc32`: the UCRT's decodes UTF-8 whatever the locale, and the
     * macOS libc has none.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @throw io_error If the C library cannot instantiate `name`, or `CharT` is `char8_t` and
     *        the inter locale's encoding is not UTF-8.
     * @endif
     */
    collate_conf(const std::string& name)
        : ft_basic<collate<CharT>>()
        , m_inter_locale(name)
    {
        if constexpr (std::is_same_v<CharT, char8_t>)
        {
            if (!detail::collate_is_utf8(m_inter_locale.c_locale))
                throw io_error("collate_conf<char8_t>: inter locale is not UTF-8");
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 比较两个以空字符结尾的字符串的排列顺序。
     *
     * 按 inter locale 调用后端的 `collate_strcoll`/`collate_wcscoll`。不处理内嵌的空字符：
     * 按空字符分段由 `collate` facet 负责。
     *
     * 结果为 `std::weak_ordering`：比较结果为 0 只表示两串等价，
     * 内容不同的串也可能等价（例如某些在所有层级上都被忽略的字符）。
     *
     * @param s1 第一个字符串，以空字符结尾。
     * @param s2 第二个字符串，以空字符结尾。
     * @return 表示两串排列关系的 `std::weak_ordering` 值。
     * @throw io_error 若后端报告失败，例如窄字符串含 locale 编码下的非法字节序列（Windows、
     *        macOS；glibc 从不报告）。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of two null-terminated strings.
     *
     * Calls the backend's `collate_strcoll`/`collate_wcscoll` with the inter locale.
     * Embedded null characters are not handled here: splitting into null-delimited
     * segments is the job of the `collate` facet.
     *
     * The result is a `std::weak_ordering`: a comparison giving 0 only means
     * the two strings are equivalent, and strings with different contents may
     * be equivalent (e.g. characters ignored at every collation level).
     *
     * @param s1 The first string, null-terminated.
     * @param s2 The second string, null-terminated.
     * @return A `std::weak_ordering` value indicating the collation relationship.
     * @throw io_error If the backend reports failure, such as a narrow string holding a
     *        byte sequence invalid in the locale's encoding (Windows, macOS; glibc never
     *        reports one).
     * @endif
     */
    virtual std::weak_ordering compare(const CharT* s1, const CharT* s2) const
    {
        std::optional<int> res;
        if constexpr (std::is_same_v<c_char, char>)
            res = detail::collate_strcoll(to_c(s1), to_c(s2), m_inter_locale.c_locale);
        else
            res = detail::collate_wcscoll(to_c(s1), to_c(s2), m_inter_locale.c_locale);

        if (!res)
            throw io_error("collate_conf::compare: the locale cannot collate the string");
        return *res <=> 0;
    }

    /**
     * @lang{ZH}
     * @brief 计算以空字符结尾的字符串的排序键长度。
     *
     * 按 inter locale 调用后端的 `collate_strxfrm`/`collate_wcsxfrm`（目标缓冲区为空、容量为 0）。
     * 返回值不含结尾空字符；调用 `transform()` 时目标容量须至少为返回值加 1。
     *
     * @param src 待变换的字符串，以空字符结尾。
     * @return 排序键的字符数，不含结尾空字符。
     * @throw io_error 若后端报告失败。
     * @endif
     *
     * @lang{EN}
     * @brief Computes the collation-key length of a null-terminated string.
     *
     * Calls the backend's `collate_strxfrm`/`collate_wcsxfrm` with the inter locale, a
     * null destination and zero capacity. The result excludes the terminating null
     * character; the destination passed to `transform()` needs a capacity of at least
     * the result plus 1.
     *
     * @param src The string to transform, null-terminated.
     * @return The number of characters in the collation key, excluding the terminating null.
     * @throw io_error If the backend reports failure.
     * @endif
     */
    virtual std::size_t transform_length(const CharT* src) const
    {
        std::optional<std::size_t> res;
        if constexpr (std::is_same_v<c_char, char>)
            res = detail::collate_strxfrm(nullptr, to_c(src), 0, m_inter_locale.c_locale);
        else
            res = detail::collate_wcsxfrm(nullptr, to_c(src), 0, m_inter_locale.c_locale);

        if (!res)
            throw io_error("collate_conf::transform_length: the locale cannot collate the string");
        return *res;
    }

    /**
     * @lang{ZH}
     * @brief 将以空字符结尾的字符串变换为不透明的排序键。
     *
     * 按 inter locale 调用后端的 `collate_strxfrm`/`collate_wcsxfrm`，将排序键及结尾空字符
     * 写入 `dest`。输出是保序的排序权重——逐字符比较两个结果等价于 `compare()` 比较原串——
     * 并非可读的字符序列，也不应被解码。
     *
     * @param src 待变换的字符串，以空字符结尾。
     * @param dest 写入排序键的目标缓冲区。
     * @param n `dest` 的容量（字符数），须大于 `transform_length(src)`。
     * @return 排序键的字符数，不含结尾空字符。
     * @throw io_error 若后端报告失败，或 `n` 不足以容纳排序键及结尾空字符。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a null-terminated string into an opaque collation key.
     *
     * Calls the backend's `collate_strxfrm`/`collate_wcsxfrm` with the inter locale,
     * writing the collation key and a terminating null character into `dest`. The
     * output is order-preserving sort weights — comparing two results character by
     * character reproduces the `compare()` order on the original strings — and is not
     * a readable or valid character sequence; it must never be decoded.
     *
     * @param src The string to transform, null-terminated.
     * @param dest Destination buffer where the collation key is written.
     * @param n Capacity of `dest` in characters; must exceed `transform_length(src)`.
     * @return The number of characters in the collation key, excluding the terminating null.
     * @throw io_error If the backend reports failure, or `n` cannot hold the key plus
     *        its terminating null.
     * @endif
     */
    virtual std::size_t transform(const CharT* src, CharT* dest, std::size_t n) const
    {
        std::optional<std::size_t> res;
        if constexpr (std::is_same_v<c_char, char>)
            res = detail::collate_strxfrm(to_c(dest), to_c(src), n, m_inter_locale.c_locale);
        else
            res = detail::collate_wcsxfrm(to_c(dest), to_c(src), n, m_inter_locale.c_locale);

        if (!res)
            throw io_error("collate_conf::transform: the locale cannot collate the string");
        // A short buffer leaves dest indeterminate; never hand that back as a key.
        if (*res >= n)
            throw io_error("collate_conf::transform: destination too small");
        return *res;
    }
private:
    /**
     * @lang{ZH}
     * @brief 封装了底层 C locale 的 RAII 包装器，其句柄传给后端的各个函数。
     *
     * 由 `compare()`、`transform_length()` 和 `transform()` 共享，这三个函数均为 `const`。
     * 后端不切换线程 locale，只把句柄传给接受 locale 参数的函数（POSIX 的 `*_l`、Windows
     * 的 NLS 函数），多个线程可以同时使用同一句柄。
     * @endif
     *
     * @lang{EN}
     * @brief RAII wrapper of the underlying C locale, whose handle the backend functions take.
     *
     * Shared across `compare()`, `transform_length()`, and `transform()`,
     * all of which are `const`. The backend never switches the thread's locale; it only
     * hands the handle to functions taking a locale argument (POSIX's `*_l`, the NLS
     * functions on Windows), which several threads may do with one handle at once.
     * @endif
     */
    clocale_wrapper   m_inter_locale;
};
}
