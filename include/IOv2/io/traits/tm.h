// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file tm.h
 * @lang{ZH}
 * 让 `os << tm` 与 `is >> tm` 工作：为 `std::tm` 提供插入端的 `io_traits`，并经
 * `parse_context_type` 中转提供提取端。
 *
 * 提取不直接解析进 `std::tm`，而是先解析进 `time_parse_context`，再由后者的
 * `convert_to(std::tm&)` 写回——这正是 `IOv2/io/traits/traits_base.h` 里"解析上下文"一节
 * 描述的机制，`std::tm` 是它在库内的唯一用户。走这一层的原因是**回退值**：
 * `parse_context_type<TChar, std::tm>::make_parse_context` 用目标 `std::tm` 的现有字段
 * 铺好回退值，格式串未覆盖的字段于是保留旧值，而不是退回挂钟时间。
 *
 * 两个方向共用 `detail::tm_stream_format` 生成的格式串（展开后的 locale `%c`，按平台补
 * `%z` / `%Z`），因此写得出的一定读得回。标准里 `operator<<(std::tm)` 并不存在（只有
 * `put_time` / `get_time`），本库把它补上并让它遵守与其它插入器相同的字段宽度约定。
 * @endif
 *
 * @lang{EN}
 * Makes `os << tm` and `is >> tm` work: an insertion-side `io_traits` for `std::tm`, and an
 * extraction side routed through `parse_context_type`.
 *
 * Extraction does not parse into the `std::tm` directly; it parses into a `time_parse_context`
 * whose `convert_to(std::tm&)` then writes the result back -- exactly the mechanism described
 * under "Parse contexts" in `IOv2/io/traits/traits_base.h`, of which `std::tm` is the library's
 * only user. The reason for the extra layer is **fallbacks**:
 * `parse_context_type<TChar, std::tm>::make_parse_context` seeds the context from the target's
 * existing fields, so any field the format string does not cover keeps its old value instead of
 * reverting to the wall clock.
 *
 * Both directions share the format built by `detail::tm_stream_format` (the locale's `%c`,
 * expanded, with `%z` / `%Z` appended as the platform requires), so whatever is written can be
 * read back. The standard has no `operator<<(std::tm)` at all (only `put_time` / `get_time`);
 * this library adds one and makes it follow the same field-width contract as every other
 * inserter.
 * @endif
 */
#pragma once
#include <IOv2/common/defs.h>
#include <IOv2/facet/timeio.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/locale/locale.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>

namespace IOv2
{
/**
 * @lang{ZH}
 * @brief 把 `std::tm` 的提取改道到 `time_parse_context`，并用目标的现有字段作回退值。
 *
 * 这是 `traits_base.h` 所述"解析上下文"机制的实例：`type` 指定实际解析进的中转类型，
 * `make_parse_context` 从待写入的 `std::tm` 构造它。`operator>>` 先让
 * `io_traits<TChar, type>::sread` 解析进中转对象，再调用其 `convert_to(std::tm&)` 写回。
 *
 * @tparam TChar 流的字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief Reroutes extraction of a `std::tm` through a `time_parse_context`, seeded with the
 *        target's existing fields as fallbacks.
 *
 * An instance of the "parse context" mechanism described in `traits_base.h`: `type` names the
 * relay type actually parsed into, and `make_parse_context` builds it from the `std::tm` about
 * to be written. `operator>>` lets `io_traits<TChar, type>::sread` parse into the relay and
 * then calls its `convert_to(std::tm&)` to write the result back.
 *
 * @tparam TChar The stream's character type.
 * @endif
 */
template <typename TChar>
struct parse_context_type<TChar, std::tm>
{
    /**
     * @lang{ZH}
     * @brief 本上下文所处的时区档：由 @ref time_value_fields<std::tm> 的两个标志位直接决定。
     *
     * `std::tm` 的 `tm_gmtoff` 与 `tm_zone` 是一对实现定义的扩展，实践中要么都在、要么都不在，
     * 所以本判据实际只在 `zone` 与 `none` 之间取值，`offset` 那一支留给将来只有其中一个的平台。
     * 判据与 put 侧同源，因此两个方向不会脱节：都在时 `%z` / `%Z` 双向都真的工作，
     * 都不在时两侧一同退化为字面量。
     * @endif
     *
     * @lang{EN}
     * @brief The tier this context sits at, decided directly by the two flags in
     *        @ref time_value_fields<std::tm>.
     *
     * A `std::tm`'s `tm_gmtoff` and `tm_zone` are a pair of implementation-defined extensions
     * that are in practice either both present or both absent, so this picks between `zone` and
     * `none`; the `offset` arm is there for a platform that might one day carry only one of them.
     * The test is the one put uses, so the two directions cannot drift apart: with both members
     * `%z` and `%Z` really work both ways, with neither both sides degrade to literals together.
     * @endif
     */
    static constexpr tz_level tm_parse_tz_level =
        time_value_fields<std::tm>::has_zone   ? tz_level::zone
      : time_value_fields<std::tm>::has_offset ? tz_level::offset
                                               : tz_level::none;

    /**
     * @lang{ZH}
     * @brief 实际解析进的中转类型：带日期、带时间、时区档取 @ref tm_parse_tz_level 的
     *        `time_parse_context`。
     * @endif
     *
     * @lang{EN}
     * @brief The relay type actually parsed into: a `time_parse_context` with date, with time,
     *        and at the tier @ref tm_parse_tz_level names.
     * @endif
     */
    using type = time_parse_context<TChar, true, true, tm_parse_tz_level>;

    /**
     * @lang{ZH}
     * @brief 由一个 `std::tm` 构造出以其字段为回退值的解析上下文。
     *
     * 返回的上下文的日期与时间字段被预置为 @p tmb 中的对应值，因此随后 `get()` 未解析到的
     * 字段会保留 @p tmb 的取值——除非其它字段的归一化进位波及到它——而不是退回默认构造所采用
     * 的挂钟时间。时区档随平台而定，见 @ref tm_parse_tz_level "tm_parse_tz_level"。
     *
     * 归一化只用 `std::chrono` 完成，**不经 `mktime()`**（后者依赖 `TZ`、会因夏令时平移小时数、
     * 且写入全局状态）。规则为：
     * - `tm_mon` 不限于 `0..11`，溢出部分按月进位并入年份；
     * - `tm_mday` 按"自当月 1 日起的天数偏移"处理，故 `0` 表示上月最后一天；
     * - `tm_hour` / `tm_min` / `tm_sec` 同样不限于范围：三者先折成"当日秒数"，超出
     *   `[0, 24h)` 的部分按天进位或借位并入日期，余下部分才作为时刻。故
     *   `00:00:-5` 得到前一天的 `23:59:55`，`24:00:00` 得到次日的 `00:00:00`；
     * - `tm_sec == 60`（闰秒）取 `59`，因为 `hh_mm_ss` 无法表示它。这是时间组唯一的
     *   截断，其余越界值一律进位；
     * - 日期偏移（`tm_mday` 与时间组的进位之和）先被夹取到 ±4,000,000 天再叠到当月 1 日上，
     *   所得日期再被夹取到 `std::chrono::year` 可表示的日历范围内。故越界极远的 `tm_mday`
     *   停在离当月 1 日约 ±10,951 年处，而不是日历边界。
     *
     * @warning **@p tmb 必须是已初始化的对象**（例如 `std::tm t{}`）。那六项在解析开始前被
     *          **无条件读取**一次以铺好回退值，与格式串之后是否覆盖它们无关，故传入未初始化的
     *          `std::tm` 即为未定义行为（[basic.indet]/2 读取不确定值），即使格式串填满六项也一样。
     *          这一点与 `std::get_time` 不同——`std::time_get::get` 只写不读。`is >> tm` 与
     *          `get_time` 两条入口都经由本函数，故两者同受此约束；详见 `get_time` 的同名警告。
     * @param tmb 提供回退值的 `std::tm`；其 `tm_wday` / `tm_yday` / `tm_isdst` 不参与计算。
     * @return 已装入回退值的上下文。
     * @endif
     *
     * @lang{EN}
     * @brief Builds a parse context whose fallbacks are the fields of a `std::tm`.
     *
     * The returned context has its date and time fields pre-seeded from @p tmb, so any field a
     * subsequent `get()` does not parse keeps the value it had in @p tmb -- unless another
     * field's normalization carry reaches it -- rather than falling back to the wall-clock time
     * a default-constructed context uses. The time-zone tier follows the platform; see
     * @ref tm_parse_tz_level.
     *
     * Normalization is done purely with `std::chrono`, **not through `mktime()`** (which depends
     * on `TZ`, shifts the hour across a DST boundary and writes global state). The rules are:
     * - `tm_mon` is not restricted to `0..11`; anything out of range carries into the year;
     * - `tm_mday` is treated as a day offset from the 1st of the month, so `0` denotes the last
     *   day of the previous month;
     * - `tm_hour`, `tm_min` and `tm_sec` are likewise unrestricted: the three are folded into a
     *   second-of-day count, whatever falls outside `[0, 24h)` carries into (or borrows from)
     *   the date, and only the remainder becomes the time. So `00:00:-5` yields `23:59:55` on
     *   the previous day, and `24:00:00` yields `00:00:00` on the next one;
     * - `tm_sec == 60` (a leap second) becomes `59`, as `hh_mm_ss` cannot represent it. That is
     *   the only truncation in the time group; every other out-of-range value carries;
     * - the day offset (`tm_mday` plus whatever the time group carried) is clamped to
     *   ±4,000,000 days before it is added to the 1st of the month, and the resulting date is
     *   then clamped to the calendar range `std::chrono::year` can represent. A `tm_mday` far
     *   out of range therefore stops about ±10,951 years from the 1st of the month rather than
     *   at the calendar bound.
     *
     * @warning **@p tmb must be an initialized object** (a `std::tm t{}`, say). Those six fields
     *          are read **unconditionally** before parsing starts, to seed the fallbacks, whether
     *          or not the format string later overwrites them; passing an uninitialized `std::tm`
     *          is therefore undefined behavior ([basic.indet]/2, reading an indeterminate value)
     *          even when the format string fills in all six. This differs from `std::get_time`,
     *          where `std::time_get::get` only writes and never reads. Both `is >> tm` and
     *          `get_time` reach the seeding through this function, so the constraint applies to
     *          either spelling; see the matching warning on `get_time`.
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
        constexpr std::int64_t secs_per_day = std::int64_t{24} * 60 * 60;

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
 * 先把 locale 的 `%c` 用 @ref timeio::expand_format 展开，因为 `%z` 可能藏在 `%r` / `%X`
 * 这类复合说明符里，不展开就查不出来；随后按 `%z` 和 `%Z` 分别判断，缺哪个补哪个。
 *
 * **两个说明符各补各的。** `%z` 还原 `tm_gmtoff`，`%Z` 还原 `tm_zone`，谁都替不了谁：
 * 缩写定不出偏移（`CST` 同时是美中 −6、中国 +8、古巴 −5），偏移也给不出区名。所以 en_US 的
 * `%c` 自带 `%Z` 也照样要补 `%z`。补出来的 `%Z` 带括号、`%z` 不带，形如 `... +0800 (CST)`；
 * 已经带 `%Z` 的 locale 不再补，于是仍是 `... %Z %z`。
 *
 * **无条件补，不看这一个 `tm` 的取值**——`sread` 手上只有 `time_parse_context`，无从判断。
 * 两侧共用这一个与取值无关的格式串，才能保证写得出的一定读得回。平台的 `std::tm` 没有
 * `tm_gmtoff` / `tm_zone` 时两个都不补，补了只会在输出里留下 `%z` / `%Z` 这几个字符。
 *
 * @tparam TChar 流的字符类型。
 * @param tio 提供 locale 数据的 facet。
 * @return 供 `put` 与 `get` 共用的格式串。
 * @endif
 *
 * @lang{EN}
 * @brief The format `os << tm` and `is >> tm` share: an expanded `%c`, plus `%z` and `(%Z)`
 *        where those are needed.
 *
 * The locale's `%c` is first run through @ref timeio::expand_format, because a `%z` can sit
 * inside a compound such as `%r` or `%X` where no search would find it; `%z` and `%Z` are then
 * tested separately and whichever is missing is appended.
 *
 * **Each specifier is appended on its own.** `%z` restores `tm_gmtoff`, `%Z` restores
 * `tm_zone`, and neither stands in for the other: an abbreviation does not determine an offset
 * (`CST` is US Central −6, China +8 and Cuba −5 at once), and an offset does not give a zone
 * name. So en_US's `%c` already carrying a `%Z` still does not spare it the appended `%z`. An
 * appended `%Z` is parenthesized and an appended `%z` is not, giving `... +0800 (CST)`; a locale
 * that already has a `%Z` gets nothing appended for it and so stays `... %Z %z`.
 *
 * **It is appended unconditionally, not based on this particular `tm`** -- `sread` holds a
 * `time_parse_context` and no `std::tm` to inspect. Only one value-independent format, shared by
 * both sides, keeps whatever can be written readable. Nothing is appended when the platform's
 * `std::tm` has neither `tm_gmtoff` nor `tm_zone`, where appending would only put those
 * characters in the output.
 *
 * @tparam TChar The stream's character type.
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
            constexpr std::array<TChar, 3> tail{ static_cast<TChar>(' '), static_cast<TChar>('%'),
                                                 static_cast<TChar>('z') };
            fmt.append(tail.data(), tail.size());
        }
    }

    if constexpr (time_value_fields<std::tm>::has_zone)
    {
        if (!timeio<TChar>::contains_specifier(fmt, 'Z'))
        {
            constexpr std::array<TChar, 5> tail{ static_cast<TChar>(' '), static_cast<TChar>('('),
                                                 static_cast<TChar>('%'), static_cast<TChar>('Z'),
                                                 static_cast<TChar>(')') };
            fmt.append(tail.data(), tail.size());
        }
    }
    return fmt;
}
} // namespace detail

/**
 * @lang{ZH}
 * @brief `std::tm` 的插入端：`os << tm`。
 *
 * 只有 `swrite`。提取端不在这里——`parse_context_type<TChar, std::tm>` 把 `is >> tm` 改道到
 * `time_parse_context`，`sread` 因此定义在下面那个以中转类型为键的特化上。
 *
 * @tparam TChar 流的字符类型。
 * @endif
 *
 * @lang{EN}
 * @brief The insertion side for `std::tm`: `os << tm`.
 *
 * `swrite` only. The extraction side is not here: `parse_context_type<TChar, std::tm>` reroutes
 * `is >> tm` to a `time_parse_context`, so `sread` lives on the specialization below, keyed on
 * the relay type.
 *
 * @tparam TChar The stream's character type.
 * @endif
 */
template <typename TChar>
struct io_traits<TChar, std::tm>
{
    /**
     * @lang{ZH}
     * @brief 用 @ref detail::tm_stream_format 的格式串写出一个 `std::tm`，并按字段宽度补齐。
     *
     * @note 本插入器应用并消耗 `io.width()`，与本库其余插入器一致；`os << put_time(...)`
     *       则两样都不做（见 @ref put_time "put_time"）。分界与标准相同：`std::chrono` 的
     *       流插入器补齐并消耗，`std::put_time` 不。
     * @param s 输出迭代器。
     * @param io 提供宽度、填充字符与对齐标志的流。
     * @param loc 提供 `timeio<TChar>` facet 的 locale。
     * @param value 要写出的 `std::tm`；字段越界或 `tm_sec == 60` 由 facet 拒绝。
     * @return 写完之后的输出迭代器。
     * @throw stream_error 若 locale 中没有 `timeio<TChar>` facet、facet 拒绝该 `std::tm`，
     *        或所需填充量超过 `ios_defs::max_pad_count`。
     * @endif
     *
     * @lang{EN}
     * @brief Writes a `std::tm` with the format from @ref detail::tm_stream_format, padded to
     *        the field width.
     *
     * @note This inserter applies and consumes `io.width()`, as every other inserter in this
     *       library does; `os << put_time(...)` does neither (see @ref put_time). The split is
     *       the standard's: its `std::chrono` stream inserters pad and consume, `std::put_time`
     *       does not.
     * @param s The output iterator.
     * @param io The stream supplying width, fill character and adjustment flags.
     * @param loc The locale supplying the `timeio<TChar>` facet.
     * @param value The `std::tm` to write; out-of-range fields and `tm_sec == 60` are rejected
     *              by the facet.
     * @return The output iterator past what was written.
     * @throw stream_error If the locale carries no `timeio<TChar>` facet, the facet rejects the
     *        `std::tm`, or the required fill count exceeds `ios_defs::max_pad_count`.
     * @endif
     */
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, const std::tm& value)
    {
        auto width_guard = io.width_guard();
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        const auto fmt = detail::tm_stream_format(*mp);
        if (io.width() == 0)
            return mp->put(s, value, std::basic_string_view<TChar>(fmt));

        // Padding needs the length up front, and an expanded %c has no bounded one.
        std::basic_string<TChar> buf;
        mp->put(std::back_inserter(buf), value, std::basic_string_view<TChar>(fmt));
        return ostream_insert(s, io, buf.data(), buf.size());
    }
};

/**
 * @lang{ZH}
 * @brief 日期＋时间解析上下文的抽取实现，限定在本平台那一档上。
 *
 * 对 `TzLevel` 做偏特化而不钉死某一档，是因为档位随平台而定；`requires` 再把它收回到
 * `parse_context_type<TChar, std::tm>::tm_parse_tz_level`，即本平台**实际会产生**的那一档。
 * 收紧的理由是 `detail::tm_stream_format` 按平台而非按 `TzLevel` 生成格式串：两者同源，
 * 故对平台档恒一致；而显式写出离平台的档位会拿到不匹配的格式串，`%z` / `%Z` 被按字面量
 * 匹配而必然失配。有了这条约束，那种写法是**编译错误**，不是运行期静默失败。
 * 格式串与 `io_traits<TChar, std::tm>::swrite` 取自同一个函数，因此写出来的一定读得回。
 *
 * @tparam TChar 流的字符类型。
 * @tparam TzLevel 上下文的时区档；受 `requires` 约束，只能是本平台的那一档。
 * @endif
 *
 * @lang{EN}
 * @brief Extraction for a date-and-time parse context, restricted to this platform's tier.
 *
 * It is partially specialized on `TzLevel` rather than pinned to one tier because the tier is
 * chosen per platform; the `requires` then ties it back to
 * `parse_context_type<TChar, std::tm>::tm_parse_tz_level`, the tier this platform actually
 * produces. The restriction is there because `detail::tm_stream_format` builds its format from
 * the platform rather than from `TzLevel`: both read the same traits, so they always agree for
 * the platform's tier, whereas an explicitly named off-platform tier would get a format whose
 * `%z` / `%Z` are matched as literals and can only fail. With this constraint that spelling is a
 * **compile error** instead of a silent run-time failure. The format comes from the same function
 * `io_traits<TChar, std::tm>::swrite` uses, so whatever is written can be read back.
 *
 * @tparam TChar The stream's character type.
 * @tparam TzLevel The context's time-zone tier; the `requires` restricts it to this platform's.
 * @endif
 */
template <typename TChar, tz_level TzLevel>
    requires (TzLevel == parse_context_type<TChar, std::tm>::tm_parse_tz_level)
struct io_traits<TChar, time_parse_context<TChar, true, true, TzLevel>>
{
    /**
     * @lang{ZH}
     * @brief 用 @ref detail::tm_stream_format 的格式串把日期时间解析进上下文。
     *
     * 解析交给 locale 的 `timeio<TChar>` facet；解析到的字段写入 @p value，未解析到的保留
     * `make_parse_context` 铺好的回退值。与标准的 `get_time` 一致，本函数**不消费**
     * `width()`，`ios_base` 形参因此未被使用。
     *
     * @param iter 输入迭代器。
     * @param iter_end 输入哨位。
     * @param loc 提供 `timeio<TChar>` facet 的 locale。
     * @param value 接收解析结果的上下文；随后由 `operator>>` 调用其 `convert_to(std::tm&)`。
     * @return 指向最后一个被消费字符之后的输入迭代器。
     * @throw stream_error 若 locale 中没有 `timeio<TChar>` facet，或解析失败（格式不匹配、
     *        字段值越界）。
     * @endif
     *
     * @lang{EN}
     * @brief Parses a date and time into the context with the format from
     *        @ref detail::tm_stream_format.
     *
     * Parsing is done by the locale's `timeio<TChar>` facet; parsed fields are stored in
     * @p value, and the rest keep the fallbacks `make_parse_context` installed. As with the
     * standard's `get_time`, this function does **not** consume `width()`, which is why the
     * `ios_base` parameter goes unused.
     *
     * @param iter The input iterator.
     * @param iter_end The input sentinel.
     * @param loc The locale supplying the `timeio<TChar>` facet.
     * @param value The context receiving the parse; `operator>>` then calls its
     *              `convert_to(std::tm&)`.
     * @return An input iterator past the last consumed character.
     * @throw stream_error If the locale carries no `timeio<TChar>` facet, or parsing fails (format
     *        mismatch, field value out of range).
     * @endif
     */
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>&, const locale<TChar>& loc, time_parse_context<TChar, true, true, TzLevel>& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        const auto fmt = detail::tm_stream_format(*mp);
        return mp->get(iter, iter_end, value, std::basic_string_view<TChar>(fmt));
    }
};
}
