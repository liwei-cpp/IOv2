// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file collate_details.h
 * @lang{ZH}
 * 定义了 `collate_conf` 类，这是 `collate<CharT>` facet 的底层实现。
 * 该类封装了 C 标准库的 `strcoll`/`strxfrm` 与宽字符 `wcscoll`/`wcsxfrm` 操作，
 * 提供基于 locale 的字符串比较与排序键变换功能。
 * @endif
 *
 * @lang{EN}
 * Defines the `collate_conf` class, the underlying implementation for the `collate<CharT>` facet.
 * This class wraps the C standard library's `strcoll`/`strxfrm` and wide-character
 * `wcscoll`/`wcsxfrm` operations to provide locale-aware string comparison and
 * collation-key transformation.
 * @endif
 */
#pragma once
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/defs.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/facet/facet_common.h>

#include <array>
#include <compare>
#include <cstddef>
#include <cstring>
#include <cuchar>
#include <cwchar>
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
 * 通过 C 标准库的 `strcoll`/`strxfrm` 和宽字符 `wcscoll`/`wcsxfrm`
 * 实现基于 locale 的字符串比较与排序键变换。
 * 所有输入均为以空字符结尾的字符串；按内嵌空字符分段由 `collate` facet 负责。
 *
 * @note 此类不是线程安全的，多线程并发由更高层次的代码处理。
 *
 * @tparam CharT 字符类型，支持 `char`、`wchar_t`、`char8_t` 和 `char32_t`（仅限 UTF-32 平台）。
 * @endif
 *
 * @lang{EN}
 * @brief Underlying implementation class for the `collate<CharT>` facet.
 *
 * Implements locale-aware string comparison and collation-key transformation
 * via the C standard library's `strcoll`/`strxfrm` and wide-character
 * `wcscoll`/`wcsxfrm`. Every input is a null-terminated string; splitting a
 * sequence at embedded null characters is the job of the `collate` facet.
 *
 * @note This class is not thread-safe; multi-threading is handled at a higher level.
 *
 * @tparam CharT The character type. Supports `char`, `wchar_t`, `char8_t`, and
 *               `char32_t` (only on platforms where `wchar_t` is UTF-32).
 * @endif
 */
template <typename CharT>
class collate_conf : public ft_basic<collate<CharT>>
{
    /**
     * @lang{ZH}
     * @brief `strxfrm`/`wcsxfrm` 失败时的返回值哨兵。
     *
     * `strxfrm`/`wcsxfrm` 通过返回 `(size_t)-1` 来表示失败。
     * 若将此值用作缓冲区大小，对其加 1 会发生回绕，
     * 产生大小为零的缓冲区，后续写操作将越界。
     * @endif
     *
     * @lang{EN}
     * @brief Sentinel value returned by `strxfrm`/`wcsxfrm` on failure.
     *
     * `strxfrm`/`wcsxfrm` signal failure by returning `(size_t)-1`. Using that
     * value as a buffer size would wrap on `+ 1` and silently produce a
     * zero-sized buffer that subsequent writes overflow.
     * @endif
     */
    static constexpr std::size_t xfrm_failed = static_cast<std::size_t>(-1);

public:
    /**
     * @lang{ZH}
     * @brief 构造函数，初始化 `collate_conf` 并绑定到指定的 locale。
     *
     * 当 `CharT` 为 `char8_t` 时，额外验证 `m_inter_locale` 的 LC_CTYPE 编码集是否为 UTF-8。
     * `collate<char8_t>` 内部经由窄字符 `strcoll`/`strxfrm` 路径处理，
     * 这两个函数只有在 LC_CTYPE 编码集为 UTF-8 时才能正确解析 UTF-8 字节输入。
     * 验证方法是用已知码点对 locale 自身的多字节解码器（`mbrtoc32`）进行探测——
     * `mbrtoc32` 与 `strcoll`/`strxfrm` 使用相同的 LC_CTYPE 编码集，
     * 且由于 `m_inter_locale` 以 `LC_ALL_MASK` 从单一名称构造，其 CTYPE 与
     * COLLATE 编码集一致，故探测结果可代表两者的实际编码。
     *
     * @param name locale 名称字符串（例如 `"zh_CN.UTF-8"`）。
     * @throw io_error 若 C 库无法实例化 `name`，或 `CharT` 为 `char8_t` 且 inter locale 的编码集不是 UTF-8。
     * @endif
     *
     * @lang{EN}
     * @brief Constructor that initializes `collate_conf` and binds it to the specified locale.
     *
     * When `CharT` is `char8_t`, additionally verifies that the LC_CTYPE codeset of
     * `m_inter_locale` is UTF-8. `collate<char8_t>` routes internally through the narrow
     * `strcoll`/`strxfrm` path, which correctly parses UTF-8 byte input only when the
     * LC_CTYPE codeset is UTF-8. Verification is performed by probing the locale's own
     * multibyte decoder (`mbrtoc32`) with known code points — `mbrtoc32` uses the same
     * LC_CTYPE codeset as `strcoll`/`strxfrm`, and because `m_inter_locale` is built from
     * a single name with `LC_ALL_MASK`, its CTYPE and COLLATE codesets are the same,
     * so agreement here means `strcoll`/`strxfrm` agree.
     *
     * @param name The locale name string (e.g., `"zh_CN.UTF-8"`).
     * @throw io_error If the C library cannot instantiate `name`, or `CharT` is `char8_t` and
     *        the inter locale's codeset is not UTF-8.
     * @endif
     */
    collate_conf(const std::string& name)
        : ft_basic<collate<CharT>>()
        , m_inter_locale(name)
    {
        if constexpr (std::is_same_v<CharT, char8_t>)
        {
            struct Probe { const char* mb; std::size_t len; char32_t cp; };
            static constexpr std::array<Probe, 3> probes = {{
                {"\xC3\xA9",         2, U'é'},      // U+00E9  e-acute
                {"\xE2\x82\xAC",     3, U'€'},      // U+20AC  euro sign
                {"\xF0\x9F\x98\x80", 4, U'\U0001F600'},  // U+1F600 grinning face
            }};
            clocale_user guard(m_inter_locale);
            for (const auto& p : probes)
            {
                char32_t c32 = 0;
                std::mbstate_t st{};
                std::size_t n = std::mbrtoc32(&c32, p.mb, p.len, &st);
                if ((n != p.len) || (c32 != p.cp))
                    throw io_error("collate_conf<char8_t>: inter locale is not UTF-8");
            }
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 比较两个以空字符结尾的字符串的排列顺序。
     *
     * 在 inter locale 下调用 `strcoll`/`wcscoll`。不处理内嵌的空字符：
     * 按空字符分段由 `collate` facet 负责。
     *
     * 结果为 `std::weak_ordering`：`strcoll` 返回 0 只表示两串等价，
     * 内容不同的串也可能等价（例如某些在所有层级上都被忽略的字符）。
     *
     * @param s1 第一个字符串，以空字符结尾。
     * @param s2 第二个字符串，以空字符结尾。
     * @return 表示两串排列关系的 `std::weak_ordering` 值。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of two null-terminated strings.
     *
     * Calls `strcoll`/`wcscoll` under the inter locale. Embedded null
     * characters are not handled here: splitting into null-delimited segments
     * is the job of the `collate` facet.
     *
     * The result is a `std::weak_ordering`: `strcoll` returning 0 only means
     * the two strings are equivalent, and strings with different contents may
     * be equivalent (e.g. characters ignored at every collation level).
     *
     * @param s1 The first string, null-terminated.
     * @param s2 The second string, null-terminated.
     * @return A `std::weak_ordering` value indicating the collation relationship.
     * @endif
     */
    virtual std::weak_ordering compare(const CharT* s1, const CharT* s2) const
    {
        clocale_user guard(m_inter_locale);

        int c_res = 0;
        if constexpr (std::is_same_v<CharT, char>)
            c_res = std::strcoll(s1, s2);
        else if constexpr (std::is_same_v<CharT, wchar_t>)
            c_res = std::wcscoll(s1, s2);
        else if constexpr (std::is_same_v<CharT, char8_t>)
            c_res = std::strcoll(reinterpret_cast<const char*>(s1),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                 reinterpret_cast<const char*>(s2));   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        else if constexpr ((std::is_same_v<CharT, char32_t> &&
                           wchar_t_is_utf32))
            c_res = std::wcscoll(reinterpret_cast<const wchar_t*>(s1),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                 reinterpret_cast<const wchar_t*>(s2)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        else
            static_assert(dependent_false_v<CharT>, "collate_conf::compare is not implemented.");

        return c_res <=> 0;
    }

    /**
     * @lang{ZH}
     * @brief 计算以空字符结尾的字符串的排序键长度。
     *
     * 在 inter locale 下调用 `strxfrm`/`wcsxfrm`（目标缓冲区为空、容量为 0）。
     * 返回值不含结尾空字符；调用 `transform()` 时目标容量须至少为返回值加 1。
     *
     * @param src 待变换的字符串，以空字符结尾。
     * @return 排序键的字符数，不含结尾空字符。
     * @throw io_error 若 `strxfrm`/`wcsxfrm` 报告失败。
     * @endif
     *
     * @lang{EN}
     * @brief Computes the collation-key length of a null-terminated string.
     *
     * Calls `strxfrm`/`wcsxfrm` under the inter locale with a null destination
     * and zero capacity. The result excludes the terminating null character;
     * the destination passed to `transform()` needs a capacity of at least the
     * result plus 1.
     *
     * @param src The string to transform, null-terminated.
     * @return The number of characters in the collation key, excluding the terminating null.
     * @throw io_error If `strxfrm`/`wcsxfrm` reports failure.
     * @endif
     */
    virtual std::size_t transform_length(const CharT* src) const
    {
        clocale_user guard(m_inter_locale);

        std::size_t res = 0;
        if constexpr (std::is_same_v<CharT, char>)
            res = std::strxfrm(nullptr, src, 0);
        else if constexpr (std::is_same_v<CharT, wchar_t>)
            res = std::wcsxfrm(nullptr, src, 0);
        else if constexpr (std::is_same_v<CharT, char8_t>)
            res = std::strxfrm(nullptr, reinterpret_cast<const char*>(src), 0);   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        else if constexpr ((std::is_same_v<CharT, char32_t> &&
                           wchar_t_is_utf32))
            res = std::wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(src), 0); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        else
            static_assert(dependent_false_v<CharT>, "collate_conf::transform_length is not implemented.");

        if (res == xfrm_failed)
            throw io_error("collate_conf::transform_length: strxfrm/wcsxfrm failed");
        return res;
    }

    /**
     * @lang{ZH}
     * @brief 将以空字符结尾的字符串变换为不透明的排序键。
     *
     * 在 inter locale 下调用 `strxfrm`/`wcsxfrm`，将排序键及结尾空字符写入 `dest`。
     * 输出是保序的排序权重——对结果执行 `strcmp`/`wcscmp` 等价于对原串执行
     * `strcoll`/`wcscoll`——并非可读的字符序列，也不应被解码。
     *
     * @param src 待变换的字符串，以空字符结尾。
     * @param dest 写入排序键的目标缓冲区。
     * @param n `dest` 的容量（字符数），须大于 `transform_length(src)`。
     * @return 排序键的字符数，不含结尾空字符。
     * @throw io_error 若 `strxfrm`/`wcsxfrm` 报告失败，或 `n` 不足以容纳排序键及结尾空字符。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a null-terminated string into an opaque collation key.
     *
     * Calls `strxfrm`/`wcsxfrm` under the inter locale, writing the collation
     * key and a terminating null character into `dest`. The output is
     * order-preserving sort weights — comparing results with `strcmp`/`wcscmp`
     * reproduces the `strcoll`/`wcscoll` order on the original strings — and is
     * not a readable or valid character sequence; it must never be decoded.
     *
     * @param src The string to transform, null-terminated.
     * @param dest Destination buffer where the collation key is written.
     * @param n Capacity of `dest` in characters; must exceed `transform_length(src)`.
     * @return The number of characters in the collation key, excluding the terminating null.
     * @throw io_error If `strxfrm`/`wcsxfrm` reports failure, or `n` cannot hold
     *        the key plus its terminating null.
     * @endif
     */
    virtual std::size_t transform(const CharT* src, CharT* dest, std::size_t n) const
    {
        clocale_user guard(m_inter_locale);

        std::size_t res = 0;
        if constexpr (std::is_same_v<CharT, char>)
            res = std::strxfrm(dest, src, n);
        else if constexpr (std::is_same_v<CharT, wchar_t>)
            res = std::wcsxfrm(dest, src, n);
        else if constexpr (std::is_same_v<CharT, char8_t>)
            res = std::strxfrm(reinterpret_cast<char*>(dest),         // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                               reinterpret_cast<const char*>(src),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                               n);
        else if constexpr ((std::is_same_v<CharT, char32_t> &&
                           wchar_t_is_utf32))
            res = std::wcsxfrm(reinterpret_cast<wchar_t*>(dest),      // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                               reinterpret_cast<const wchar_t*>(src), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                               n);
        else
            static_assert(dependent_false_v<CharT>, "collate_conf::transform is not implemented.");

        if (res == xfrm_failed)
            throw io_error("collate_conf::transform: strxfrm/wcsxfrm failed");
        // A short buffer leaves dest indeterminate; never hand that back as a key.
        if (res >= n)
            throw io_error("collate_conf::transform: destination too small");
        return res;
    }
private:
    /**
     * @lang{ZH}
     * @brief 封装了底层 C locale 的 RAII 包装器，用于驱动 `strcoll`/`strxfrm` 等操作。
     *
     * 由 `compare()`、`transform_length()` 和 `transform()` 共享，这三个函数均为 `const`。
     * 每次调用均构造一个 `clocale_user`，通过 `uselocale(m_inter_locale.c_locale)`
     * 切换调用线程的 locale。POSIX 未明确保证同一 `locale_t` 可被多个线程并发传递给
     * `uselocale()`；IOv2 依赖 glibc 和 macOS libc 在目标平台（Linux/macOS）上提供的
     * 事实上的线程安全性。移植到语义更严格的平台时需重新评估此假设。
     * @endif
     *
     * @lang{EN}
     * @brief RAII wrapper encapsulating the underlying C locale used by `strcoll`/`strxfrm` and related calls.
     *
     * Shared across `compare()`, `transform_length()`, and `transform()`,
     * all of which are `const`. Each call constructs a `clocale_user` that calls
     * `uselocale(m_inter_locale.c_locale)` to swap the calling thread's locale.
     * POSIX does not explicitly guarantee that the same `locale_t` may be
     * passed to `uselocale()` concurrently from multiple threads; IOv2 relies
     * on the de-facto thread-safety provided by glibc and macOS libc on the
     * target platforms (Linux / macOS). Porting to platforms with stricter
     * semantics requires reevaluating this assumption.
     * @endif
     */
    clocale_wrapper   m_inter_locale;
};
}
