/**
 * @file facet_helper.h
 * @lang{ZH}
 * facet 实现层共用的内部辅助工具集。
 *
 * 本文件提供以下辅助函数，供各 facet 具体实现（如 `numpunct`、`moneypunct` 等）内部使用：
 * - `string_to_char_convert`：将窄字符串的首字符转换为 `char`，用于读取 `lconv` /
 *   `nl_langinfo()` 中表示单个用户可见字符的字段。
 * - `string_to_widechar_convert`：将窄字符串的首字符转换为宽字符 `CharT`，适用场景同上。
 * - `add_grouping`：在字符序列中按分组规则插入分隔符，用于数字格式化输出。
 * - `adjust_grouping`：将原始 POSIX 风格的分组向量规范化为内部约定格式。
 * - `verify_grouping`：验证已解析的分组序列是否符合 `numpunct::grouping` 的规定。
 * @endif
 *
 * @lang{EN}
 * Internal utility helpers shared across facet implementations.
 *
 * This file provides the following helper functions for use by concrete facet
 * implementations (e.g. `numpunct`, `moneypunct`):
 * - `string_to_char_convert`: Converts the first character of a narrow string to
 *   `char`, for reading `lconv` / `nl_langinfo()` fields that represent a single
 *   user-visible character.
 * - `string_to_widechar_convert`: Converts the first character of a narrow string to
 *   a wide character `CharT`; applicable in the same contexts.
 * - `add_grouping`: Inserts grouping separators into a character sequence for
 *   formatted numeric output.
 * - `adjust_grouping`: Normalizes a raw POSIX-style grouping vector to the internal
 *   convention.
 * - `verify_grouping`: Validates a parsed grouping sequence against the
 *   `numpunct::grouping` specification.
 * @endif
 */

#pragma once
#include <common/clocale_wrapper.h>
#include <common/metafunctions.h>
#include <cvt/cvt_facilities.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace IOv2::FacetHelper
{
    /**
     * @lang{ZH}
     * 将（可能是多字节的）窄字符串 `narrow` 的第一个字符转换为给定 locale 下的 `char`，
     * 若 `narrow` 为空或转换结果不可表示则返回 `'\0'`。
     *
     * 适用于从 `lconv` / `nl_langinfo()` 读出的、语义上表示单个用户可见字符的字段
     * （例如 `decimal_point`、`thousands_sep`、`mon_decimal_point`、`mon_thousands_sep`）。
     *
     * @par 为何使用 `const std::string&` 而非 `const char*`
     * `lconv*` 和 `nl_langinfo()` 返回的指针可能被任何 `setlocale()` / `uselocale()` 调用所失效。
     * 下方转换过程会经由 `detail::to_wstring` 间接执行此类调用，因此输入必须是调用方
     * 持有的快照，而非指向 libc 所有 locale 数据的借用指针。
     * 使用 `std::string` 使这一契约在每个调用处都无法忽视。
     * @endif
     *
     * @lang{EN}
     * Convert the first character of a (possibly multibyte) narrow string
     * `narrow` to a `char` in the given locale, returning `'\0'` if `narrow`
     * is empty or conversion is not representable.
     *
     * Suitable for fields read out of `lconv` / `nl_langinfo()` that
     * semantically represent a single user-visible character (e.g.
     * `decimal_point`, `thousands_sep`, `mon_decimal_point`, `mon_thousands_sep`).
     *
     * @par Why `const std::string&` and not `const char*`
     * `lconv*` and `nl_langinfo()` pointers may be invalidated by any `setlocale()` /
     * `uselocale()` call. The conversion below transitively performs such
     * calls (via `detail::to_wstring`), so the input must be a caller-owned
     * snapshot rather than a borrowed pointer into the libc-owned locale
     * data. Taking `std::string` makes this contract unmissable at every
     * call site.
     * @endif
     *
     * @param narrow
     * @lang{ZH} 待转换的（可能是多字节的）窄字符串。 @endif
     * @lang{EN} The (possibly multibyte) narrow string to convert. @endif
     *
     * @param locale_name
     * @lang{ZH} 执行转换所使用的 locale 名称。 @endif
     * @lang{EN} The locale name under which to perform the conversion. @endif
     *
     * @return
     * @lang{ZH}
     * `narrow` 在给定 locale 下首字符对应的 `char`；若 `narrow` 为空或转换结果不可表示
     * 则返回 `'\0'`。
     * @endif
     * @lang{EN}
     * The first character of `narrow` converted to `char` in the given locale;
     * `'\0'` if `narrow` is empty or the conversion is not representable.
     * @endif
     */
    inline char string_to_char_convert(const std::string& narrow,
                                       const std::string& locale_name)
    {
        if (narrow.empty()) return '\0';
        if (narrow.size() == 1) return narrow[0];

        // Convert (multi-byte) narrow to wchar_t then narrow down
        std::wstring wide_str = detail::to_wstring(narrow.c_str(), locale_name);
        if (wide_str.empty()) return '\0';

        // Narrow the first wide character back to a single byte in the target
        // locale. This reproduces ctype_conf<wchar_t>::narrow() (wctob() under
        // a thread-local locale switch) directly, instead of building a whole
        // ctype_conf<wchar_t> facet whose 256-entry widen table and wctype
        // masks this path never reads. The out_of_wchar_range guard used there
        // is unnecessary here: the value is a wchar_t and is therefore always
        // representable as wchar_t.
        clocale_wrapper loc(locale_name.c_str());
        clocale_user guard(loc);
        const int c = wctob(static_cast<wint_t>(wide_str[0]));
        return c == EOF ? '\0' : static_cast<char>(c);
    }

    /**
     * @lang{ZH}
     * 将（可能是多字节的）窄字符串 `narrow` 的第一个字符转换为给定 locale 下的宽字符
     * `CharT`，若 `narrow` 为空或转换结果不含字符则返回 `default_char`。
     *
     * 适用于从 `lconv` / `nl_langinfo()` 读出的、语义上表示单个用户可见字符的字段
     * （例如 `decimal_point`、`thousands_sep`、`mon_decimal_point`、`mon_thousands_sep`）。
     *
     * @par `default_char` 契约
     * 在输入为空或转换结果为空的路径上原样返回；本函数**不**验证 `default_char` 本身
     * 在 `locale_name` 下是否可表示、格式是否正确或是否有意义。调用方负责选择合理的
     * 回退值（通常为 ASCII 范围内的普通字符，如 `L' '`、`L'.'`、`U' '`、`U'.'`）。
     * 注意与 `string_to_char_convert` 的不对称性：后者使用硬编码的 `'\0'` 回退且不接受
     * 调用方自定义；本函数将回退值作为参数，目的是让调用方可以选择适合 locale 的替代字符，
     * 但代价是验证责任由调用方自行承担。
     *
     * @par 为何使用 `const std::string&` 而非 `const char*`
     * `lconv*` 和 `nl_langinfo()` 返回的指针可能被任何 `setlocale()` / `uselocale()` 调用
     * 所失效。下方转换过程会经由 `detail::to_wstring`/`to_u32string` 间接执行此类调用，
     * 因此输入必须是调用方持有的快照，而非指向 libc 所有 locale 数据的借用指针。
     * @endif
     *
     * @lang{EN}
     * Convert the first character of a (possibly multibyte) narrow string
     * `narrow` to a wide character `CharT` in the given locale, returning
     * `default_char` when `narrow` is empty or conversion produces no
     * characters.
     *
     * Suitable for fields read out of `lconv` / `nl_langinfo()` that
     * semantically represent a single user-visible character (e.g.
     * `decimal_point`, `thousands_sep`, `mon_decimal_point`, `mon_thousands_sep`).
     *
     * @par `default_char` contract
     * Returned verbatim on the empty-input and empty-conversion paths; this
     * function does NOT validate that `default_char` is itself representable,
     * well-formed, or otherwise meaningful under `locale_name`. Callers are
     * responsible for choosing a sensible fallback (typically a plain ASCII-range
     * value such as `L' '`, `L'.'`, `U' '`, or `U'.'`). Note the asymmetry with
     * `string_to_char_convert`, which uses a hard-coded `'\0'` fallback and accepts
     * no caller default: here the fallback is a parameter precisely so callers can
     * pick a locale-appropriate substitute, but the price is that validation is
     * their responsibility, not ours.
     *
     * @par Why `const std::string&` and not `const char*`
     * `lconv*` and `nl_langinfo()` pointers may be invalidated by any `setlocale()` /
     * `uselocale()` call. The conversion below transitively performs such calls
     * (via `detail::to_wstring`/`to_u32string`), so the input must be a
     * caller-owned snapshot rather than a borrowed pointer into the
     * libc-owned locale data. Taking `std::string` makes this contract
     * unmissable at every call site.
     * @endif
     *
     * @tparam CharT
     * @lang{ZH}
     * 目标宽字符类型。须为 `wchar_t`，或在 `char32_t` 与 `wchar_t` 等价的平台上为
     * `char32_t`（由 `requires` 子句在编译期强制验证）。
     * @endif
     * @lang{EN}
     * The target wide character type. Must be `wchar_t`, or `char32_t` on platforms
     * where `char32_t` and `wchar_t` are equivalent (enforced at compile time by the
     * `requires` clause).
     * @endif
     *
     * @param narrow
     * @lang{ZH} 待转换的（可能是多字节的）窄字符串。 @endif
     * @lang{EN} The (possibly multibyte) narrow string to convert. @endif
     *
     * @param locale_name
     * @lang{ZH} 执行转换所使用的 locale 名称。 @endif
     * @lang{EN} The locale name under which to perform the conversion. @endif
     *
     * @param default_char
     * @lang{ZH} 当 `narrow` 为空或转换结果不含字符时返回的回退值。 @endif
     * @lang{EN} The fallback value returned when `narrow` is empty or conversion
     * produces no characters. @endif
     *
     * @return
     * @lang{ZH}
     * `narrow` 在给定 locale 下首字符对应的 `CharT`；若 `narrow` 为空或转换结果不含
     * 字符则返回 `default_char`。
     * @endif
     * @lang{EN}
     * The first character of `narrow` converted to `CharT` in the given locale;
     * `default_char` if `narrow` is empty or conversion produces no characters.
     * @endif
     */
    template <typename CharT>
        requires std::is_same_v<CharT, wchar_t> ||
            (std::is_same_v<CharT, char32_t> &&
                wchar_t_is_utf32)
    inline CharT string_to_widechar_convert(const std::string& narrow,
                                            const std::string& locale_name,
                                            CharT default_char)
    {
        if (narrow.empty()) return default_char;
        if constexpr (std::is_same_v<CharT, wchar_t>)
        {
            auto wide = detail::to_wstring(narrow.c_str(), locale_name);
            return wide.empty() ? default_char : wide[0];
        }
        else
        {
            auto wide = detail::to_u32string(narrow.c_str(), locale_name);
            return wide.empty() ? default_char : wide[0];
        }
    }

    /**
     * @lang{ZH}
     * 在字符序列 `[first, last)` 中按 `grouping` 规则插入分组分隔符 `sep`，将结果写入
     * 以 `s` 为起点的输出缓冲区，并返回写入结束位置的指针。
     *
     * @par 前置条件
     * - `grouping` 非空（Debug 模式下通过 `assert` 验证）。
     * - `[first, last)` 为合法区间，即 `first <= last`（Debug 模式下通过 `assert` 验证；
     *   若 `first > last`，内部复制循环将永不终止并越界写入输出缓冲区）。
     * - `s` 指向的输出缓冲区容量足以容纳 `(last - first)` 个字符加上每个非前导分组对应
     *   的一个分隔符；缓冲区大小由调用方负责保证（不做运行时验证）。
     * @endif
     *
     * @lang{EN}
     * Insert grouping separators `sep` into the character sequence `[first, last)`
     * according to `grouping`, write the result into the output buffer starting at
     * `s`, and return a pointer one past the last element written.
     *
     * @par Preconditions
     * - `grouping` is non-empty. [asserted in debug builds]
     * - `[first, last)` is a valid range, i.e. `first <= last`. Passing
     *   `first > last` causes the inner copy loop to never terminate and
     *   write past the output buffer. [asserted in debug builds]
     * - `s` points to an output buffer with sufficient capacity to hold
     *   `(last - first)` characters plus one separator per non-leading
     *   group; callers are responsible for sizing the buffer.
     *   [not validated]
     * @endif
     *
     * @tparam CharT
     * @lang{ZH} 序列元素及分隔符的字符类型。 @endif
     * @lang{EN} The character type of the sequence elements and separator. @endif
     *
     * @param s
     * @lang{ZH} 指向输出缓冲区起始位置的指针。 @endif
     * @lang{EN} Pointer to the start of the output buffer. @endif
     *
     * @param sep
     * @lang{ZH} 插入到各分组之间的分隔符字符。 @endif
     * @lang{EN} The separator character to insert between groups. @endif
     *
     * @param grouping
     * @lang{ZH}
     * 使用 `adjust_grouping()` 建立的内部 `uint8_t` 约定表示的分组规格：
     * 1–255 为显式分组大小，0 为唯一停止哨兵，最后一个 1–255 元素对所有后续分组隐式重复。
     * @endif
     * @lang{EN}
     * The grouping specification using the internal `uint8_t` convention established
     * by `adjust_grouping()`: 1–255 are explicit group sizes, 0 is the sole stop
     * sentinel, and the last 1–255 element is implicitly repeated for all remaining
     * groups.
     * @endif
     *
     * @param first
     * @lang{ZH} 输入字符序列的起始指针。 @endif
     * @lang{EN} Pointer to the start of the input character sequence. @endif
     *
     * @param last
     * @lang{ZH} 输入字符序列的末尾指针（独占上界）。 @endif
     * @lang{EN} Pointer to one past the end of the input character sequence. @endif
     *
     * @return
     * @lang{ZH} 指向写入输出缓冲区最后一个元素之后位置的指针。 @endif
     * @lang{EN} Pointer to one past the last element written into `s`. @endif
     */
    template <typename CharT>
    inline CharT* add_grouping(CharT* s, CharT sep, const std::vector<uint8_t>& grouping,
                               const CharT* first, const CharT* last)
    {
        assert(!grouping.empty());
        assert(first <= last);

        size_t idx = 0;
        size_t ctr = 0;
        const size_t max_idx = grouping.size() - 1;

        while (last - first > grouping[idx]
                && (grouping[idx] > 0))
        {
            last -= grouping[idx];
            idx < max_idx ? ++idx : ++ctr;
        }

        s = std::copy(first, last, s);
        first = last;

        const uint8_t n = grouping[idx];
        while (ctr--)
        {
            *s++ = sep;
            s = std::copy_n(first, n, s);
            first += n;
        }

        while (idx--)
        {
            *s++ = sep;
            const uint8_t m = grouping[idx];
            s = std::copy_n(first, m, s);
            first += m;
        }
        return s;
    }

    /**
     * @lang{ZH}
     * 将原始 POSIX 风格的分组向量原地规范化为内部约定格式；若向量不含可用约束则将其清空。
     *
     * @par 内部约定（`uint8_t`，平台无关）
     * - 1–255：显式分组大小。
     * - 0：停止标志——此点之后不再分组（**唯一**哨兵值）。
     * - 若最后一个元素为 1–255，则对所有后续分组隐式重复；若最后一个元素为 0，则显式停止优先。
     *
     * @par 对原始 POSIX 输入所做的转换
     * - **POSIX "0 = 重复前一分组"**：将 0 及其后的所有元素丢弃。POSIX 格式
     *   `[N1, ..., Nk, 0, ...]` 在本约定中变为 `[N1, ..., Nk]`，因为最后一个元素的
     *   隐式重复语义已完全覆盖该情形。
     * - **POSIX `CHAR_MAX`（= 停止）**：改写为内部 0。`CHAR_MAX` 在有符号 `char` 平台上
     *   为 127，在无符号 `char` 平台上为 255；经此步骤后调用方无需关心平台的 `char` 符号性。
     *
     * @note **顺序至关重要**：必须先去除 POSIX "0 = 重复" 标记，再执行 `CHAR_MAX → 0` 改写，
     * 否则新引入的内部停止哨兵会被误认为 POSIX 重复标记而被丢弃。
     * @endif
     *
     * @lang{EN}
     * Normalise a raw POSIX-style grouping vector in-place to the internal convention,
     * or clear it if it carries no usable constraint.
     *
     * @par Internal convention (`uint8_t`, platform-independent)
     * - 1–255: explicit group size.
     * - 0: stop — no further grouping after this point (the ONLY sentinel).
     * - The LAST element (if in 1–255) is implicitly repeated for all remaining groups.
     *   If the last element is 0, the explicit stop wins.
     *
     * @par Conversions applied to raw POSIX input
     * - **POSIX "0 = repeat previous"**: the 0 and everything after it is dropped.
     *   POSIX `[N1, ..., Nk, 0, ...]` becomes `[N1, ..., Nk]` in our convention,
     *   because last-element implicit repeat already covers that semantic exactly.
     * - **POSIX `CHAR_MAX` (= stop)**: rewritten to internal 0. `CHAR_MAX` is 127
     *   on signed-char platforms and 255 on unsigned-char platforms; callers do not
     *   need to know the platform's char signedness after this step.
     *
     * @note **Order matters**: stripping POSIX "0 = repeat" MUST happen before
     * the `CHAR_MAX → 0` rewrite, otherwise the newly-introduced internal stop
     * sentinel would be mistaken for a POSIX repeat-previous marker and dropped.
     * @endif
     *
     * @param grouping
     * @lang{ZH} 待原地规范化的原始 POSIX 分组向量。 @endif
     * @lang{EN} The raw POSIX grouping vector to normalize in place. @endif
     */
    inline void adjust_grouping(std::vector<uint8_t>& grouping)
    {
        // Step 1: POSIX "0 = repeat previous" → drop the 0 and the tail.
        auto zero = std::ranges::find(grouping, uint8_t{0});
        grouping.erase(zero, grouping.end());

        // Step 2: POSIX CHAR_MAX (= stop) → internal 0 sentinel, plus
        // canonical-form cleanup. After step 1 there are no 0s in the vector,
        // so the FIRST CHAR_MAX is the only point at which we introduce one,
        // and three cases fully cover the outcome:
        //   - at begin():   grouping would start with the stop sentinel,
        //                   i.e. no usable leading group ⇒ discard.
        //                   (This also subsumes the empty-vector case left
        //                   by step 1, since find on an empty range returns
        //                   end() == begin().)
        //   - in the tail:  rewrite to 0 and drop everything after it; those
        //                   bytes are dead code anyway because add_grouping
        //                   short-circuits on 0.
        //   - not found:    nothing to do.
        constexpr auto posix_sentinel =
            static_cast<uint8_t>(std::numeric_limits<char>::max());
        auto stop = std::ranges::find(grouping, posix_sentinel);
        if (stop == grouping.begin())
            grouping.clear();
        else if (stop != grouping.end())
        {
            *stop = 0;
            grouping.erase(std::next(stop), grouping.end());
        }
    }

    /**
     * @lang{ZH}
     * 验证已解析的分组序列 `grouping_tmp` 是否符合 `numpunct::grouping` 所规定的
     * `grouping` 约束。
     *
     * @par 前置条件
     * `grouping` 和 `grouping_tmp` 均非空；仅在 Debug 模式下通过 `assert` 验证，Release
     * 模式下由调用方保证。违反该前置条件将导致 `size() - 1` 下溢为 `SIZE_MAX`，后续索引
     * 越界读取。调用方可保证非空性，因为 `grouping_tmp` 仅在 `!grouping.empty()` 分支内
     * 填充，故 `grouping_tmp` 非空蕴含 `grouping` 非空。
     *
     * `grouping` 使用由 `adjust_grouping()` 建立的内部 `uint8_t` 约定：1–255 为显式分组
     * 大小，0 为唯一停止哨兵，最后一个 1–255 元素对所有后续分组隐式重复。
     * @endif
     *
     * @lang{EN}
     * Verify whether the parsed grouping sequence `grouping_tmp` conforms to the
     * `numpunct::grouping` constraint specified by `grouping`.
     *
     * @par Preconditions
     * Both `grouping` and `grouping_tmp` must be non-empty; non-emptiness is validated
     * only by `assert()` in debug builds; release builds rely on the caller. Violating
     * the precondition in release causes `size() - 1` to underflow to `SIZE_MAX` and
     * subsequent indexing to read out of bounds. Callers guarantee non-emptiness because
     * `grouping_tmp` is only populated inside `!grouping.empty()` branches, so the
     * non-emptiness of `grouping_tmp` implies the non-emptiness of `grouping`.
     *
     * `grouping` uses the internal `uint8_t` convention established by
     * `adjust_grouping()`: 1–255 are explicit group sizes, 0 is the sole stop
     * sentinel, and the last 1–255 element is implicitly repeated for all remaining
     * groups.
     * @endif
     *
     * @param grouping
     * @lang{ZH} 参考分组规格（来自 `numpunct::grouping`），使用内部 `uint8_t` 约定。 @endif
     * @lang{EN} The reference grouping specification (from `numpunct::grouping`),
     * using the internal `uint8_t` convention. @endif
     *
     * @param grouping_tmp
     * @lang{ZH} 解析过程中实际观测到的各分组大小序列。 @endif
     * @lang{EN} The grouping sizes actually observed during parsing. @endif
     *
     * @return
     * @lang{ZH} 若 `grouping_tmp` 符合 `grouping` 的约束则返回 `true`；否则返回 `false`。 @endif
     * @lang{EN} `true` if `grouping_tmp` conforms to `grouping`; `false` otherwise. @endif
     */
    inline bool verify_grouping(const std::vector<uint8_t>& grouping,
                                const std::vector<uint8_t>& grouping_tmp)
    {
        assert(!grouping.empty());
        assert(!grouping_tmp.empty());

        size_t i = grouping_tmp.size() - 1;
        const size_t min_val = std::min(i, grouping.size() - 1);
        bool test = true;

        // Parsed number groupings have to match the
        // numpunct::grouping string exactly, starting at the
        // right-most point of the parsed sequence of elements ...
        for (size_t j = 0; j < min_val && test; --i, ++j)
            test = grouping_tmp[i] == grouping[j];
        for (; i && test; --i)
            test = grouping_tmp[i] == grouping[min_val];
        // ... but the first parsed grouping can be <= numpunct grouping.
        // Skip this check when grouping[min_val] is 0 (stop sentinel,
        // no upper bound on the first parsed group).
        if (grouping[min_val] > 0)
            test &= grouping_tmp[0] <= grouping[min_val];
        return test;
    }
}
