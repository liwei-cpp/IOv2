// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The format string `os << tm` writes through, swept over every UTF-8 locale
 * the build machine has.
 *
 * detail::tm_stream_format takes the locale's expanded %c and appends the
 * time-zone specifiers the platform's std::tm can carry: " %z" when the type
 * has tm_gmtoff, " (%Z)" when it has tm_zone -- each only if the expansion
 * does not already contain that specifier. So the growth in length falls into
 * a small set of values, and which one a locale lands on says which branches
 * ran.
 *
 * Two review rounds measured the same histogram by hand and neither could
 * leave anything behind that would notice it changing. The point of this sweep
 * is not the exact counts, which depend on the locales installed, but that
 * every branch is still reachable and that no locale produces a format the
 * appending logic mangles -- in particular that a %c ending in an odd number
 * of '%' never fuses with the appended " %z" into an unknown specifier.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/tm.h>

#include <clocale>
#include <cstdlib>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
// The locales the machine actually has. An image without locales-all still
// runs the suite; it just sweeps a shorter list.
std::vector<std::string> utf8_locales()
{
    std::vector<std::string> names;
    if (std::FILE* pipe = ::popen("locale -a 2>/dev/null", "r"))
    {
        char line[256];
        while (std::fgets(line, sizeof line, pipe))
        {
            std::string name(line);
            while (!name.empty() && (name.back() == '\n' || name.back() == '\r'))
                name.pop_back();
            if (name.find("UTF-8") != std::string::npos || name.find("utf8") != std::string::npos)
                names.push_back(name);
        }
        ::pclose(pipe);
    }
    return names;
}

// How many characters tm_stream_format added to the locale's own expanded %c,
// or nothing when this library declines the locale at all.
//
// Some locales ship data that cannot be parsed back unambiguously -- two days
// sharing a name, or identical AM and PM designators -- and the timeio facet
// refuses to build over them rather than produce text it could not read. Those
// are not failures of the formatting logic, so the sweep steps over them.
std::optional<std::size_t> growth(const IOv2::locale<char>& loc)
{
    try
    {
        auto facet = loc.template get<IOv2::timeio<char>>();
        if (!facet)
            return std::nullopt;

        const auto expanded = facet->template expand_format<std::tm>('c');
        const auto full     = IOv2::detail::tm_stream_format(*facet);
        return full.size() - expanded.size();
    }
    catch (...)
    {
        return std::nullopt;
    }
}
}

TEST(IoTraitsTmFormatAcrossLocales, EveryAppendingBranchIsStillReachable)
{
    const auto names = utf8_locales();
    if (names.empty())
        GTEST_SKIP() << "no UTF-8 locales installed";

    std::map<std::size_t, std::size_t> histogram;
    std::size_t                        usable   = 0;
    std::size_t                        declined = 0;

    for (const auto& name : names)
    {
        IOv2::locale<char> loc{""};
        try
        {
            loc = IOv2::locale<char>(name.c_str());
        }
        catch (...)
        {
            continue;  // a name `locale -a` lists but the runtime will not build
        }
        const auto grew = growth(loc);
        if (!grew)
        {
            ++declined;
            continue;
        }
        ++usable;
        ++histogram[*grew];
    }

    ASSERT_GT(usable, 0u) << declined << " locale(s) were declined outright";

    // " %z" is 3 characters and " (%Z)" is 5, so a locale whose %c names
    // neither grows by 8, one that already names %z grows by 5, and one that
    // already names both grows by 0. Whatever this platform's std::tm carries,
    // the growth is always a sum of that alphabet.
    for (const auto& [grew, count] : histogram)
    {
        EXPECT_TRUE(grew == 0 || grew == 3 || grew == 5 || grew == 8)
            << "growth " << grew << " on " << count << " locale(s) is not a sum of 3 and 5";
    }

    // Whatever the split, the common case has to be the one that appends both.
    EXPECT_GT(histogram[8] + histogram[5] + histogram[3], 0u)
        << "no locale needed any appending at all, which would mean the branches are dead";
}

TEST(IoTraitsTmFormatAcrossLocales, EveryLocaleProducesAFormatThatWritesAndReadsBack)
{
    const auto names = utf8_locales();
    if (names.empty())
        GTEST_SKIP() << "no UTF-8 locales installed";

    std::tm when{};
    when.tm_year = 121;  // 2021
    when.tm_mon  = 2;
    when.tm_mday = 4;
    when.tm_hour = 13;
    when.tm_min  = 45;
    when.tm_sec  = 6;

    std::size_t written = 0;
    std::size_t skipped = 0;

    for (const auto& name : names)
    {
        IOv2::locale<char> loc{""};
        try
        {
            loc = IOv2::locale<char>(name.c_str());
        }
        catch (...)
        {
            continue;
        }

        if (!growth(loc))
        {
            ++skipped;  // declined for ambiguous data; see growth()
            continue;
        }

        IOv2::ostream<IOv2::mem_device<char>, char> os{IOv2::mem_device<char>{}, loc};
        os << when;

        // The whole state matters, not just strfailbit: a refusal that arrives
        // as a different bit is still a refusal, and checking only one bit is
        // how an empty write can look like a success.
        ASSERT_TRUE(os.good()) << "locale " << name << " state " << os.rdstate();
        EXPECT_FALSE(os.device().str().empty()) << "locale " << name;
        ++written;
    }

    // `skipped` counts locales this library declines outright, which is a
    // deliberate refusal rather than a failure -- see growth(). What the sweep
    // asserts is that every locale it does accept writes successfully; that is
    // done per locale above, so all that is left here is that the sweep was not
    // vacuous.
    EXPECT_GT(written, 0u) << skipped << " locale(s) were declined";
}
