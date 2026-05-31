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
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <common/metafunctions.h>
#include <cvt/cvt_facilities.h>
#include <facet/facet_common.h>

#include <algorithm>
#include <array>
#include <compare>
#include <cstring>
#include <cuchar>
#include <cwchar>
#include <string>
#include <type_traits>
#include <vector>

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
 * 字符序列以空字符（`\0`）为段分隔符，各段依次独立处理。
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
 * `wcscoll`/`wcsxfrm`. Character sequences are processed segment by segment,
 * with null characters (`\0`) acting as segment delimiters.
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
    static constexpr size_t xfrm_failed = static_cast<size_t>(-1);

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
     * @throw cvt_error 若 `CharT` 为 `char8_t` 且 inter locale 的编码集不是 UTF-8。
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
     * @throw cvt_error If `CharT` is `char8_t` and the inter locale's codeset is not UTF-8.
     * @endif
     */
    collate_conf(const std::string& name)
        : ft_basic<collate<CharT>>()
        , m_inter_locale(name.c_str())
    {
        if constexpr (std::is_same_v<CharT, char8_t>)
        {
            struct Probe { const char* mb; size_t len; char32_t cp; };
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
                size_t n = std::mbrtoc32(&c32, p.mb, p.len, &st);
                if ((n != p.len) || (c32 != p.cp))
                    throw cvt_error("collate_conf<char8_t>: inter locale is not UTF-8");
            }
        }
    }

public:
    /**
     * @lang{ZH}
     * @brief 比较两个字符序列的排列顺序。
     *
     * 使用当前 locale 的 `strcoll`/`wcscoll` 逐段比较两个字符序列。
     * 序列以空字符（`\0`）为分隔符拆分为多个段，各段依次比较。
     * 若所有段均相等，则根据序列末尾是否存在额外的空字符来决定最终顺序。
     *
     * @param low1 第一个序列的起始指针。
     * @param high1 第一个序列的结束指针（不包含）。
     * @param low2 第二个序列的起始指针。
     * @param high2 第二个序列的结束指针（不包含）。
     * @return 表示两序列排列关系的 `std::strong_ordering` 值。
     * @endif
     *
     * @lang{EN}
     * @brief Compares the collation order of two character sequences.
     *
     * Compares two character sequences segment by segment using the locale's
     * `strcoll`/`wcscoll`. Each sequence is split into segments delimited by
     * null characters (`\0`), and segments are compared in order. If all
     * segments are equal, the final ordering is determined by whether an
     * extra null character is present at the end of either sequence.
     *
     * @param low1 Pointer to the start of the first sequence.
     * @param high1 Pointer to one past the end of the first sequence.
     * @param low2 Pointer to the start of the second sequence.
     * @param high2 Pointer to one past the end of the second sequence.
     * @return A `std::strong_ordering` value indicating the collation relationship.
     * @endif
     */
    virtual std::strong_ordering compare(const CharT* low1, const CharT* high1,
                                         const CharT* low2, const CharT* high2) const
    {
        std::vector<CharT> buf1; bool extra_eos1 = false;
        std::vector<CharT> buf2; bool extra_eos2 = false;

        clocale_user guard(m_inter_locale);

        while ((low1 != high1) && (low2 != high2))
        {
            const CharT* cl1 = low1;
            if (auto ch1 = std::find(low1, high1, static_cast<CharT>(0)); ch1 == high1)
            {
                auto data_len = high1 - low1;
                buf1.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low1, high1, buf1.data());
                buf1[data_len] = static_cast<CharT>(0);
                extra_eos1 = true;
                cl1 = buf1.data();
                low1 = high1;
            }
            else low1 = ch1 + 1;

            const CharT* cl2 = low2;
            if (auto ch2 = std::find(low2, high2, static_cast<CharT>(0)); ch2 == high2)
            {
                auto data_len = high2 - low2;
                buf2.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low2, high2, buf2.data());
                buf2[data_len] = static_cast<CharT>(0);
                extra_eos2 = true;
                cl2 = buf2.data();
                low2 = high2;
            }
            else low2 = ch2 + 1;

            int c_res = 0;
            if constexpr (std::is_same_v<CharT, char>)
                c_res = std::strcoll(cl1, cl2);
            else if constexpr (std::is_same_v<CharT, wchar_t>)
                c_res = std::wcscoll(cl1, cl2);
            else if constexpr (std::is_same_v<CharT, char8_t>)
                c_res = std::strcoll(reinterpret_cast<const char*>(cl1),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                     reinterpret_cast<const char*>(cl2));   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            else if constexpr (wchar_t_is_utf32)
            {
                if constexpr (std::is_same_v<CharT, char32_t>)
                    c_res = std::wcscoll(reinterpret_cast<const wchar_t*>(cl1),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                         reinterpret_cast<const wchar_t*>(cl2)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::compare is not implemented.");
            }
            else
                static_assert(dependent_false_v<CharT>, "collate_conf::compare is not implemented.");

            if (c_res < 0) return std::strong_ordering::less;
            if (c_res > 0) return std::strong_ordering::greater;
        }

        if (low1 != high1) return std::strong_ordering::greater;
        if (low2 != high2) return std::strong_ordering::less;
        if (extra_eos1 && !extra_eos2) return std::strong_ordering::less;
        if (!extra_eos1 && extra_eos2) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    /**
     * @lang{ZH}
     * @brief 计算排序键变换后所需的存储长度。
     *
     * 对字符序列中每个以空字符分隔的段，调用 `strxfrm`/`wcsxfrm`（传入空目标缓冲区）
     * 以获取各段变换后的长度，并将各段长度及段间分隔符累加后返回总长度。
     * 该返回值可用作 `transform()` 所需目标缓冲区的容量。
     *
     * @param low 字符序列的起始指针。
     * @param high 字符序列的结束指针（不包含）。
     * @return 存储完整排序键所需的字符数。
     * @throw cvt_error 若 `strxfrm`/`wcsxfrm` 报告失败。
     * @endif
     *
     * @lang{EN}
     * @brief Computes the storage length required for the collation-key transformation.
     *
     * For each null-delimited segment in the character sequence, calls
     * `strxfrm`/`wcsxfrm` with a null destination buffer to obtain the transformed
     * length of that segment. The total length, including inter-segment separators,
     * is accumulated and returned. The result can be used as the capacity for the
     * destination buffer passed to `transform()`.
     *
     * @param low Pointer to the start of the character sequence.
     * @param high Pointer to one past the end of the character sequence.
     * @return The number of characters required to store the complete collation key.
     * @throw cvt_error If `strxfrm`/`wcsxfrm` reports failure.
     * @endif
     */
    virtual size_t transform_length(const CharT* low, const CharT* high) const
    {
        size_t res = 0;
        std::vector<CharT> buf;

        clocale_user guard(m_inter_locale);
        while (low != high)
        {
            const CharT* cur = low;
            if (auto next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                auto data_len = high - low;
                buf.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low, high, buf.data());
                buf[data_len] = static_cast<CharT>(0);
                cur = buf.data();
                low = high;
            }
            else
            {
                low = next + 1;
                ++res;  // for the terminal character
            }

            size_t seg_len = 0;
            if constexpr (std::is_same_v<CharT, char>)
                seg_len = strxfrm(nullptr, cur, 0);
            else if constexpr (std::is_same_v<CharT, wchar_t>)
                seg_len = wcsxfrm(nullptr, cur, 0);
            else if constexpr (std::is_same_v<CharT, char8_t>)
                seg_len = strxfrm(nullptr, reinterpret_cast<const char*>(cur), 0);   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            else if constexpr (wchar_t_is_utf32)
            {
                if constexpr (std::is_same_v<CharT, char32_t>)
                    seg_len = wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(cur), 0); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::transform_length is not implemented.");
            }
            else
                static_assert(dependent_false_v<CharT>, "collate_conf::transform_length is not implemented.");

            if (seg_len == xfrm_failed)
                throw cvt_error("collate_conf::transform_length: strxfrm/wcsxfrm failed");
            res += seg_len;
        }

        return res;
    }

    /**
     * @lang{ZH}
     * @brief 将字符序列变换为不透明的排序键。
     *
     * 对字符序列中每个以空字符分隔的段，调用 `strxfrm`/`wcsxfrm` 将其变换为排序权重，
     * 并将结果写入 `dest`。输出的字节是保序的排序权重——对结果执行 `strcmp`/`wcscmp`
     * 等价于对原始输入执行 `strcoll`/`wcscoll`——并非可读的字符序列，也不应被解码。
     * 各段之间以空字符 `'\0'` 分隔以保持段间结构。
     *
     * @param low 字符序列的起始指针。
     * @param high 字符序列的结束指针（不包含）。
     * @param dest 写入排序键的目标缓冲区。
     * @param mx_len 最多写入的字符数；传入 `0` 表示不限制。
     * @return 实际写入 `dest` 的字符数。
     * @throw cvt_error 若 `strxfrm`/`wcsxfrm` 报告失败。
     * @endif
     *
     * @lang{EN}
     * @brief Transforms a character sequence into an opaque collation key.
     *
     * For each null-delimited segment in the character sequence, calls
     * `strxfrm`/`wcsxfrm` to produce order-preserving sort weights, and writes
     * the result into `dest`. The output bytes are collation weights — comparing
     * the result with `strcmp`/`wcscmp` reproduces the `strcoll`/`wcscoll` order
     * on the original input — and are not a readable or valid character sequence;
     * they must never be decoded. Segments are separated by null characters `'\0'`
     * to preserve inter-segment structure.
     *
     * @param low Pointer to the start of the character sequence.
     * @param high Pointer to one past the end of the character sequence.
     * @param dest Destination buffer where the collation key is written.
     * @param mx_len Maximum number of characters to write; pass `0` for unlimited.
     * @return The number of characters actually written to `dest`.
     * @throw cvt_error If `strxfrm`/`wcsxfrm` reports failure.
     * @endif
     */
    virtual size_t transform(const CharT* low, const CharT* high, CharT* dest, size_t mx_len = 0) const
    {
        size_t trans_count = 0;
        // All buffers are hoisted out of the loop so resize() reuses their
        // capacity instead of reallocating per segment.
        // buf  : input staging — null-terminated copy of a segment that has
        //        no embedded '\0' (so cur can point at a terminated string).
        // buf2 : output staging for strxfrm/wcsxfrm.
        std::vector<CharT> buf;
        std::vector<CharT> buf2;
        bool extra_eos = false;

        clocale_user guard(m_inter_locale);
        while ((low != high) && ((mx_len == 0) || (trans_count < mx_len)))
        {
            const CharT* cur = low;
            if (auto next = std::find(low, high, static_cast<CharT>(0)); next == high)
            {
                auto data_len = high - low;
                buf.resize(data_len + 1 + SIMD_PADDING_BYTES / sizeof(CharT));
                std::copy(low, high, buf.data());
                buf[data_len] = static_cast<CharT>(0);
                cur = buf.data();
                low = high;
                extra_eos = true;
            }
            else
                low = next + 1;

            {
                // strxfrm/wcsxfrm always append a terminating '\0' (writing
                // cur_trans + 1 elements). Transform into the reused buffer
                // first, then copy exactly cur_trans elements to dest, so the
                // appended '\0' is discarded. Writing it straight into dest
                // would overflow the caller's buffer by one on the trailing
                // segment, for which transform_length only accounts for
                // cur_trans (no terminator).
                size_t trans_len = 0;
                if constexpr (std::is_same_v<CharT, char>)
                    trans_len = strxfrm(nullptr, cur, 0);
                else if constexpr (std::is_same_v<CharT, wchar_t>)
                    trans_len = wcsxfrm(nullptr, cur, 0);
                else if constexpr (std::is_same_v<CharT, char8_t>)
                    trans_len = strxfrm(nullptr, reinterpret_cast<const char*>(cur), 0);   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                else if constexpr ((std::is_same_v<CharT, char32_t> &&
                                   wchar_t_is_utf32))
                    trans_len = wcsxfrm(nullptr, reinterpret_cast<const wchar_t*>(cur), 0); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::transform is not implemented.");

                if (trans_len == xfrm_failed)
                    throw cvt_error("collate_conf::transform: strxfrm/wcsxfrm failed");
                buf2.resize(trans_len + 1);

                size_t cur_trans = 0;
                if constexpr (std::is_same_v<CharT, char>)
                    cur_trans = strxfrm(buf2.data(), cur, buf2.size());
                else if constexpr (std::is_same_v<CharT, wchar_t>)
                    cur_trans = wcsxfrm(buf2.data(), cur, buf2.size());
                else if constexpr (std::is_same_v<CharT, char8_t>)
                    cur_trans = strxfrm(reinterpret_cast<char*>(buf2.data()),         // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                        reinterpret_cast<const char*>(cur),           // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                        buf2.size());
                else if constexpr ((std::is_same_v<CharT, char32_t> &&
                                   wchar_t_is_utf32))
                    cur_trans = wcsxfrm(reinterpret_cast<wchar_t*>(buf2.data()),      // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                        reinterpret_cast<const wchar_t*>(cur),        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                        buf2.size());
                else
                    static_assert(dependent_false_v<CharT>, "collate_conf::transform is not implemented.");

                if (cur_trans == xfrm_failed)
                    throw cvt_error("collate_conf::transform: strxfrm/wcsxfrm failed");
                if (mx_len != 0)
                    cur_trans = std::min(cur_trans, mx_len - trans_count);
                dest = std::copy(buf2.data(), buf2.data() + cur_trans, dest);
                trans_count += cur_trans;
            }

            if ((!extra_eos) && ((mx_len == 0) || (trans_count < mx_len)))
            {
                *dest++ = '\0';
                ++trans_count;
            }
        }
        return trans_count;
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
