// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <type_traits>
#include <io/io_base.h>
#include <io/traits/traits_base.h>
#include <facet/timeio.h>
#include <locale/locale.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>

namespace IOv2
{
template <typename TChar>
struct parse_context_type<TChar, std::tm>
{
    /**
     * @lang{ZH}
     * @brief 本上下文所处的时区档：由 @ref time_value_fields<std::tm> 的两个标志位直接决定。
     *
     * 判据与 put 侧同源，因此两个方向不会脱节。`std::tm` 的 `tm_gmtoff` 与 `tm_zone` 是一对
     * 实现定义的扩展，要么都在、要么都不在（glibc 与 musl 把它们放在同一个特性宏下，BSD /
     * macOS 无条件都有，POSIX.1-2024 是把两个一起加进标准的），所以本判据实际只在 `zone`
     * 与 `none` 之间取值，`offset` 那一支留给将来可能出现的、只有其中一个的平台。
     *
     * 两个成员都在时，`%z` 与 `%Z` 两个方向都真的工作：put 由 `tm_gmtoff` 算出 `+0800`、把
     * `tm_zone` 原样写出，get 解析到的偏移与区名经 `convert_to(std::tm&)` 写回这两个成员。
     * 都不在时两侧一同退化为字面量——`do_put` 收到的 `zi` 是空指针，`%z` / `%Z` 原样写出；
     * 本档为 `none`，`do_get` 的两个说明符同样只匹配字面量。
     *
     * 若此处写死某一档，在成员缺失的平台上 put 会写出字面的 `%z`，get 却要求读到 `+0800`，
     * 往返就断了；而且解析出的值没有成员可以写回，纯属白做。
     * @endif
     *
     * @lang{EN}
     * @brief The tier this context sits at, decided directly by the two flags in
     *        @ref time_value_fields<std::tm>.
     *
     * The test is the one put uses, so the two directions cannot drift apart. A `std::tm`'s
     * `tm_gmtoff` and `tm_zone` are a pair of implementation-defined extensions that are either
     * both present or both absent (glibc and musl put them behind one feature macro, BSD and
     * macOS have both unconditionally, and POSIX.1-2024 added the two together), so in practice
     * this picks between `zone` and `none`; the `offset` arm is there for a platform that might
     * one day carry only one of them.
     *
     * With both members, `%z` and `%Z` really work both ways: put computes `+0800` from
     * `tm_gmtoff` and writes `tm_zone` out verbatim, and the offset and zone get parses are
     * written back to those members by `convert_to(std::tm&)`. With neither, both sides degrade
     * to literals together -- `do_put` receives a null `zi` and writes `%z` / `%Z` verbatim, and
     * at this `none` tier `do_get` likewise matches only the literals.
     *
     * Hard-coding a tier here would break the round trip on a platform lacking the members: put
     * would write a literal `%z` while get demanded a real `+0800`, and any value parsed would
     * have no member to be written back to.
     * @endif
     */
    static constexpr tz_level tm_parse_tz_level =
        time_value_fields<std::tm>::has_zone   ? tz_level::zone
      : time_value_fields<std::tm>::has_offset ? tz_level::offset
                                               : tz_level::none;

    using type = time_parse_context<TChar, true, true, tm_parse_tz_level>;

    /**
     * @lang{ZH}
     * @brief 由一个 `std::tm` 构造出以其字段为回退值的解析上下文。
     *
     * 返回的上下文的日期与时间字段被预置为 @p tmb 中的对应值，因此随后 `get()` 未解析到的
     * 字段会保留 @p tmb 的取值，而不是退回默认构造所采用的挂钟时间。`std::tm` 只带得动部分
     * 时区信息（`tm_gmtoff` / `tm_zone`，还是实现定义的扩展），所以 `type` 的档位随这两个
     * 成员而定：都在时是 tz_level::zone，`%z` / `%Z` 都解析、也都写得回去；都不在时退到
     * tz_level::none，两个说明符一律按字面量处理。见 @ref tm_parse_tz_level。
     *
     * 归一化只用 `std::chrono` 完成，**不经 `mktime()`**：后者依赖 `TZ` 环境变量、会因夏令时
     * 而平移小时数、写入 `tzset()` 的全局状态，且在部分实现上对 1970 年之前的时间直接失败——
     * 对一个仅用来提供回退值的纯变换来说，这些副作用都不可接受。归一化规则为：
     * - `tm_mon` 不限于 `0..11`，溢出部分按月进位并入年份；
     * - `tm_mday` 按"自当月 1 日起的天数偏移"处理，故 `0` 表示上月最后一天；
     * - `tm_hour` / `tm_min` / `tm_sec` 同样不限于范围：三者先折成"当日秒数"，超出
     *   `[0, 24h)` 的部分按天进位或借位并入日期，余下部分才作为时刻。故
     *   `00:00:-5` 得到前一天的 `23:59:55`，`24:00:00` 得到次日的 `00:00:00`；
     * - `tm_sec == 60`（闰秒）取 `59`，因为 `hh_mm_ss` 无法表示它。这是时间组唯一的
     *   截断，其余越界值一律进位；
     * - 结果被夹取到 `std::chrono::year` 可表示的日历范围内。
     *
     * @param tmb 提供回退值的 `std::tm`；其 `tm_wday` / `tm_yday` / `tm_isdst` 不参与计算。
     * @return 已装入回退值的上下文。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a parse context whose fallbacks are the fields of a `std::tm`.
     *
     * The returned context has its date and time fields pre-seeded from @p tmb, so any field a
     * subsequent `get()` does not parse keeps the value it had in @p tmb rather than falling
     * back to the wall-clock time a default-constructed context uses. A `std::tm` can carry
     * only part of a time zone (`tm_gmtoff` / `tm_zone`, and those are implementation-defined
     * extensions), so `type`'s tier follows those two members: with both it is
     * `tz_level::zone`, where `%z` and `%Z` both parse and both are written back; with neither
     * it drops to `tz_level::none`, where both specifiers are treated as literals. See
     * @ref tm_parse_tz_level.
     *
     * Normalization is done purely with `std::chrono`, **not through `mktime()`**: that
     * function depends on the `TZ` environment variable, shifts the hour across a DST boundary,
     * writes the global state `tzset()` owns, and outright fails for pre-1970 times on some
     * implementations -- none of which is acceptable in a pure transformation that merely
     * supplies fallbacks. The rules are:
     * - `tm_mon` is not restricted to `0..11`; anything out of range carries into the year;
     * - `tm_mday` is treated as a day offset from the 1st of the month, so `0` denotes the last
     *   day of the previous month;
     * - `tm_hour`, `tm_min` and `tm_sec` are likewise unrestricted: the three are folded into a
     *   second-of-day count, whatever falls outside `[0, 24h)` carries into (or borrows from)
     *   the date, and only the remainder becomes the time. So `00:00:-5` yields `23:59:55` on
     *   the previous day, and `24:00:00` yields `00:00:00` on the next one;
     * - `tm_sec == 60` (a leap second) becomes `59`, as `hh_mm_ss` cannot represent it. That is
     *   the only truncation in the time group; every other out-of-range value carries;
     * - the result is clamped to the calendar range `std::chrono::year` can represent.
     *
     * @param tmb The `std::tm` supplying the fallbacks; its `tm_wday`, `tm_yday`, and
     *            `tm_isdst` take no part in the computation.
     * @return The context with the fallbacks installed.
     * @endif
     */
    static type make_parse_context(const std::tm& tmb)
    {
        using namespace std::chrono;

        std::int64_t total_mon = (static_cast<std::int64_t>(tmb.tm_year) + 1900) * 12 + tmb.tm_mon;
        std::int64_t norm_year = total_mon / 12;
        std::int64_t norm_mon = total_mon % 12;
        if (norm_mon < 0)
        {
            norm_mon += 12;
            --norm_year;
        }

        norm_year = std::clamp<std::int64_t>(norm_year,
                                            static_cast<int>(year::min()),
                                            static_cast<int>(year::max()));

        constexpr sys_days cal_min{year::min() / January / 1};
        constexpr sys_days cal_max{year::max() / December / 31};
        constexpr std::int64_t secs_per_day = 24 * 60 * 60;

        // The time group is folded into one second-of-day count and normalized with the same
        // carry the date group uses, so an out-of-range hour/minute/second moves the date
        // instead of silently wrapping within it. A leap second is the one exception: it
        // becomes 59 because hh_mm_ss cannot represent it.
        std::int64_t sec = (tmb.tm_sec == 60) ? 59 : tmb.tm_sec;
        std::int64_t tod = static_cast<std::int64_t>(tmb.tm_hour) * 3600
                         + static_cast<std::int64_t>(tmb.tm_min) * 60
                         + sec;
        std::int64_t day_carry = (tod >= 0) ? tod / secs_per_day
                                            : -((secs_per_day - 1 - tod) / secs_per_day);

        auto first = sys_days{year{static_cast<int>(norm_year)}
                              / month{static_cast<unsigned>(norm_mon) + 1} / day{1}};
        auto offset = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(tmb.tm_mday) - 1 + day_carry, -4'000'000, 4'000'000);

        type ctx;
        ctx.set_hint(
            year_month_day{std::clamp(first + days{static_cast<int>(offset)}, cal_min, cal_max)});
        ctx.set_hint(hh_mm_ss<seconds>{seconds{tod - day_carry * secs_per_day}});
        return ctx;
    }
};


namespace detail
{
/**
 * @lang{ZH}
 * @brief `os << tm` 与 `is >> tm` 共用的格式串：展开后的 `%c`，必要时补 `%z` 与 `(%Z)`。
 *
 * 先把 locale 的 `%c` 用 @ref timeio::expand_format 展开——不为了过滤（`std::tm` 什么都
 * 供得出，没有说明符会被摘掉），而是为了**看得见**：`%z` 可能藏在 `%r` / `%X` 这类复合
 * 说明符里面，不展开就查不出来。随后 @ref timeio::contains_specifier 判断展开结果里有没有
 * `%z`，没有才补。
 *
 * **两个说明符各补各的，判据互相独立。** `std::tm` 的时区是两个成员，一个说明符还原一个：
 * `%z` 还原 `tm_gmtoff`，`%Z` 还原 `tm_zone`，谁都替不了谁。`%Z` 供不出偏移——`do_get` 的
 * `case 'Z'` 从不置 `m_have_offset`，而 `convert_to(std::tm&)` 仅在该标志为真时写
 * `tm_gmtoff`；缩写本身也定不出偏移（`CST` 同时是美中 −6、中国 +8、古巴 −5）。反过来 `%z`
 * 供不出区名。实测 `tm_gmtoff = 28800, tm_zone = "CST"` 的值：只走 `%Z` 读回来偏移是 `0`，
 * 只走 `%z` 读回来 `tm_zone` 是空指针。所以 en_US 的 `%c` 自带 `%Z` 也照样要补 `%z`。
 *
 * **补出来的 `%Z` 带括号，`%z` 不带。** 形如 `... +0800 (CST)`，与 RFC 5322 的日期一致，
 * 也与本机两个 locale 自带的 `%a %Y %b %d %H:%M:%S (%Z)` 一致。已经带 `%Z` 的 locale
 * （本机 165 个，其中 163 个裸写在末尾）不再补，于是它们仍是 `... %Z %z`——各 locale 的
 * 形状本就不同，这里只要求写得出的读得回来、且看着像样，不要求跨 locale 统一。
 *
 * **无条件补，不看这一个 `tm` 的取值。** 值级判断在这里做不到：`sread` 手上只有
 * `time_parse_context`，没有 `std::tm`，无从判断。若 put 值门控而 get 不门控，带时区的值写出
 * 的偏移会残留在流里污染下一次提取；若两边都门控，无时区的值写出的文本又喂不进要求偏移的
 * 格式串。两侧共用这一个与取值无关的格式串，才能保证写得出的一定读得回。
 *
 * 平台的 `std::tm` 没有 `tm_gmtoff` / `tm_zone` 时两个都不补：那种平台上 `do_put` 收到的
 * `zi` 是空指针，两个说明符都会退化成字面量，而
 * @ref parse_context_type<TChar, std::tm>::tm_parse_tz_level 也已降到 `tz_level::none`，
 * 补了只会在输出里留下 `%z` / `%Z` 这几个字符。
 *
 * @param tio 提供 locale 数据的 facet。
 * @return 供 `put` 与 `get` 共用的格式串。
 * @endif
 *
 * @lang{EN}
 * @brief The format `os << tm` and `is >> tm` share: an expanded `%c`, plus `%z` and `(%Z)`
 *        where those are needed.
 *
 * The locale's `%c` is first run through @ref timeio::expand_format -- not to filter anything
 * (a `std::tm` supplies every field, so no specifier is dropped) but to make the format
 * **visible**: a `%z` can sit inside a compound such as `%r` or `%X`, where no search would
 * find it. @ref timeio::contains_specifier then decides whether the expansion already has a
 * `%z`, and only if it does not is one appended.
 *
 * **Only `%z` is appended, never `%Z`,** because `%Z` contributes nothing to the reconstructed
 * `tm`: `do_get`'s `%Z` only fills `m_zone_abbrev` and never sets `m_have_offset`, while
 * `convert_to(std::tm&)` writes `tm_gmtoff` only when `m_have_offset` is set. So `"%F %T %Z"`
 * writes `... CST` and reads back with `tm_gmtoff` still `0`; `%z` alone round-trips. That also
 * makes "skip it when a `%Z` is already there" wrong: en_US's `%c` carries a `%Z` and still
 * loses the offset, so the test looks for `%z` and nothing else.
 *
 * **It is appended unconditionally, not based on this particular `tm`.** A value-level test is
 * impossible here: `sread` holds a `time_parse_context` and no `std::tm` to inspect. Were put
 * to gate on the value while get did not, the offset written for a zoned value would be left in
 * the stream to corrupt the next extraction; were both to gate, text written for a zone-less
 * value would not satisfy a format demanding an offset. Only one value-independent format,
 * shared by both sides, keeps whatever can be written readable.
 *
 * Nothing is appended when the platform's `std::tm` has neither `tm_gmtoff` nor `tm_zone`:
 * there `do_put` receives a null `zi` and degrades both specifiers to literals, and
 * @ref parse_context_type<TChar, std::tm>::tm_parse_tz_level has already dropped to
 * `tz_level::none`, so appending would only put those characters in the output.
 *
 * @param tio The facet supplying the locale data.
 * @return The format string shared by `put` and `get`.
 * @endif
 */
template <typename TChar>
std::basic_string<TChar> tm_stream_format(const timeio<TChar>& tio)
{
    std::basic_string<TChar> fmt = tio.template expand_format<std::tm>('c');

    if constexpr (time_value_fields<std::tm>::has_offset)
    {
        if (!timeio<TChar>::contains_specifier(fmt, 'z'))
        {
            const TChar tail[] = { static_cast<TChar>(' '), static_cast<TChar>('%'),
                                   static_cast<TChar>('z'), TChar() };
            fmt += tail;
        }
    }

    if constexpr (time_value_fields<std::tm>::has_zone)
    {
        if (!timeio<TChar>::contains_specifier(fmt, 'Z'))
        {
            const TChar tail[] = { static_cast<TChar>(' '), static_cast<TChar>('('),
                                   static_cast<TChar>('%'), static_cast<TChar>('Z'),
                                   static_cast<TChar>(')'), TChar() };
            fmt += tail;
        }
    }
    return fmt;
}
} // namespace detail

template <typename TChar>
struct io_traits<TChar, std::tm>
{
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, const std::tm& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        const auto fmt = detail::tm_stream_format(*mp);
        return mp->put(s, value, std::basic_string_view<TChar>(fmt));
    }
};

/**
 * @lang{ZH}
 * @brief 日期＋时间解析上下文的抽取实现，对全部三个时区档通用。
 *
 * 之所以对 `TzLevel` 做偏特化而不是钉死某一档：`parse_context_type<TChar, std::tm>::type`
 * 的档位取决于平台的 `std::tm` 有没有 `tm_gmtoff` / `tm_zone`（见
 * @ref parse_context_type<TChar, std::tm>::tm_parse_tz_level），只特化其中一档
 * 会让别的平台找不到 `io_traits`。三个档在这里的行为一致——都用
 * @ref detail::tm_stream_format 抽取——档位只影响 `do_get` 内部 `%z` / `%Z` 是真解析还是
 * 退化为字面量。
 *
 * 格式串与 `io_traits<TChar, std::tm>::swrite` 取自同一个函数，因此写出来的一定读得回。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction for a date-and-time parse context, common to all three time-zone tiers.
 *
 * Why this is partially specialized on `TzLevel` rather than pinned to one tier:
 * `parse_context_type<TChar, std::tm>::type` picks its tier from whether the platform's
 * `std::tm` has `tm_gmtoff` / `tm_zone` (see
 * @ref parse_context_type<TChar, std::tm>::tm_parse_tz_level), so specializing just one tier
 * would leave some other platform with no `io_traits` at all. All three tiers behave alike here -- extraction goes through
 * @ref detail::tm_stream_format -- the tier only decides whether `%z` / `%Z` really parse or
 * degrade to literals inside `do_get`.
 *
 * The format comes from the same function `io_traits<TChar, std::tm>::swrite` uses, so whatever
 * is written can be read back.
 * @endif
 */
template <typename TChar, tz_level TzLevel>
struct io_traits<TChar, time_parse_context<TChar, true, true, TzLevel>>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, time_parse_context<TChar, true, true, TzLevel>& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        const auto fmt = detail::tm_stream_format(*mp);
        return mp->get(iter, iter_end, value, std::basic_string_view<TChar>(fmt));
    }
};
}
