// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file facet_helper.h
 * @lang{ZH}
 * facet 实现层共用的内部辅助工具集。
 *
 * 本文件提供以下辅助函数，供各 facet 具体实现（如 `numpunct`、`moneypunct` 等）内部使用：
 * - `is_c_locale_name`：判断区域设置名称是否指代 C/POSIX 区域设置。
 * - `add_grouping`：在字符序列中按分组规则插入分隔符（可以是多个字符），用于数字格式化输出。
 * - `punct_distinct`：判断千位分隔符与小数点在输入里能否区分开。
 * - `take_punct` / `step_back`：在输入中识别（可能多字符的）千位分隔符或小数点，试探读过的
 *   字符不匹配时尽量退回。
 * - `adjust_grouping`：将原始 POSIX 风格的分组向量规范化为内部约定格式。
 * - `lconv_grouping`：把 `lconv` 的分组字节转换为内部约定，分隔符无法区分时不分组。
 * - `verify_grouping`：验证已解析的分组序列是否符合 `numpunct::grouping` 的规定。
 * @endif
 *
 * @lang{EN}
 * Internal utility helpers shared across facet implementations.
 *
 * This file provides the following helper functions for use by concrete facet
 * implementations (e.g. `numpunct`, `moneypunct`):
 * - `is_c_locale_name`: Tests whether a locale name designates the C/POSIX locale.
 * - `add_grouping`: Inserts grouping separators (possibly several characters long) into
 *   a character sequence for formatted numeric output.
 * - `punct_distinct`: Whether the thousands separator and the decimal point can be told
 *   apart in input.
 * - `take_punct` / `step_back`: Recognise a (possibly multi-character) thousands
 *   separator or decimal point in input, putting back what was read on trial if it does
 *   not match.
 * - `adjust_grouping`: Normalizes a raw POSIX-style grouping vector to the internal
 *   convention.
 * - `lconv_grouping`: Turns `lconv`'s grouping bytes into the internal convention, with
 *   no grouping where the separators cannot be told apart.
 * - `verify_grouping`: Validates a parsed grouping sequence against the
 *   `numpunct::grouping` specification.
 * @endif
 */

#pragma once
#include <IOv2/common/clocale_wrapper.h>
#include <IOv2/common/metafunctions.h>
#include <IOv2/cvt/cvt_facilities.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace IOv2::FacetHelper
{
    /**
     * @lang{ZH}
     * 判断区域设置名称 `name` 是否指代 C/POSIX 区域设置。
     *
     * POSIX 区域设置名称的形式为 `language[_territory][.codeset][@modifier]`。
     * `"C.UTF-8"`、`"C.utf8"`、`"C.UTF-8@euro"` 等拼写与 `"C"` 指代同一个区域设置，
     * 但精确比较无法识别它们：codeset 段在查表前会被规范化（去除非字母数字字符后转为
     * 小写），因此可接受的拼写不构成有限集合。本函数改为截取 `.` 与 `@` 之前的语言段
     * 再比较。语言段本身区分大小写（`"c"`、`"posix"` 均非合法名称），故不作大小写折叠。
     *
     * 光比较语言段还不够：`"C.BOGUS"`、`"C@euro"`、`"POSIX.utf8"` 的语言段都是 `C`/`POSIX`，
     * 而 `newlocale()` 并不接受它们。语言段匹配之后因此还要用 `clocale_wrapper` 探测一次，
     * 名称无法实例化就抛。校验放在这里而不是留给调用方的常规路径，是为了不依赖"每条常规
     * 路径都会自行构造 `clocale_wrapper`"这一隐含前提。`"C"` 与 `"POSIX"` 由标准保证存在，
     * 直接短路，无须探测；语言段不匹配的名称（如 `"en_US.BOGUS"`）不归本函数认领，一律
     * 返回 `false` 而不探测。
     * @endif
     *
     * @lang{EN}
     * Determine whether the locale name `name` designates the C/POSIX locale.
     *
     * POSIX locale names have the form `language[_territory][.codeset][@modifier]`.
     * Spellings such as `"C.UTF-8"`, `"C.utf8"` and `"C.UTF-8@euro"` designate the
     * same locale as `"C"`, but an exact comparison cannot recognize them: the
     * codeset segment is normalized before lookup (non-alphanumeric characters
     * stripped, then lowercased), so the set of accepted spellings is not finite.
     * This function compares the language segment — the part preceding `.` and `@`
     * — instead. That segment is itself case-sensitive (neither `"c"` nor
     * `"posix"` is a valid name), so no case folding is applied.
     *
     * Comparing the language segment alone is not enough: `"C.BOGUS"`, `"C@euro"` and
     * `"POSIX.utf8"` all have a `C`/`POSIX` language segment, yet `newlocale()` rejects
     * them. A matching segment is therefore followed by a `clocale_wrapper` probe, which
     * throws if the name cannot be instantiated. Validating here rather than leaving it to
     * the caller's regular path avoids depending on the implicit premise that every such
     * path constructs a `clocale_wrapper` of its own. `"C"` and `"POSIX"` are guaranteed to
     * exist by the standard and short-circuit without a probe; names whose language segment
     * does not match (say `"en_US.BOGUS"`) are not this function's to claim and return
     * `false` without being probed.
     * @endif
     *
     * @param name
     * @lang{ZH} 待检查的区域设置名称。 @endif
     * @lang{EN} The locale name to test. @endif
     *
     * @return
     * @lang{ZH} 若 `name` 的语言段为 `"C"` 或 `"POSIX"` 则返回 `true`，否则返回 `false`。 @endif
     * @lang{EN} `true` if the language segment of `name` is `"C"` or `"POSIX"`, `false`
     *           otherwise. @endif
     *
     * @throw io_error
     * @lang{ZH} 语言段为 `"C"` 或 `"POSIX"`，但 `newlocale()` 无法实例化该名称。 @endif
     * @lang{EN} The language segment is `"C"` or `"POSIX"` but `newlocale()` cannot
     *           instantiate the name. @endif
     */
    inline bool is_c_locale_name(const std::string& name)
    {
        if ((name == "C") || (name == "POSIX"))
            return true;

        const std::string_view head =
            std::string_view(name).substr(0, name.find_first_of(".@"));
        if ((head != "C") && (head != "POSIX"))
            return false;

        clocale_wrapper probe(name);
        return true;
    }

    /**
     * @lang{ZH}
     * 在字符序列 `[first, last)` 中按 `grouping` 规则插入分组分隔符 `sep`，将结果写入
     * 以 `out` 为起点的输出缓冲区，并返回写入结束位置的指针。
     *
     * @par 前置条件
     * - `grouping` 非空（Debug 模式下通过 `assert` 验证）。
     * - `[first, last)` 为合法区间，即 `first <= last`（Debug 模式下通过 `assert` 验证；
     *   若 `first > last`，`last - first` 会被转换为一个巨大的无符号宽度，导致越界写入）。
     * - `out` 指向的输出缓冲区容量足以容纳 `(last - first)` 个字符加上每个非前导分组对应
     *   的一个分隔符（`sep.size()` 个字符）；缓冲区大小由调用方负责保证（不做运行时验证）。
     *   `(last - first) * (1 + sep.size())` 总是够用。
     * @endif
     *
     * @lang{EN}
     * Insert grouping separators `sep` into the character sequence `[first, last)`
     * according to `grouping`, write the result into the output buffer starting at
     * `out`, and return a pointer one past the last element written.
     *
     * @par Preconditions
     * - `grouping` is non-empty. [asserted in debug builds]
     * - `[first, last)` is a valid range, i.e. `first <= last`. Passing
     *   `first > last` converts `last - first` into an enormous unsigned width
     *   and writes far past the output buffer. [asserted in debug builds]
     * - `out` points to an output buffer with sufficient capacity to hold
     *   `(last - first)` characters plus one separator (`sep.size()` characters)
     *   per non-leading group; callers are responsible for sizing the buffer.
     *   `(last - first) * (1 + sep.size())` is always enough. [not validated]
     * @endif
     *
     * @tparam CharT
     * @lang{ZH} 序列元素及分隔符的字符类型。 @endif
     * @lang{EN} The character type of the sequence elements and separator. @endif
     *
     * @param out
     * @lang{ZH} 指向输出缓冲区起始位置的指针。 @endif
     * @lang{EN} Pointer to the start of the output buffer. @endif
     *
     * @param sep
     * @lang{ZH} 插入到各分组之间的分隔符，可以是多个字符（如 UTF-8 的 U+202F）。 @endif
     * @lang{EN} The separator to insert between groups; it may be several characters long
     * (U+202F in UTF-8, say). @endif
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
     * @lang{EN} Pointer to one past the last element written into `out`. @endif
     */
    template <typename CharT>
    inline CharT* add_grouping(CharT* out, std::basic_string_view<CharT> sep,
                               const std::vector<uint8_t>& grouping,
                               const CharT* first, const CharT* last)
    {
        assert(!grouping.empty());
        assert(first <= last);

        const std::size_t last_rule = grouping.size() - 1;

        // Pass 1 -- measure. Peel whole groups off the low-order end for as long
        // as what is left over can still fill one more. `rule` records how far
        // the walk got into `grouping`; once it reaches the final entry that
        // entry keeps applying, and `extra` counts how many further times it did.
        // `head` is left holding the width of the leading, possibly short, group.
        std::size_t rule  = 0;
        std::size_t extra = 0;
        auto        head  = static_cast<std::size_t>(last - first);
        while (true)
        {
            const std::size_t size = grouping[rule];
            if (size == 0 || head <= size)
                break;
            head -= size;
            if (rule < last_rule)
                ++rule;
            else
                ++extra;
        }

        // One separator per peeled group, so the finished width is known up front.
        const std::size_t groups = rule + extra;
        const std::size_t width  = static_cast<std::size_t>(last - first) + groups * sep.size();

        // Pass 2 -- emit. Fill the buffer from its high-order end, which lets the
        // groups be replayed in the very order pass 1 peeled them off; rule k
        // governs the k-th group up from the bottom, saturating at `last_rule`.
        // `head >= 1` whenever `groups > 0`, so the leading separator write can
        // never run off the front of the buffer.
        CharT*       dst = out + width;
        const CharT* src = last;
        for (std::size_t k = 0; k < groups; ++k)
        {
            const std::size_t size = grouping[std::min(k, last_rule)];
            src -= size;
            dst -= size;
            std::copy_n(src, size, dst);
            dst -= sep.size();
            std::ranges::copy(sep, dst);
        }
        std::copy_n(first, head, out);
        return out + width;
    }

    /**
     * @lang{ZH}
     * 判断千位分隔符与小数点能否在输入里区分开：两者都非空，且谁也不是谁的前缀。
     *
     * 解析时逐字符同时比对两者，读到第一个不同的字符才判定是哪一个。若一个是另一个的前缀
     * （相等也算），较短的那个永远先匹配完，较长的那个就读不到了；此时只能关掉分组。
     * @endif
     *
     * @lang{EN}
     * Whether the thousands separator and the decimal point can be told apart in input:
     * both are non-empty, and neither is a prefix of the other.
     *
     * Parsing compares the input against both at once and settles on one at the first
     * character where they differ. Were one a prefix of the other (equality included),
     * the shorter would always match first and the longer could never be read, which
     * leaves turning grouping off as the only option.
     * @endif
     *
     * @return
     * @lang{ZH} 能区分开则为 `true`。 @endif
     * @lang{EN} `true` if they can be told apart. @endif
     */
    template <typename CharT>
    inline bool punct_distinct(std::basic_string_view<CharT> sep, std::basic_string_view<CharT> point)
    {
        if (sep.empty() || point.empty())
            return false;
        const std::size_t n = std::min(sep.size(), point.size());
        return sep.substr(0, n) != point.substr(0, n);
    }

    /**
     * @lang{ZH} `take_punct` 的结果。 @endif
     * @lang{EN} What `take_punct` found. @endif
     */
    enum class punct_kind : std::uint8_t
    {
        none,       ///< @lang{ZH} 不是标点；输入位置不变。 @endif @lang{EN} No punctuation; the input position is unchanged. @endif
        separator,  ///< @lang{ZH} 千位分隔符，已整个读过。 @endif @lang{EN} The thousands separator, read whole. @endif
        point,      ///< @lang{ZH} 小数点；是否读过见 `take_punct` 的 `consume_point`。 @endif @lang{EN} The decimal point; whether it was read, see `take_punct`'s `consume_point`. @endif
        torn        ///< @lang{ZH} 读了某个标点的开头几个字符后发现不是它，又无法退回；字段到此为止。 @endif @lang{EN} The first characters of a punctuation were read before it turned out not to be one, and could not be put back; the field ends here. @endif
    };

    /**
     * @lang{ZH}
     * 退回刚读过的 `k` 个字符 `units[0..k)`。双向迭代器后退；有 `putbackc` 的迭代器（IOv2 的
     * `ichannel_iterator`，压回区不设上限）按逆序压回；其他迭代器无法退回。
     * @endif
     * @lang{EN}
     * Puts back the `k` characters just read, `units[0..k)`. A bidirectional iterator steps
     * back; an iterator with `putbackc` (IOv2's `ichannel_iterator`, whose putback area is
     * unbounded) has them pushed back in reverse order; any other iterator cannot.
     * @endif
     *
     * @return
     * @lang{ZH} 已退回（或 `k` 为 0）则为 `true`。 @endif
     * @lang{EN} `true` if they were put back (or `k` is 0). @endif
     */
    template <typename TIter, typename CharT>
    inline bool step_back(TIter& beg, std::basic_string_view<CharT> units, std::size_t k)
    {
        if constexpr (std::bidirectional_iterator<TIter>)
        {
            std::ranges::advance(beg, -static_cast<std::iter_difference_t<TIter>>(k));
            return true;
        }
        else if constexpr (requires(TIter& it, CharT c) { it.putbackc(c); })
        {
            while (k != 0)
                beg.putbackc(units[--k]);
            return true;
        }
        else
            return k == 0;
    }

    /**
     * @lang{ZH}
     * 在输入的当前位置识别千位分隔符或小数点，两者都可以由多个字符组成。
     *
     * 逐字符同时比对两者，读到第一个不同的字符才判定；调用方须保证两者能区分开（见
     * `punct_distinct`），不参与识别的那个传空串。试探读过的字符若最终不是标点，就用
     * `step_back` 退回，数字照常在此结束：例如千位分隔符为 U+202F（UTF-8 `E2 80 AF`）时，
     * `100€`（€ 为 `E2 82 AC`）读出 100，`€` 留在输入里。迭代器无法退回时结果为
     * `punct_kind::torn`，已读的字符丢失。
     *
     * @param beg 输入的当前位置，随识别结果前进或退回。
     * @param end 输入的末尾。
     * @param sep 千位分隔符；不参与识别时为空串。
     * @param point 小数点；不参与识别时为空串。
     * @param consume_point 为 `false` 时，识别出的小数点不读过（多字符的小数点已读的部分
     *        尽量退回）——整数解析在小数点前停下，把它留在输入里。
     * @endif
     *
     * @lang{EN}
     * Recognises the thousands separator or the decimal point at the current input
     * position; either may be several characters long.
     *
     * The input is compared against both at once and settled at the first character where
     * they differ; the caller makes sure the two can be told apart (see `punct_distinct`)
     * and passes an empty string for one that does not take part. Characters read on trial
     * that turn out to be no punctuation are put back with `step_back`, and the number ends
     * there as usual: with U+202F (UTF-8 `E2 80 AF`) as the separator, `100€` (€ being
     * `E2 82 AC`) reads as 100 and leaves the `€` in the input. Where the iterator cannot
     * put them back the result is `punct_kind::torn`, and the characters read are lost.
     *
     * @param beg The current input position, moved on or put back as the result requires.
     * @param end The end of the input.
     * @param sep The thousands separator; empty when it does not take part.
     * @param point The decimal point; empty when it does not take part.
     * @param consume_point With `false`, a decimal point found is not read past (the part
     *        of a multi-character one already read is put back if possible): integer
     *        parsing stops in front of the point and leaves it in the input.
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent, typename CharT>
    inline punct_kind take_punct(TIter& beg, TSent end, std::basic_string_view<CharT> sep,
                                 std::basic_string_view<CharT> point, bool consume_point)
    {
        bool sep_alive   = !sep.empty();
        bool point_alive = !point.empty();
        std::size_t k = 0;
        while (beg != end)
        {
            const CharT c = *beg;
            const bool s = sep_alive && sep[k] == c;
            const bool p = point_alive && point[k] == c;
            if (!s && !p)
                break;
            sep_alive   = s;
            point_alive = p;

            // The point is settled at its last character, before that one is read.
            if (p && k + 1 == point.size())
            {
                if (consume_point)
                    ++beg;
                else
                    step_back(beg, point, k);
                return punct_kind::point;
            }
            ++beg;
            ++k;
            if (s && k == sep.size())
                return punct_kind::separator;
        }

        const std::basic_string_view<CharT> read = sep_alive ? sep : point;
        return step_back(beg, read, k) ? punct_kind::none : punct_kind::torn;
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
     * 把 `lconv` 的分组字节（`grouping` 或 `mon_grouping`）转换为内部约定（见
     * `adjust_grouping`）；千位分隔符与小数点无法区分时（见 `punct_distinct`，空分隔符也算）
     * 不分组，返回空向量。
     * @endif
     * @lang{EN}
     * Turns `lconv`'s grouping bytes (`grouping` or `mon_grouping`) into the internal
     * convention (see `adjust_grouping`); where the thousands separator and the decimal
     * point cannot be told apart (see `punct_distinct`; an empty separator counts), there is
     * no grouping and the vector comes back empty.
     * @endif
     */
    template <typename CharT>
    inline std::vector<uint8_t> lconv_grouping(std::string_view raw, std::basic_string_view<CharT> sep,
                                               std::basic_string_view<CharT> point)
    {
        std::vector<uint8_t> res;
        if (raw.empty() || !punct_distinct(sep, point))
            return res;
        res.reserve(raw.size());
        for (const char c : raw)
            res.push_back(static_cast<uint8_t>(c));
        adjust_grouping(res);
        return res;
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

        const std::size_t last_rule = grouping.size() - 1;

        // Walk the observed groups from the low-order end upwards, advancing a
        // cursor through the rules alongside. The cursor stalls on the final
        // rule, which is what makes that rule govern every group beyond the
        // explicit ones. Every group but the leading one has to be exactly as
        // wide as the rule facing it.
        std::size_t rule = 0;
        for (std::size_t k = grouping_tmp.size() - 1; k > 0; --k)
        {
            if (grouping_tmp[k] != grouping[rule])
                return false;
            if (rule < last_rule)
                ++rule;
        }

        // The cursor now faces the leading group, which is allowed to fall short
        // of its rule, and is unbounded above when that rule is the stop sentinel.
        return grouping[rule] == 0 || grouping_tmp[0] <= grouping[rule];
    }
}
