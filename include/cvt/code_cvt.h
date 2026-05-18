/**
 * @file code_cvt.h
 * @lang{ZH}
 * 字符编码转换器（code_cvt）及底层编码内核（codecvt_kernel）的定义文件。
 * 本文件提供两种编码内核实现：
 * - `codecvt_kernel<char, TInt>`：基于区域设置（locale）的多字节编码内核，
 *   通过 C 标准库的 mbrtowc / wcrtomb 在 char 与 wchar_t/char32_t 之间转换。
 * - `codecvt_kernel<char8_t, TInt>`：无状态 UTF-8 编码内核，
 *   直接在 char8_t 与 char32_t/wchar_t 之间转换。
 * 以及基于上述内核的通用编码转换器 `code_cvt` 及其对应的工厂类 `code_cvt_creator`。
 * @endif
 *
 * @lang{EN}
 * Definition file for the character encoding converter (code_cvt) and its
 * underlying encoding kernels (codecvt_kernel). This file provides two kernel
 * implementations:
 * - `codecvt_kernel<char, TInt>`: A locale-based multi-byte encoding kernel that
 *   converts between char and wchar_t/char32_t via the C standard library's
 *   mbrtowc / wcrtomb functions.
 * - `codecvt_kernel<char8_t, TInt>`: A stateless UTF-8 encoding kernel that
 *   converts directly between char8_t and char32_t/wchar_t.
 * Along with the generic encoding converter `code_cvt` built on these kernels,
 * and the corresponding factory class `code_cvt_creator`.
 * @endif
 */
#pragma once
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

#include <algorithm>
#include <climits>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace IOv2
{
/**
 * @lang{ZH}
 * 字符编码转换内核的主模板前向声明（不提供通用实现）。
 * 仅为以下特化版本提供实现：
 * - `codecvt_kernel<char, TInt>`：基于区域设置的多字节编码内核。
 * - `codecvt_kernel<char8_t, TInt>`：无状态 UTF-8 编码内核。
 * @endif
 *
 * @lang{EN}
 * Primary template forward declaration for the character encoding conversion kernel
 * (no generic implementation is provided). Only the following specializations
 * are implemented:
 * - `codecvt_kernel<char, TInt>`: Locale-based multi-byte encoding kernel.
 * - `codecvt_kernel<char8_t, TInt>`: Stateless UTF-8 encoding kernel.
 * @endif
 */
template <typename TExt, typename TInt>
struct codecvt_kernel;

/**
 * @lang{ZH}
 * 基于区域设置（locale）的字符编码转换内核（char <-> wchar_t/char32_t）。
 * 使用 C 标准库的 mbrtowc / wcrtomb 在多字节字符与宽字符之间进行转换，
 * 编码方式由构造时指定的区域设置名称决定，通过 clocale_wrapper 在运行时切换。
 *
 * @note 线程安全性：本类**不是**线程安全的。多线程并发访问同一实例须通过外部同步机制保护。
 *       此外，由于构造阶段调用了 `mbtowc`（其内部维护静态状态），并发构造多个实例同样不是
 *       线程安全的。构造完成后，单实例的读写操作使用显式的 `mbstate_t`（`m_state`），
 *       在单线程场景下是安全的。
 * @endif
 *
 * @lang{EN}
 * Locale-based character encoding conversion kernel (char <-> wchar_t/char32_t).
 * Uses the C standard library's mbrtowc / wcrtomb functions to convert between
 * multi-byte characters and wide characters. The encoding is determined by the
 * locale name provided at construction time and is applied at runtime via
 * clocale_wrapper.
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization. Additionally,
 *       concurrent construction of multiple instances is not thread-safe due to the
 *       use of mbtowc's internal static state. Once constructed, single-instance
 *       operations use explicit mbstate_t (m_state) and are safe for single-threaded use.
 * @endif
 */
template <typename TInt>
    requires std::is_same_v<TInt, wchar_t> ||
                (std::is_same_v<TInt, char32_t> &&
                 (sizeof(char32_t) == sizeof(wchar_t)) &&
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
struct codecvt_kernel<char, TInt>
{
    static_assert(MB_LEN_MAX <= std::numeric_limits<unsigned>::max(),
                  "MB_LEN_MAX exceeds unsigned range");

    /**
     * @lang{ZH}
     * 以指定的区域设置名称构造转换内核，并初始化编码转换状态。
     *
     * @param name 区域设置名称（如 "zh_CN.UTF-8"），传递给 clocale_wrapper。
     *
     * @throws cvt_error 若区域设置报告 `MB_CUR_MAX == 0`（通常意味着区域设置配置异常）。
     * @endif
     *
     * @lang{EN}
     * Construct the conversion kernel with the specified locale name and initialize
     * the encoding conversion state.
     *
     * @param name Locale name (e.g., "zh_CN.UTF-8"), passed to clocale_wrapper.
     *
     * @throws cvt_error If the locale reports `MB_CUR_MAX == 0` (which typically
     *                   indicates a misconfigured locale).
     * @endif
     */
    explicit codecvt_kernel(const std::string& name)
        : m_inter_locale(name.c_str())
    {
        clocale_user guard(m_inter_locale);

        // there are no known constant-length encodings
        // m_epc == 1 means fixed length
        // Defensive check: the C standard requires MB_CUR_MAX to be a positive
        // integer, but a misconfigured CRT / exotic libc could report 0, which
        // would lead to division-by-zero UB in seek / rseek. Reject it here so
        // m_epc >= 1 holds as a class invariant for any constructed instance.
        const auto cur_max = MB_CUR_MAX;
        if (cur_max == 0) [[unlikely]]
            throw cvt_error("codecvt_kernel: locale reports MB_CUR_MAX == 0");
        m_epc = static_cast<unsigned>(cur_max);

        // Note: mbtowc uses internal static state, so concurrent construction of
        // codecvt_kernel instances is NOT thread-safe. However, this is acceptable
        // because this class does not support multi-threaded construction.
        // Once constructed, instance-level operations use explicit mbstate_t (m_state)
        // and are safe for single-instance usage.
        // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
        m_is_state_dep = (std::mbtowc(nullptr, nullptr, MB_CUR_MAX) != 0);
        init_state();
    }

    /**
     * @lang{ZH}
     * 将编码转换状态（`mbstate_t`）重置为初始状态。
     * @endif
     *
     * @lang{EN}
     * Reset the encoding conversion state (`mbstate_t`) to its initial value.
     * @endif
     */
    void init_state() { m_state = std::mbstate_t{}; }

    /**
     * @lang{ZH}
     * 判断编码转换状态是否处于初始状态。
     *
     * @return 若 `mbstate_t` 为初始状态，返回 `true`；否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * Check whether the encoding conversion state is in its initial state.
     *
     * @return `true` if `mbstate_t` is in its initial state; `false` otherwise.
     * @endif
     */
    [[nodiscard]] bool is_init_state() const { return std::mbsinit(&m_state); }

    /**
     * @lang{ZH}
     * 返回每个内部字符对应的最大外部字节数（即当前区域设置的 `MB_CUR_MAX`）。
     *
     * @return 最大多字节字节数（External bytes Per internal Character，简称 epc）。
     * @endif
     *
     * @lang{EN}
     * Return the maximum number of external bytes per internal character
     * (i.e., `MB_CUR_MAX` for the current locale).
     *
     * @return Maximum number of multi-byte bytes (External bytes Per internal Character, epc).
     * @endif
     */
    [[nodiscard]] unsigned epc() const { return m_epc; }

    /**
     * @lang{ZH}
     * 判断当前区域设置的编码是否为变长编码。
     * 当 `epc() > 1` 时，视为变长编码。
     *
     * @return 若为变长编码，返回 `true`；若为定长编码（每字符恰好 1 字节），返回 `false`。
     * @endif
     *
     * @lang{EN}
     * Check whether the encoding for the current locale is variable-length.
     * When `epc() > 1`, the encoding is treated as variable-length.
     *
     * @return `true` if the encoding is variable-length; `false` if it is fixed-length
     *         (exactly 1 byte per character).
     * @endif
     */
    [[nodiscard]] bool is_var_length() const { return m_epc != 1; }

    /**
     * @lang{ZH}
     * 判断当前区域设置的编码是否为状态依赖编码（如 Shift-JIS）。
     *
     * @return 若为状态依赖编码，返回 `true`；否则返回 `false`。
     * @endif
     *
     * @lang{EN}
     * Check whether the encoding for the current locale is state-dependent
     * (e.g., Shift-JIS).
     *
     * @return `true` if the encoding is state-dependent; `false` otherwise.
     * @endif
     */
    [[nodiscard]] bool is_state_dep() const { return m_is_state_dep; }

    /**
     * @lang{ZH}
     * 将单个内部字符编码为外部字节序列，并追加写入输出缓冲区。
     *
     * @param ch      待编码的内部字符。
     * @param to      指向输出缓冲区当前写入位置的指针（成功时向后推进）。
     * @param to_end  输出缓冲区的结束指针（不可写入此位置）。
     *
     * @return 若成功编码并写入，返回 `true`；若缓冲区空间不足或字符无法编码，返回 `false`。
     *
     * @note 前置条件：`to` 和 `to_end` 必须指向同一个数组，否则行为未定义。
     *       `std::greater` 可在任意指针间建立全序，但后续的指针减法要求两者同源（[expr.add]/5）。
     * @endif
     *
     * @lang{EN}
     * Encode a single internal character into the external byte sequence and
     * append it to the output buffer.
     *
     * @param ch      Internal character to encode.
     * @param to      Pointer to the current write position in the output buffer
     *                (advanced on success).
     * @param to_end  One-past-the-end pointer of the output buffer.
     *
     * @return `true` if the character was successfully encoded and written;
     *         `false` if the buffer is too small or the character cannot be encoded.
     *
     * @note Precondition: `to` and `to_end` must point into the same array;
     *       behavior is undefined otherwise. `std::greater` enforces a total order
     *       across pointers regardless of provenance, but the pointer subtraction
     *       below requires same-object provenance per [expr.add]/5.
     * @endif
     */
    bool out_helper(TInt ch, char*& to, char* to_end)
    {
        if (std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::out_helper fail: invalid pointer range");
        clocale_user guard(m_inter_locale);
        if (static_cast<size_t>(to_end - to) < m_epc) // NOLINT(modernize-use-integer-sign-comparison)
            return false;

        const size_t conv = std::wcrtomb(to, ch, &m_state);
        if (conv == static_cast<size_t>(-1)) // NOLINT(modernize-use-integer-sign-comparison)
        {
            init_state();  // Reset to known state per C standard
            return false;
        }
        to += conv;

        return true;
    }

    /**
     * @lang{ZH}
     * 将外部字节序列解码为内部字符，并写入输出缓冲区。
     *
     * @param from      指向输入字节序列当前读取位置的指针（处理后向后推进）。
     * @param from_end  输入字节序列的结束指针。
     * @param to        指向输出缓冲区当前写入位置的指针（写入后向后推进）。
     * @param to_end    输出缓冲区的结束指针。
     *
     * @return 一个 `std::pair<bool, size_t>`：
     *         - `first`：转换是否成功（`false` 表示遇到无效字节序列）。
     *         - `second`：已写入输出缓冲区的内部字符数量。
     *
     * @note 前置条件：`from`/`from_end` 和 `to`/`to_end` 各自必须指向同一数组，否则行为未定义。
     * @endif
     *
     * @lang{EN}
     * Decode external byte sequences into internal characters and write them to
     * the output buffer.
     *
     * @param from      Pointer to the current read position in the input byte sequence
     *                  (advanced as bytes are consumed).
     * @param from_end  One-past-the-end pointer of the input byte sequence.
     * @param to        Pointer to the current write position in the output buffer
     *                  (advanced as characters are written).
     * @param to_end    One-past-the-end pointer of the output buffer.
     *
     * @return A `std::pair<bool, size_t>`:
     *         - `first`: Whether the conversion succeeded (`false` indicates an invalid
     *           byte sequence).
     *         - `second`: Number of internal characters written to the output buffer.
     *
     * @note Precondition: `from`/`from_end` and `to`/`to_end` must each point into
     *       the same array; behavior is undefined otherwise. `std::greater` enforces
     *       a total order across pointers regardless of provenance, but the pointer
     *       subtractions below require same-object provenance per [expr.add]/5.
     * @endif
     */
    std::pair<bool, size_t> in_helper(const char*& from, const char* from_end,
                                      TInt*& to, TInt* to_end)
    {
        if (std::greater<>{}(from, from_end) || std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::in_helper fail: invalid pointer range");
        clocale_user guard(m_inter_locale);
        wchar_t wch = 0;
        size_t i_count = 0;

        const size_t to_max = to_end - to;
        while (from < from_end && (i_count < to_max))
        {
            auto tmp_state = m_state;
            size_t conv = mbrtowc(&wch, from, from_end - from, &tmp_state);
            if (conv == static_cast<size_t>(-1)) // NOLINT(modernize-use-integer-sign-comparison)
                return std::pair{false, i_count};
            else if (conv == static_cast<size_t>(-2)) // NOLINT(modernize-use-integer-sign-comparison)
            {
                from = from_end;
                m_state = tmp_state;
                break;
            }
            else if (conv == 0)
            {
                // Find the actual byte length of the null character encoding
                // before writing to output buffer to ensure consistency
                size_t n = 1;
                const auto max_n = static_cast<size_t>(from_end - from);
                for (; n <= max_n; ++n)
                {
                    auto tmp_state2(m_state);
                    if (mbrtowc(nullptr, from, n, &tmp_state2) == 0)
                    {
                        m_state = tmp_state2;
                        break;
                    }
                }
                if (n > max_n)
                    return std::pair{false, i_count};

                // Only write to output after validation succeeds
                *to++ = static_cast<TInt>(0);
                from += n;
                ++i_count;
            }
            else
            {
                // mbrtowc returning > 0 guarantees a complete character was converted;
                // no partial character can exist in m_state at this point
                m_state = tmp_state;
                *to++ = wch;
                from += conv;
                ++i_count;
            }
        }

        return std::pair{true, i_count};
    }

private:
    clocale_wrapper m_inter_locale; ///< 当前区域设置的包装对象 / Wrapper for the current locale.
    unsigned        m_epc = 0;      ///< 每个内部字符对应的最大外部字节数 / Max external bytes per internal character.
    std::mbstate_t  m_state{};      ///< 多字节转换状态 / Multi-byte conversion state.
    bool            m_is_state_dep = false; ///< 编码是否为状态依赖型 / Whether the encoding is state-dependent.
};

/**
 * @lang{ZH}
 * 无状态 UTF-8 编码转换内核（char8_t <-> char32_t/wchar_t）。
 * 直接按照 UTF-8 规范对码点进行编解码，无需区域设置，无转换状态。
 *
 * @note 线程安全性：本类**不是**线程安全的。多线程并发访问同一实例须通过外部同步机制保护。
 *       不同于 `codecvt_kernel<char, TInt>`，多实例的并发构造是安全的（无共享静态状态）。
 * @endif
 *
 * @lang{EN}
 * Stateless UTF-8 encoding conversion kernel (char8_t <-> char32_t/wchar_t).
 * Encodes and decodes code points directly according to the UTF-8 specification,
 * requiring neither a locale nor any conversion state.
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization.
 *       Unlike `codecvt_kernel<char, TInt>`, concurrent construction of multiple
 *       instances is safe (no shared static state).
 * @endif
 */
template <typename TInt>
    requires std::is_same_v<TInt, char32_t> ||
                (std::is_same_v<TInt, wchar_t> &&
                 (sizeof(char32_t) == sizeof(wchar_t)) &&
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
struct codecvt_kernel<char8_t, TInt>
{
    codecvt_kernel() = default;

    /**
     * @lang{ZH}
     * 重置编码转换状态（UTF-8 无状态，为空操作）。
     * @endif
     *
     * @lang{EN}
     * Reset the encoding conversion state (no-op for stateless UTF-8).
     * @endif
     */
    void init_state() { /* no-op for stateless UTF-8 */ }

    /**
     * @lang{ZH}
     * 判断编码转换状态是否处于初始状态（UTF-8 无状态，始终返回 `true`）。
     *
     * @return 始终返回 `true`。
     * @endif
     *
     * @lang{EN}
     * Check whether the encoding conversion state is in its initial state
     * (always `true` for stateless UTF-8).
     *
     * @return Always `true`.
     * @endif
     */
    [[nodiscard]] bool is_init_state() const { return true; }

    /**
     * @lang{ZH}
     * 返回每个内部字符对应的最大外部字节数（UTF-8 最长 4 字节）。
     *
     * @return 固定返回 `4`。
     * @endif
     *
     * @lang{EN}
     * Return the maximum number of external bytes per internal character
     * (UTF-8 uses at most 4 bytes per code point).
     *
     * @return Always `4`.
     * @endif
     */
    [[nodiscard]] unsigned epc() const { return 4; }

    /**
     * @lang{ZH}
     * 判断编码是否为变长编码（UTF-8 为变长编码，始终返回 `true`）。
     *
     * @return 始终返回 `true`。
     * @endif
     *
     * @lang{EN}
     * Check whether the encoding is variable-length (UTF-8 is always variable-length).
     *
     * @return Always `true`.
     * @endif
     */
    [[nodiscard]] bool is_var_length() const { return true; }

    /**
     * @lang{ZH}
     * 判断编码是否为状态依赖编码（UTF-8 无状态，始终返回 `false`）。
     *
     * @return 始终返回 `false`。
     * @endif
     *
     * @lang{EN}
     * Check whether the encoding is state-dependent (UTF-8 is stateless).
     *
     * @return Always `false`.
     * @endif
     */
    [[nodiscard]] bool is_state_dep() const { return false; }

    /**
     * @lang{ZH}
     * 将单个内部字符（UTF-32 码点）编码为 UTF-8 字节序列，并追加写入输出缓冲区。
     * 拒绝代理码点（[0xD800, 0xDFFF]）及超出 Unicode 范围（> 0x10FFFF）的码点。
     *
     * @param ch      待编码的内部字符（UTF-32 码点）。
     * @param to      指向输出缓冲区当前写入位置的指针（成功时向后推进）。
     * @param to_end  输出缓冲区的结束指针（不可写入此位置）。
     *
     * @return 若成功编码并写入，返回 `true`；若缓冲区空间不足（< 4 字节）
     *         或码点无效，返回 `false`。
     *
     * @note 前置条件：`to` 和 `to_end` 必须指向同一个数组，否则行为未定义。
     * @endif
     *
     * @lang{EN}
     * Encode a single internal character (UTF-32 code point) into the UTF-8 byte
     * sequence and append it to the output buffer. Surrogate code points
     * ([0xD800, 0xDFFF]) and values beyond the Unicode range (> 0x10FFFF) are rejected.
     *
     * @param ch      Internal character (UTF-32 code point) to encode.
     * @param to      Pointer to the current write position in the output buffer
     *                (advanced on success).
     * @param to_end  One-past-the-end pointer of the output buffer.
     *
     * @return `true` if the code point was successfully encoded and written;
     *         `false` if the buffer is too small (< 4 bytes) or the code point is invalid.
     *
     * @note Precondition: `to` and `to_end` must point into the same array;
     *       behavior is undefined otherwise. `std::greater` enforces a total order
     *       across pointers regardless of provenance, but the pointer subtraction
     *       below requires same-object provenance per [expr.add]/5.
     * @endif
     */
    bool out_helper(TInt ch, char8_t*& to, char8_t* to_end)
    {
        if (std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::out_helper fail: invalid pointer range");
        // Check for maximum UTF-8 length (4 bytes) upfront
        if (to_end - to < 4)
            return false;
        const auto c = static_cast<uint32_t>(ch);
        if (0xD800U <= c && c <= 0xDFFFU) [[unlikely]]
            return false;

        if (c < 0x80U)
            *to++ = static_cast<char8_t>(c);
        else if (c <= 0x7ffU)
        {
            *to++ = static_cast<char8_t>((c >> 6) + 0xC0U);
            *to++ = static_cast<char8_t>((c & 0x3FU) + 0x80U);
        }
        else if (c <= 0xFFFFU)
        {
            *to++ = static_cast<char8_t>((c >> 12) + 0xE0U);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3FU) + 0x80U);
            *to++ = static_cast<char8_t>((c & 0x3FU) + 0x80U);
        }
        else if (c <= 0x10FFFFU)
        {
            *to++ = static_cast<char8_t>((c >> 18) + 0xF0U);
            *to++ = static_cast<char8_t>(((c >> 12) & 0x3FU) + 0x80U);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3FU) + 0x80U);
            *to++ = static_cast<char8_t>((c & 0x3FU) + 0x80U);
        }
        else [[unlikely]]
            return false;
        return true;
    }

    /**
     * @lang{ZH}
     * 将 UTF-8 字节序列解码为内部字符（UTF-32 码点），并写入输出缓冲区。
     * 严格验证 UTF-8 格式：拒绝超长编码、代理码点、超出范围的码点，以及格式错误的续接字节。
     *
     * @param from      指向输入 UTF-8 字节序列当前读取位置的指针（处理后向后推进）。
     * @param from_end  输入字节序列的结束指针。
     * @param to        指向输出缓冲区当前写入位置的指针（写入后向后推进）。
     * @param to_end    输出缓冲区的结束指针。
     *
     * @return 一个 `std::pair<bool, size_t>`：
     *         - `first`：转换是否成功（`false` 表示遇到无效 UTF-8 序列）。
     *         - `second`：已写入输出缓冲区的内部字符数量。
     *
     * @note 前置条件：`from`/`from_end` 和 `to`/`to_end` 各自必须指向同一数组，否则行为未定义。
     * @endif
     *
     * @lang{EN}
     * Decode a UTF-8 byte sequence into internal characters (UTF-32 code points)
     * and write them to the output buffer. Strictly validates UTF-8 format: rejects
     * overlong encodings, surrogate code points, out-of-range values, and malformed
     * continuation bytes.
     *
     * @param from      Pointer to the current read position in the input UTF-8 byte
     *                  sequence (advanced as bytes are consumed).
     * @param from_end  One-past-the-end pointer of the input byte sequence.
     * @param to        Pointer to the current write position in the output buffer
     *                  (advanced as characters are written).
     * @param to_end    One-past-the-end pointer of the output buffer.
     *
     * @return A `std::pair<bool, size_t>`:
     *         - `first`: Whether the conversion succeeded (`false` indicates an invalid
     *           UTF-8 sequence).
     *         - `second`: Number of internal characters written to the output buffer.
     *
     * @note Precondition: `from`/`from_end` and `to`/`to_end` must each point into
     *       the same array; behavior is undefined otherwise. `std::greater` enforces
     *       a total order across pointers regardless of provenance, but the pointer
     *       subtractions below require same-object provenance per [expr.add]/5.
     * @endif
     */
    std::pair<bool, size_t> in_helper(const char8_t*& from, const char8_t* from_end,
                                      TInt*& to, TInt* to_end)
    {
        if (std::greater<>{}(from, from_end) || std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::in_helper fail: invalid pointer range");
        const TInt* const ori_to = to;

        while ((from != from_end) && (to != to_end))
        {
            auto c1 = static_cast<uint32_t>(*from);
            if (c1 < 0x80U) [[likely]]
            {
                ++from;
                *to++ = static_cast<TInt>(c1);
            }
            else if (c1 < 0xE0U)
            {
                if (c1 < 0xC0U) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                if (from_end - from < 2) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                if ((c2 & 0xC0U) != 0x80U) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 6) + c2 - 0x3080U;
                if (c < 0x80U) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 2;
            }
            else if (c1 < 0xF0U)
            {
                if (from_end - from < 3) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                if (((c2 & 0xC0U) != 0x80U) || ((c3 & 0xC0U) != 0x80U)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 12) + (c2 << 6) + c3 - 0xE2080U;
                if (c < 0x800U) return std::pair{false, static_cast<size_t>(to - ori_to)};
                if (c >= 0xD800U && c <= 0xDFFFU) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 3;
            }
            else if (c1 < 0xF8U)
            {
                if (from_end - from < 4) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                auto c4 = static_cast<uint32_t>(from[3]);
                if (((c2 & 0xC0U) != 0x80U) || ((c3 & 0xC0U) != 0x80U) || ((c4 & 0xC0U) != 0x80U)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 18) + (c2 << 12) + (c3 << 6) + c4 - 0x3C82080U;
                if (c < 0x10000U || c > 0x10FFFFU) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 4;
            }
            else
                return std::pair{false, static_cast<size_t>(to - ori_to)};
        }

        return std::pair{true, static_cast<size_t>(to - ori_to)};
    }
};

/**
 * @lang{ZH}
 * 字符编码转换器，在底层内核的外部字符类型与本层的内部字符类型之间进行转换。
 *
 * `code_cvt` 通过 CRTP 继承自 `abs_cvt`，将编码转换任务委托给
 * `codecvt_kernel<external_type, internal_type>` 实例。
 *
 * @tparam KernelType 底层 I/O 转换器类型，须满足 `io_converter` 概念；
 *                    其 `internal_type` 须与 `codecvt_kernel` 所支持的外部类型匹配。
 * @tparam CharType   本层的内部字符类型（如 `wchar_t`、`char32_t`）。
 *
 * @note 线程安全性：本类**不是**线程安全的。多线程并发访问同一实例须通过外部同步机制保护。
 * @endif
 *
 * @lang{EN}
 * Character encoding converter that transforms between the external character type
 * of an underlying kernel and the internal character type at this layer.
 *
 * `code_cvt` inherits from `abs_cvt` via CRTP and delegates encoding conversion
 * to a `codecvt_kernel<external_type, internal_type>` instance.
 *
 * @tparam KernelType Underlying I/O converter type, must satisfy the `io_converter`
 *                    concept; its `internal_type` must match the external type
 *                    supported by `codecvt_kernel`.
 * @tparam CharType   Internal character type at this layer (e.g., `wchar_t`, `char32_t`).
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization.
 * @endif
 */
template <io_converter KernelType, typename CharType>
class code_cvt : public abs_cvt<code_cvt<KernelType, CharType>, KernelType, CharType, false, false>
{
    using BT = abs_cvt<code_cvt<KernelType, CharType>, KernelType, CharType, false, false>;
    friend BT; // for put_main, get_main, and private CRTP hooks

public:
    using device_type = typename KernelType::device_type;   ///< 底层设备类型 / Underlying device type.
    using internal_type = CharType;                          ///< 本层内部字符类型 / Internal character type at this layer.
    using external_type = typename KernelType::internal_type; ///< 底层内核的内部类型（即本层外部类型）/ Internal type of the underlying kernel (external type at this layer).

private:
    /// 内部字符与外部字节的大小比（用于缓冲区计算）/ Size ratio of internal to external type (for buffer sizing).
    constexpr static size_t s_ie_ratio = sizeof(internal_type) / sizeof(external_type);
    /// 内部缓冲区最大大小 / Maximum size of the internal buffer.
    constexpr static size_t s_max_buf_size = std::max<size_t>(MB_LEN_MAX * 16, s_ie_ratio);

    static_assert(sizeof(internal_type) >= sizeof(external_type));
    static_assert((sizeof(internal_type) % sizeof(external_type)) == 0);
    static_assert(s_max_buf_size % s_ie_ratio == 0);

public:
    /**
     * @lang{ZH}
     * 以指定的底层内核和编码内核参数构造 `code_cvt`。
     *
     * @param kernel   底层 I/O 转换器实例（移动传入）。
     * @param params   转发给 `codecvt_kernel` 构造函数的额外参数
     *                 （例如，对于 locale-based 内核，为区域设置名称字符串）。
     * @endif
     *
     * @lang{EN}
     * Construct a `code_cvt` with the specified underlying kernel and encoding
     * kernel parameters.
     *
     * @param kernel   Underlying I/O converter instance (moved in).
     * @param params   Additional parameters forwarded to the `codecvt_kernel` constructor
     *                 (e.g., a locale name string for the locale-based kernel).
     * @endif
     */
    template <typename ... TParams>
    code_cvt(KernelType kernel, TParams&&... params)
        : BT(std::move(kernel))
        , m_cvt_kernel(std::forward<TParams>(params)...)
    {}

    /**
     * @lang{ZH}
     * 拷贝构造函数（仅当 `KernelType` 满足 `std::copy_constructible` 时可用）。
     * @endif
     *
     * @lang{EN}
     * Copy constructor (available only when `KernelType` satisfies `std::copy_constructible`).
     * @endif
     */
    code_cvt(const code_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_cvt_kernel(val.m_cvt_kernel)
        , m_accu_len(val.m_accu_len)
    {}

    /**
     * @lang{ZH}
     * 拷贝赋值运算符（仅当 `KernelType` 满足 `std::copy_constructible` 时可用）。
     * 通过拷贝后移动实现强异常安全保证。
     * @endif
     *
     * @lang{EN}
     * Copy assignment operator (available only when `KernelType` satisfies
     * `std::copy_constructible`). Implemented via copy-then-move to provide
     * the strong exception safety guarantee.
     * @endif
     */
    code_cvt& operator=(const code_cvt& val)
        requires (std::copy_constructible<KernelType>)
    {
        if (this == &val) return *this;
        code_cvt tmp(val);
        *this = std::move(tmp);
        return *this;
    }

    /**
     * @lang{ZH}
     * 移动构造函数。源对象移动后处于有效但未指定的状态（`m_accu_len` 重置为 0）。
     * @endif
     *
     * @lang{EN}
     * Move constructor. The source object is left in a valid but unspecified state
     * after the move (`m_accu_len` is reset to 0).
     * @endif
     */
    code_cvt(code_cvt&& val) noexcept
        : BT(std::move(val))
        , m_cvt_kernel(std::move(val.m_cvt_kernel))
        , m_accu_len(val.m_accu_len)
    {
        val.m_accu_len = 0;
    }

    /**
     * @lang{ZH}
     * 移动赋值运算符。源对象移动后处于有效但未指定的状态（`m_accu_len` 重置为 0）。
     * @endif
     *
     * @lang{EN}
     * Move assignment operator. The source object is left in a valid but unspecified
     * state after the move (`m_accu_len` is reset to 0).
     * @endif
     */
    code_cvt& operator=(code_cvt&& val) noexcept
    {
        if (this == &val) return *this;
        BT::operator=(std::move(val));
        m_cvt_kernel = std::move(val.m_cvt_kernel);
        m_accu_len = val.m_accu_len;
        val.m_accu_len = 0;
        return *this;
    }

    ~code_cvt() = default;

private:
    /**
     * @lang{ZH}
     * `abs_cvt::detach()` 的 CRTP 钩子，在 kernel 层 `detach()` 之前调用。
     *
     * 负责执行 `code_cvt` 层面的清理（`close_stream()`），并将捕获到的异常以
     * `exception_ptr` 形式返回；调用方（`abs_cvt::detach()`）负责按 first-failure-wins
     * 与 kernel 层异常合并。本函数为 `noexcept`，此约束由 `abs_cvt` 的 `static_assert` 强制。
     *
     * @return 捕获到的首个清理异常；无异常时为 `nullptr`。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::detach()`, called before the kernel-level `detach()`.
     *
     * Performs `code_cvt`-layer cleanup (`close_stream()`) and returns any captured
     * exception as an `exception_ptr`; the caller (`abs_cvt::detach()`) merges it
     * with the kernel-layer exception under first-failure-wins.
     * Must be `noexcept` — enforced by a `static_assert` in `abs_cvt`.
     *
     * @return The first captured cleanup exception, or `nullptr` if none.
     * @endif
     */
    std::exception_ptr detach_impl() noexcept
    {
        std::exception_ptr local_err = nullptr;
        try { close_stream(); }
        catch (...) { local_err = std::current_exception(); }
        return local_err;
    }

    /**
     * @lang{ZH}
     * `abs_cvt::main_cont_beg()` 的 CRTP 钩子，在 kernel 层 `main_cont_beg()` 之后调用。
     *
     * 负责重置编码转换状态并清零累计字符计数，使主内容阶段从干净状态开始。
     * 允许抛出异常；调用方会将 `m_io_status` 重置为 `neutral` 并设置污染标志后透传。
     * @endif
     *
     * @lang{EN}
     * CRTP hook for `abs_cvt::main_cont_beg()`, called after the kernel-level
     * `main_cont_beg()`.
     *
     * Resets the codec conversion state and clears the accumulated character count
     * so the main-content phase begins from a clean state.
     * May throw; the caller resets `m_io_status` to `neutral` and sets the taint
     * flag before rethrowing.
     * @endif
     */
    void main_cont_beg_impl()
    {
        m_cvt_kernel.init_state();
        m_accu_len = 0;
    }

    /**
     * @lang{ZH}
     * 将内部字符序列编码为外部字节并写入底层缓冲区（由 `abs_cvt::put` 通过 CRTP 调用）。
     * 依次对每个内部字符调用编码内核的 `out_helper()`，成功后更新累计字符计数。
     * @endif
     *
     * @lang{EN}
     * Encode the internal character sequence to external bytes and write them to
     * the underlying buffer (called by `abs_cvt::put` via CRTP).
     * Invokes the encoding kernel's `out_helper()` for each internal character
     * in sequence, updating the accumulated character count on success.
     * @endif
     */
    void put_main(cvt_writer<KernelType>& writer, const internal_type* to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        writer.reset(s_max_buf_size);
        const size_t buf_len = m_cvt_kernel.epc();
        for (size_t i = 0; i < to_size; ++i)
        {
            external_type* out_beg = writer.put_buf(buf_len);
            external_type* out_next = out_beg;

            internal_type ch = *to++;

            if (!m_cvt_kernel.out_helper(ch, out_next, out_beg + buf_len))
                throw cvt_error("code_cvt::put fail: input character cannot be encoded");

            if (out_next < out_beg + buf_len)
                writer.rollback(out_beg + buf_len - out_next);
        }
        // commit() is the responsibility of abs_cvt::put; m_accu_len is
        // updated only on success so that a thrown encoding error leaves
        // it untouched (the converter will be tainted by abs_cvt::put,
        // so further reads of m_accu_len are blocked anyway).
        m_accu_len += to_size;
    }

    /**
     * @lang{ZH}
     * 从底层缓冲区读取外部字节并解码为内部字符（由 `abs_cvt::get` 通过 CRTP 调用）。
     *
     * @return 实际读取并解码的内部字符数量。
     * @endif
     *
     * @lang{EN}
     * Read external bytes from the underlying buffer and decode them into
     * internal characters (called by `abs_cvt::get` via CRTP).
     *
     * @return Number of internal characters actually read and decoded.
     * @endif
     */
    size_t get_main(cvt_reader<KernelType>& reader, internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (to_max == 0) return 0;
        reader.reset(s_max_buf_size);
        size_t total_size = 0;

        size_t prev_rollback = 0;
        while (total_size < to_max)
        {
            size_t dest_size = std::min<size_t>(to_max - total_size, s_max_buf_size);
            dest_size = std::max(dest_size, prev_rollback + 1);

            if (dest_size > s_max_buf_size) [[unlikely]]
                throw cvt_error("code_cvt::get fail: input sequence too long");

            auto [ptr, cur_size] = reader.get_buf(dest_size);
            if (cur_size == prev_rollback)
            {
                if (cur_size == 0) return total_size;
                throw cvt_error("code_cvt::get fail: partial input sequence");
            }

            auto ext_cur = ptr;
            auto [succ, int_len] = m_cvt_kernel.in_helper(ext_cur, ptr + cur_size, to, to + to_max - total_size);

            // Update accumulated length BEFORE the failure check: in_helper may
            // have written `int_len` chars into the caller's `to` buffer before
            // hitting an invalid byte. Throwing without this update would leave
            // m_accu_len inconsistent with the chars already produced.
            m_accu_len += int_len;
            total_size += int_len;

            if (!succ)
                throw cvt_error("code_cvt::get fail: invalid external sequence");

            if (ext_cur == ptr + cur_size)
                prev_rollback = 0;
            else
            {
                prev_rollback = ptr + cur_size - ext_cur;
                reader.rollback(prev_rollback);
            }
        }
        return total_size;
    }

public:
    /// 定位操作 / Positioning operations

    /**
     * @lang{ZH}
     * 返回当前流中已处理的内部字符总数（即逻辑位置）。
     *
     * @return 已处理的内部字符数量。
     * @endif
     *
     * @lang{EN}
     * Return the total number of internal characters processed so far in the stream
     * (i.e., the logical position).
     *
     * @return Number of internal characters processed.
     * @endif
     */
    [[nodiscard]] size_t tell() const
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::assert_not_tainted();
        return m_accu_len;
    }

    /**
     * @lang{ZH}
     * 将流定位至指定的绝对内部字符位置。
     * 对于变长或状态依赖编码，仅允许 `seek(0)`（定位至流起始）且须处于输入模式。
     * 当目标位置等于当前位置时，直接返回（快速路径）。
     *
     * @param pos 目标内部字符位置。
     *
     * @throws cvt_error 若当前使用变长或状态依赖编码且不满足上述条件，
     *                   或目标位置导致底层设备字节偏移溢出。
     * @endif
     *
     * @lang{EN}
     * Position the stream to the specified absolute internal character position.
     * For variable-length or state-dependent encodings, only `seek(0)` (repositioning
     * to the beginning of the stream) in input mode is permitted.
     * Returns immediately when the target position equals the current position (fast path).
     *
     * @param pos Target internal character position.
     *
     * @throws cvt_error If the encoding is variable-length or state-dependent and the
     *                   above conditions are not met, or if the target position causes
     *                   a byte-offset overflow in the underlying device.
     * @endif
     */
    void seek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::assert_not_tainted();
        // Fast path for no-op self-seek: skip validation and kernel work when
        // pos already equals the current position. Intentional even in modes
        // where the validation below would otherwise reject — callers using
        // seek(saved_pos) for position-restore must not be rejected when
        // saved_pos happens to equal tell().
        if (this->tell() == pos) return;

        const bool needs_state_reset =
            m_cvt_kernel.is_var_length() || m_cvt_kernel.is_state_dep();

        if (needs_state_reset && (pos != 0 || BT::m_io_status == io_status::output))
            throw cvt_error("code_cvt::seek fail: cannot seek with dependent converter");

        const unsigned epc = m_cvt_kernel.epc();
        if (pos > std::numeric_limits<size_t>::max() / epc)
            throw cvt_error("code_cvt::seek fail: position overflow");

        // Commit lower layer first. If it throws, this object is unchanged
        // (strong exception guarantee). After this point the remaining
        // mutations are noexcept (mbstate_t reset / scalar assignment), so
        // we cannot end up half-updated.
        BT::m_kernel.seek(pos * epc);

        if (needs_state_reset)
            m_cvt_kernel.init_state();
        m_accu_len = pos;
    }

    /**
     * @lang{ZH}
     * 将流重新定位至底层设备相对字节位置所对应的内部字符位置。
     * 仅支持定长且状态无关的编码；否则抛出异常。
     * 调用完成后验证底层设备新位置必须是编码单元大小的整数倍（对齐检查）。
     *
     * @param pos 传递给底层内核 `rseek` 的相对外部字节位置。
     *
     * @throws cvt_error 若编码为变长或状态依赖型、位置溢出，或底层设备新位置未对齐。
     * @endif
     *
     * @lang{EN}
     * Reposition the stream to the internal character position corresponding to the
     * specified relative external byte position in the underlying device.
     * Only fixed-length, state-independent encodings are supported; throws otherwise.
     * After the call, validates that the resulting device byte position is aligned
     * to the encoding unit size.
     *
     * @param pos Relative external byte position passed to the underlying kernel's `rseek`.
     *
     * @throws cvt_error If the encoding is variable-length or state-dependent, if the
     *                   position overflows, or if the resulting device position is misaligned.
     * @endif
     */
    void rseek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        BT::assert_not_tainted();
        if (m_cvt_kernel.is_var_length() || m_cvt_kernel.is_state_dep())
            throw cvt_error("code_cvt::rseek fail: cannot seek with dependent converter");

        const unsigned epc = m_cvt_kernel.epc();
        if (pos > std::numeric_limits<size_t>::max() / epc)
            throw cvt_error("code_cvt::rseek fail: position overflow");

        const size_t saved_dev_pos = BT::m_kernel.tell();
        BT::m_kernel.rseek(pos * epc);
        const size_t new_dev_pos = BT::m_kernel.tell();

        if (new_dev_pos % epc != 0)
        {
            // Restore device so this call appears atomic. If the restore
            // itself throws, suppress it — the caller already gets a
            // cvt_error and is expected to re-seek before further use;
            // surfacing a different exception here would only obscure the
            // failure mode.
            try { BT::m_kernel.seek(saved_dev_pos); } catch (...) {} // NOLINT(bugprone-empty-catch)
            throw cvt_error("code_cvt::rseek fail: partial sequence");
        }

        m_accu_len = new_dev_pos / epc;
        m_cvt_kernel.init_state();
    }

    /// I/O 模式切换 / I/O mode switching

    /**
     * @lang{ZH}
     * 切换至输出（写入）模式。
     * 若已处于输出模式则为空操作。
     * 从输入模式切换时，编码转换状态须为初始状态；对于变长或状态依赖编码，
     * 还要求内部缓冲区为空（已到达 EOF）。
     *
     * @throws cvt_error 若不满足上述前置条件。
     * @endif
     *
     * @lang{EN}
     * Switch to output (writing) mode.
     * No-op if already in output mode.
     * When switching from input mode, the encoding conversion state must be in its
     * initial state; for variable-length or state-dependent encodings, the internal
     * buffer must also be empty (EOF reached).
     *
     * @throws cvt_error If the preconditions above are not met.
     * @endif
     */
    void switch_to_put()
        requires (cvt_cpt::support_io_switch<KernelType>)
    {
        BT::assert_not_tainted();
        switch(BT::m_io_status)
        {
        case io_status::output:
            return;
        case io_status::neutral:
            BT::m_kernel.switch_to_put();
            BT::m_io_status = io_status::output;
            return;
        default: // io_status::input
            if (!m_cvt_kernel.is_init_state())
                throw cvt_error("code_cvt::switch_to_put fail: internal state is not neutral");

            if (m_cvt_kernel.is_var_length() || m_cvt_kernel.is_state_dep())
            {
                if (!this->is_eof())
                    throw cvt_error("code_cvt::switch_to_put fail: internal buffer not empty");
            }
            BT::m_kernel.switch_to_put();
            BT::m_io_status = io_status::output;
            return;
        }
    }

    /**
     * @lang{ZH}
     * 切换至输入（读取）模式。
     * 若已处于输入模式则为空操作。
     * 从输出模式切换时，编码转换状态须为初始状态。
     *
     * @throws cvt_error 若不满足上述前置条件。
     * @endif
     *
     * @lang{EN}
     * Switch to input (reading) mode.
     * No-op if already in input mode.
     * When switching from output mode, the encoding conversion state must be
     * in its initial state.
     *
     * @throws cvt_error If the preconditions above are not met.
     * @endif
     */
    void switch_to_get()
        requires (cvt_cpt::support_io_switch<KernelType>)
    {
        BT::assert_not_tainted();
        switch(BT::m_io_status)
        {
        case io_status::input:
            return;
        case io_status::neutral:
            BT::m_kernel.switch_to_get();
            BT::m_io_status = io_status::input;
            return;
        default: // io_status::output
            if (!m_cvt_kernel.is_init_state())
                throw cvt_error("code_cvt::switch_to_get fail: converter not in initial state");
            BT::m_kernel.switch_to_get();
            BT::m_io_status = io_status::input;
            return;
        }
    }

private:
    /**
     * @lang{ZH}
     * 关闭流：将编码转换状态重置为初始状态，将 I/O 方向恢复为 `neutral`，
     * 并清除 BOS 标志和累计字符计数。
     * 由 `attach()` 和 `detach()` 在替换底层设备前内部调用。
     * @endif
     *
     * @lang{EN}
     * Close the stream: reset the encoding conversion state to its initial value,
     * revert the I/O direction to `neutral`, and clear both the BOS flag and the
     * accumulated character count.
     * Called internally by `attach()` and `detach()` before replacing the underlying device.
     * @endif
     */
    void close_stream()
    {
        m_cvt_kernel.init_state();
        BT::m_io_status = io_status::neutral;
        BT::m_is_bos_done = false;
        m_accu_len = 0;
    }

protected:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    codecvt_kernel<external_type, internal_type> m_cvt_kernel; ///< 编码转换内核实例 / Encoding conversion kernel instance.

private:
    size_t m_accu_len = 0; ///< 已处理的内部字符总数（逻辑位置）/ Total internal characters processed (logical position).
};

/**
 * @lang{ZH}
 * 编码转换工厂类的主模板前向声明（不提供通用实现）。
 * 仅为以下特化版本提供实现：
 * - `code_cvt_creator<char, TInt>`：创建基于区域设置的 `code_cvt` 实例。
 * - `code_cvt_creator<char8_t, TInt>`：创建 UTF-8 `code_cvt` 实例。
 * @endif
 *
 * @lang{EN}
 * Primary template forward declaration for the encoding converter factory class
 * (no generic implementation is provided). Only the following specializations
 * are implemented:
 * - `code_cvt_creator<char, TInt>`: Creates locale-based `code_cvt` instances.
 * - `code_cvt_creator<char8_t, TInt>`: Creates UTF-8 `code_cvt` instances.
 * @endif
 */
template <typename TExt, typename TInt>
class code_cvt_creator;

/**
 * @lang{ZH}
 * 基于区域设置（locale）的 `code_cvt` 工厂类（char <-> wchar_t/char32_t）。
 * 持有一个区域设置名称，并通过 `create()` 方法为给定底层内核生成 `code_cvt` 实例。
 * 满足 `cvt_creator` 概念。
 * @endif
 *
 * @lang{EN}
 * Factory class for locale-based `code_cvt` instances (char <-> wchar_t/char32_t).
 * Holds a locale name and creates `code_cvt` instances for a given underlying
 * kernel via the `create()` method. Satisfies the `cvt_creator` concept.
 * @endif
 */
template <typename TInt>
    requires std::is_same_v<TInt, wchar_t> ||
                (std::is_same_v<TInt, char32_t> &&
                 (sizeof(char32_t) == sizeof(wchar_t)) &&
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class code_cvt_creator<char, TInt>
{
public:
    using category = CvtCreatorCategory;

    /**
     * @lang{ZH}
     * 以指定的区域设置名称构造工厂实例。
     *
     * @param name 区域设置名称，将在每次调用 `create()` 时传递给所创建的 `code_cvt`。
     * @endif
     *
     * @lang{EN}
     * Construct the factory with the specified locale name.
     *
     * @param name Locale name to be passed to each `code_cvt` instance created by `create()`.
     * @endif
     */
    explicit code_cvt_creator(std::string name)
        : m_name(std::move(name)) {}

    /**
     * @lang{ZH}
     * 为指定的底层内核创建一个 `code_cvt` 实例。
     * 要求 `TKernel::internal_type` 为 `char`（由 `static_assert` 在编译期强制检查）。
     *
     * @tparam TKernel 底层 I/O 转换器类型，须满足 `io_converter` 概念。
     * @param  kernel  底层转换器实例（完美转发）。
     *
     * @return 已配置区域设置的 `code_cvt<TKernel, TInt>` 实例。
     * @endif
     *
     * @lang{EN}
     * Create a `code_cvt` instance for the specified underlying kernel.
     * Requires `TKernel::internal_type` to be `char` (enforced by `static_assert`
     * at compile time).
     *
     * @tparam TKernel Underlying I/O converter type, must satisfy the `io_converter` concept.
     * @param  kernel  Underlying converter instance (perfect-forwarded).
     *
     * @return A `code_cvt<TKernel, TInt>` instance configured with the stored locale name.
     * @endif
     */
    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        static_assert(std::is_same_v<typename TKernel::internal_type, char>);
        return code_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_name};
    }
private:
    std::string m_name; ///< 区域设置名称 / Locale name.
};

/**
 * @lang{ZH}
 * UTF-8 编码的 `code_cvt` 工厂类（char8_t <-> char32_t/wchar_t）。
 * 无状态，不需要区域设置。通过 `create()` 方法为给定底层内核生成 `code_cvt` 实例。
 * 满足 `cvt_creator` 概念。
 * @endif
 *
 * @lang{EN}
 * Factory class for UTF-8 `code_cvt` instances (char8_t <-> char32_t/wchar_t).
 * Stateless; no locale is required. Creates `code_cvt` instances for a given
 * underlying kernel via the `create()` method. Satisfies the `cvt_creator` concept.
 * @endif
 */
template <typename TInt>
    requires std::is_same_v<TInt, char32_t> ||
                (std::is_same_v<TInt, wchar_t> &&
                 (sizeof(char32_t) == sizeof(wchar_t)) &&
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class code_cvt_creator<char8_t, TInt>
{
public:
    using category = CvtCreatorCategory;
    code_cvt_creator() = default;

    /**
     * @lang{ZH}
     * 为指定的底层内核创建一个 UTF-8 `code_cvt` 实例。
     * 要求 `TKernel::internal_type` 为 `char8_t`（由 `static_assert` 在编译期强制检查）。
     *
     * @tparam TKernel 底层 I/O 转换器类型，须满足 `io_converter` 概念。
     * @param  kernel  底层转换器实例（完美转发）。
     *
     * @return 使用无状态 UTF-8 内核的 `code_cvt<TKernel, TInt>` 实例。
     * @endif
     *
     * @lang{EN}
     * Create a UTF-8 `code_cvt` instance for the specified underlying kernel.
     * Requires `TKernel::internal_type` to be `char8_t` (enforced by `static_assert`
     * at compile time).
     *
     * @tparam TKernel Underlying I/O converter type, must satisfy the `io_converter` concept.
     * @param  kernel  Underlying converter instance (perfect-forwarded).
     *
     * @return A `code_cvt<TKernel, TInt>` instance using the stateless UTF-8 kernel.
     * @endif
     */
    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        static_assert(std::is_same_v<typename TKernel::internal_type, char8_t>);
        return code_cvt<TKernel, TInt>{std::forward<TKernel>(kernel)};
    }
};
}
