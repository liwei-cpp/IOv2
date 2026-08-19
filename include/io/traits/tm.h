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

namespace IOv2
{
template <typename TChar>
struct parse_context_type<TChar, std::tm>
{
    using type = time_parse_context<TChar, true, true, tz_level::offset>;

    /**
     * @lang{ZH}
     * @brief 由一个 `std::tm` 构造出以其字段为回退值的解析上下文。
     *
     * 返回的上下文的日期与时间字段被预置为 @p tmb 中的对应值，因此随后 `get()` 未解析到的
     * 字段会保留 @p tmb 的取值，而不是退回默认构造所采用的挂钟时间。`std::tm` 只带得动部分
     * 时区信息（`tm_gmtoff` / `tm_zone`，还是实现定义的扩展），带不动区域身份，所以 `type`
     * 取 tz_level::offset 这一档：`%z` / `%Z` 能解析，但只有偏移会被写回 `tm_gmtoff`。
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
     * extensions) and no zone identity at all, so `type` sits at the `tz_level::offset` tier:
     * `%z` / `%Z` parse, but only the offset is written back, into `tm_gmtoff`.
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

        return mp->put(s, value, 'c');
    }
};

template <typename TChar>
struct io_traits<TChar, time_parse_context<TChar, true, true, tz_level::offset>>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, time_parse_context<TChar, true, true, tz_level::offset>& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        return mp->get(iter, iter_end, value, 'c');
    }
};
}
