#pragma once
#include <type_traits>
#include <io/io_base.h>
#include <io/fp_defs/base_fp.h>
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
    using type = time_parse_context<TChar, true, true, false>;

    /**
     * @lang{ZH}
     * @brief 由一个 `std::tm` 构造出以其字段为回退值的解析上下文。
     *
     * 返回的上下文的日期与时间字段被预置为 @p tmb 中的对应值，因此随后 `get()` 未解析到的
     * 字段会保留 @p tmb 的取值，而不是退回默认构造所采用的挂钟时间。`std::tm` 不含时区，
     * `type` 相应地也不激活时区字段组。
     *
     * 归一化只用 `std::chrono` 完成，**不经 `mktime()`**：后者依赖 `TZ` 环境变量、会因夏令时
     * 而平移小时数、写入 `tzset()` 的全局状态，且在部分实现上对 1970 年之前的时间直接失败——
     * 对一个仅用来提供回退值的纯变换来说，这些副作用都不可接受。归一化规则为：
     * - `tm_mon` 不限于 `0..11`，溢出部分按月进位并入年份；
     * - `tm_mday` 按"自当月 1 日起的天数偏移"处理，故 `0` 表示上月最后一天；
     * - `tm_sec == 60`（闰秒）取 `59`，因为 `hh_mm_ss` 无法表示它；
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
     * back to the wall-clock time a default-constructed context uses. A `std::tm` carries no
     * time zone, and `type` correspondingly leaves that field group inactive.
     *
     * Normalization is done purely with `std::chrono`, **not through `mktime()`**: that
     * function depends on the `TZ` environment variable, shifts the hour across a DST boundary,
     * writes the global state `tzset()` owns, and outright fails for pre-1970 times on some
     * implementations -- none of which is acceptable in a pure transformation that merely
     * supplies fallbacks. The rules are:
     * - `tm_mon` is not restricted to `0..11`; anything out of range carries into the year;
     * - `tm_mday` is treated as a day offset from the 1st of the month, so `0` denotes the last
     *   day of the previous month;
     * - `tm_sec == 60` (a leap second) becomes `59`, as `hh_mm_ss` cannot represent it;
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

        auto first = sys_days{year{static_cast<int>(norm_year)}
                              / month{static_cast<unsigned>(norm_mon) + 1} / day{1}};
        auto offset = std::clamp<std::int64_t>(static_cast<std::int64_t>(tmb.tm_mday) - 1,
                                              -4'000'000,
                                              4'000'000);
        auto sec = std::min(static_cast<std::int64_t>(tmb.tm_sec), std::int64_t{59});

        type ctx;
        ctx.set_hint(
            year_month_day{std::clamp(first + days{static_cast<int>(offset)}, cal_min, cal_max)});
        ctx.set_hint(hh_mm_ss<seconds>{hours{tmb.tm_hour} + minutes{tmb.tm_min} + seconds{sec}});
        return ctx;
    }
};


template <typename TChar>
struct writer<TChar, std::tm>
{
    template <typename TIter>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter swrite(TIter s, ios_base<TChar>& io, const locale<TChar>& loc, const std::tm& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        return mp->put(s, value, 'c');
    }
};

template <typename TChar>
struct reader<TChar, time_parse_context<TChar, true, true, false>>
{
    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter iter, TSent iter_end, ios_base<TChar>& io, const locale<TChar>& loc, time_parse_context<TChar, true, true, false>& value)
    {
        auto mp = loc.template get<timeio<TChar>>();
        if (!mp)
            throw stream_error("cannot get timeio facet");

        return mp->get(iter, iter_end, value, 'c');
    }
};
}
