// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// Timeio port probe, tz database half: whether the platform's std::chrono has a tz
// database at all, what it holds, and what IOv2's %Z trie walk (base_ft<timeio>::
// s_timezone_tree, timeio_details.h) would cost and collect there. Not a test and not for
// main: it prints what it sees and always exits 0.
//   - the library's configuration macros, and whether the tzdb is declared
//   - get_tzdb(): version, zone and link counts, how long it takes
//   - sample zones: abbreviation and offset now and at a few past instants
//   - IOv2's walk: every zone from year::min() to max(now + 10y, 2038), timed; the distinct
//     abbreviations, which of them are not ASCII or look like offsets
//   - Windows: the ICU default locale, and the abbreviations under other ICU locales
//     (the MSVC STL asks ICU for them in the default locale)
// Built with -DPROBE_FORCE_TZDB=1 it uses the tzdb even where the macros say it is absent,
// so that a compile error answers the question.
// Windows: cl /std:c++latest /utf-8 /EHsc /W4 tzdb_probe.cpp
// POSIX:   c++ -std=c++20 tzdb_probe.cpp [-fexperimental-library]

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define PROBE_STR2(x) #x
#define PROBE_STR(x) PROBE_STR2(x)

#if PROBE_FORCE_TZDB
#define PROBE_HAS_TZDB 1
#elif defined(_LIBCPP_VERSION)
// LLVM 19 and older spell it negatively, 20 and later as 0/1 macros.
#if defined(_LIBCPP_HAS_NO_TIME_ZONE_DATABASE) || defined(_LIBCPP_HAS_NO_EXPERIMENTAL_TZDB)
#define PROBE_HAS_TZDB 0
#elif defined(_LIBCPP_HAS_TIME_ZONE_DATABASE) && defined(_LIBCPP_HAS_EXPERIMENTAL_TZDB)
#define PROBE_HAS_TZDB (_LIBCPP_HAS_TIME_ZONE_DATABASE && _LIBCPP_HAS_EXPERIMENTAL_TZDB)
#else
#define PROBE_HAS_TZDB 0
#endif
#elif defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
#define PROBE_HAS_TZDB 1
#else
#define PROBE_HAS_TZDB 0
#endif

namespace
{
std::string show(std::string_view s)
{
    std::string r = "\"";
    for (const char ch : s)
    {
        const auto c = static_cast<unsigned char>(ch);
        char buf[8];
        if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') { buf[0] = static_cast<char>(c); buf[1] = 0; }
        else std::snprintf(buf, sizeof buf, "\\x%02X", c);
        r += buf;
    }
    return r + "\"";
}

void print_config()
{
    std::printf("configuration:\n");
#if defined(__cpp_lib_chrono)
    std::printf("  __cpp_lib_chrono = %ld\n", static_cast<long>(__cpp_lib_chrono));
#else
    std::printf("  __cpp_lib_chrono undefined\n");
#endif
#if defined(_LIBCPP_VERSION)
    std::printf("  _LIBCPP_VERSION = %d\n", _LIBCPP_VERSION);
#endif
#if defined(_LIBCPP_HAS_TIME_ZONE_DATABASE)
    std::printf("  _LIBCPP_HAS_TIME_ZONE_DATABASE = %s\n", PROBE_STR(_LIBCPP_HAS_TIME_ZONE_DATABASE));
#endif
#if defined(_LIBCPP_HAS_NO_TIME_ZONE_DATABASE)
    std::printf("  _LIBCPP_HAS_NO_TIME_ZONE_DATABASE defined\n");
#endif
#if defined(_LIBCPP_HAS_EXPERIMENTAL_TZDB)
    std::printf("  _LIBCPP_HAS_EXPERIMENTAL_TZDB = %s\n", PROBE_STR(_LIBCPP_HAS_EXPERIMENTAL_TZDB));
#endif
#if defined(_LIBCPP_HAS_NO_EXPERIMENTAL_TZDB)
    std::printf("  _LIBCPP_HAS_NO_EXPERIMENTAL_TZDB defined\n");
#endif
#if defined(_LIBCPP_HAS_EXPERIMENTAL_LIBRARY)
    std::printf("  _LIBCPP_HAS_EXPERIMENTAL_LIBRARY = %s\n", PROBE_STR(_LIBCPP_HAS_EXPERIMENTAL_LIBRARY));
#endif
#if defined(_LIBCPP_AVAILABILITY_HAS_TZDB)
    std::printf("  _LIBCPP_AVAILABILITY_HAS_TZDB = %s\n", PROBE_STR(_LIBCPP_AVAILABILITY_HAS_TZDB));
#endif
#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
    std::printf("  macOS deployment target = %d\n", __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__);
#endif
#if defined(__GLIBCXX__)
    std::printf("  __GLIBCXX__ = %ld\n", static_cast<long>(__GLIBCXX__));
#endif
#if defined(_MSVC_STL_VERSION)
    std::printf("  _MSVC_STL_VERSION = %d, _MSVC_STL_UPDATE = %ld\n", _MSVC_STL_VERSION, static_cast<long>(_MSVC_STL_UPDATE));
#endif
    std::printf("  tzdb used by this build: %s\n", PROBE_HAS_TZDB ? "yes" : "no");
}

#if PROBE_HAS_TZDB
using namespace std::chrono;

double ms_since(steady_clock::time_point t0)
{
    return duration<double, std::milli>(steady_clock::now() - t0).count();
}

std::string info_line(const sys_info& i)
{
    char buf[160];
    std::snprintf(buf, sizeof buf, "abbrev %s offset %+llds save %lldmin", show(i.abbrev).c_str(),
                  static_cast<long long>(i.offset.count()), static_cast<long long>(duration_cast<minutes>(i.save).count()));
    return buf;
}

const char* const sample_zones[] = {
    "UTC", "America/New_York", "America/Chicago", "America/Los_Angeles", "America/Sao_Paulo", "Europe/London",
    "Europe/Berlin", "Europe/Moscow", "Africa/Cairo", "Asia/Kolkata", "Asia/Shanghai", "Asia/Tokyo",
    "Australia/Sydney", "Pacific/Auckland", "CST6CDT", "EST",
};

void samples(const tzdb& db)
{
    std::printf("sample zones (now | 1900-01-01 | 1970-01-01 | 2000-07-01):\n");
    const sys_seconds now = time_point_cast<seconds>(system_clock::now());
    const sys_seconds past[] = {sys_days{1900y / January / 1}, sys_days{1970y / January / 1}, sys_days{2000y / July / 1}};
    for (const char* name : sample_zones)
    {
        try
        {
            const time_zone* z = db.locate_zone(name);
            std::printf("  %-20s -> %s\n    now  %s\n", name, std::string(z->name()).c_str(), info_line(z->get_info(now)).c_str());
            for (const auto& t : past)
            {
                try { std::printf("    past %s\n", info_line(z->get_info(t)).c_str()); }
                catch (const std::exception& e) { std::printf("    past throws: %s\n", e.what()); }
            }
        }
        catch (const std::exception& e)
        {
            std::printf("  %-20s throws: %s\n", name, e.what());
        }
    }
    std::printf("names locate_zone accepts:");
    for (const char* name : {"US/Pacific", "Asia/Calcutta", "Japan", "EST", "CST6CDT", "Etc/GMT+8", "GMT", "Z", "PST", "CET"})
    {
        try { std::printf(" %s->%s", name, std::string(db.locate_zone(name)->name()).c_str()); }
        catch (const std::exception&) { std::printf(" %s->(throws)", name); }
    }
    std::printf("\n");
    try { std::printf("current_zone() = %s\n", std::string(db.current_zone()->name()).c_str()); }
    catch (const std::exception& e) { std::printf("current_zone() throws: %s\n", e.what()); }
    for (const char* name : {"America/Chicago", "Asia/Tokyo"})
    {
        try { std::printf("get_info(sys_seconds::min()) on %s: %s\n", name, info_line(db.locate_zone(name)->get_info(sys_seconds::min())).c_str()); }
        catch (const std::exception& e) { std::printf("get_info(sys_seconds::min()) on %s throws: %s\n", name, e.what()); }
    }
}

// IOv2's walk over every zone, as s_timezone_tree does it.
void walk(const tzdb& db, std::size_t zone_limit, bool report)
{
    const sys_seconds projected = time_point_cast<seconds>(system_clock::now()) + years{10};
    const sys_seconds floor{sys_days{2038y / January / 1}};
    const sys_seconds horizon = (projected > floor) ? projected : floor;
    const sys_seconds start{sys_days{year::min() / January / 1}};

    std::map<std::string, int> abbrevs;
    std::vector<std::string> failures;
    std::size_t calls = 0, zones = 0, most = 0;
    std::string most_zone;
    const auto t0 = steady_clock::now();
    for (const auto& zone : db.zones)
    {
        if (zones++ == zone_limit) break;
        std::size_t here = 0;
        try
        {
            sys_seconds t = start;
            while (t < horizon)
            {
                const sys_info info = zone.get_info(t);
                ++calls; ++here;
                if (!info.abbrev.empty()) ++abbrevs[info.abbrev];
                if (info.end <= t) break;
                t = time_point_cast<seconds>(info.end);
            }
        }
        catch (const std::exception& e)
        {
            failures.push_back(std::string(zone.name()) + ": " + e.what());
        }
        if (here > most) { most = here; most_zone = zone.name(); }
        if (report && zones % 100 == 0) std::printf("  %zu zones, %zu get_info calls, %.0f ms\n", zones, calls, ms_since(t0));
    }
    std::printf("  walked %zu zones: %zu get_info calls in %.1f ms; most calls %zu (%s); %zu zones threw\n",
                zones > zone_limit ? zone_limit : zones, calls, ms_since(t0), most, most_zone.c_str(), failures.size());
    if (!report) return;
    for (std::size_t i = 0; i < failures.size() && i < 10; ++i) std::printf("    %s\n", failures[i].c_str());

    std::vector<std::string> non_ascii, offset_like, numeric;
    for (const auto& [a, n] : abbrevs)
    {
        bool ascii = true, digit = false;
        for (const unsigned char c : a) { if (c >= 0x80) ascii = false; if (c >= '0' && c <= '9') digit = true; }
        if (!ascii) non_ascii.push_back(show(a));
        if (a.rfind("GMT", 0) == 0 || a.rfind("UTC", 0) == 0) { if (a.size() > 3) offset_like.push_back(show(a)); }
        else if (digit) numeric.push_back(show(a));
    }
    std::printf("  %zu distinct abbreviations\n", abbrevs.size());
    std::printf("  not ASCII (%zu):", non_ascii.size());
    for (std::size_t i = 0; i < non_ascii.size() && i < 40; ++i) std::printf(" %s", non_ascii[i].c_str());
    std::printf("\n  GMT/UTC with an offset (%zu):", offset_like.size());
    for (std::size_t i = 0; i < offset_like.size() && i < 40; ++i) std::printf(" %s", offset_like[i].c_str());
    std::printf("\n  other with digits (%zu):", numeric.size());
    for (std::size_t i = 0; i < numeric.size() && i < 60; ++i) std::printf(" %s", numeric[i].c_str());
    std::printf("\n  all:");
    std::size_t col = 0;
    for (const auto& [a, n] : abbrevs) { std::printf(" %s", a.c_str()); if (++col % 30 == 0) std::printf("\n      "); }
    std::printf("\n");

    std::size_t clash = 0;
    for (const auto& z : db.zones) if (abbrevs.count(std::string(z.name()))) ++clash;
    for (const auto& l : db.links) if (abbrevs.count(std::string(l.name()))) ++clash;
    std::printf("  abbreviations that are also zone or link names: %zu\n", clash);
}

#if defined(_WIN32)
// The MSVC STL takes abbreviations from ICU's default locale; set it from here (the same
// icu.dll, so the same process-wide default) and see what comes back.
void icu_locales(const tzdb& db)
{
    const HMODULE icu = LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!icu) { std::printf("icu.dll: not loadable (%lu)\n", GetLastError()); return; }
    using get_default_t = const char* (*)();
    using set_default_t = void (*)(const char*, int*);
    const auto get_default = reinterpret_cast<get_default_t>(reinterpret_cast<void*>(GetProcAddress(icu, "uloc_getDefault")));
    const auto set_default = reinterpret_cast<set_default_t>(reinterpret_cast<void*>(GetProcAddress(icu, "uloc_setDefault")));
    wchar_t user[LOCALE_NAME_MAX_LENGTH] = {};
    GetUserDefaultLocaleName(user, LOCALE_NAME_MAX_LENGTH);
    std::printf("Windows: ACP %u, user locale %ls, UI language 0x%04X; ICU default locale %s\n", GetACP(), user,
                GetUserDefaultUILanguage(), get_default ? get_default() : "(no uloc_getDefault)");
    if (!get_default || !set_default) return;
    const std::string original = get_default();
    const sys_seconds jan = sys_days{2026y / January / 15}, jul = sys_days{2026y / July / 15};
    for (const char* loc : {"en_US", "en_GB", "de_DE", "fr_FR", "fi_FI", "ru_RU", "zh_CN", "ja_JP", "ar_SA", "fa_IR", "hi_IN"})
    {
        int err = 0;
        set_default(loc, &err);
        std::printf("  ICU default %s (err %d):", loc, err);
        for (const char* name : {"America/New_York", "Europe/Berlin", "Europe/London", "Asia/Shanghai", "Asia/Tokyo", "Asia/Kolkata", "UTC"})
        {
            try
            {
                const time_zone* z = db.locate_zone(name);
                std::printf(" %s=%s/%s", name, show(z->get_info(jan).abbrev).c_str(), show(z->get_info(jul).abbrev).c_str());
            }
            catch (const std::exception& e) { std::printf(" %s throws (%s)", name, e.what()); }
        }
        std::printf("\n");
    }
    int err = 0;
    set_default(original.c_str(), &err);
}
#endif
#endif
} // namespace

int main()
{
    print_config();
#if PROBE_HAS_TZDB
    try
    {
        const auto t0 = steady_clock::now();
        const tzdb& db = get_tzdb();
        std::printf("get_tzdb(): version %s, %zu zones, %zu links, %zu leap seconds, %.1f ms\n", db.version.c_str(),
                    db.zones.size(), db.links.size(), db.leap_seconds.size(), ms_since(t0));
        samples(db);
#if defined(_WIN32)
        icu_locales(db);
#endif
        std::printf("IOv2's %%Z walk, first run:\n");
        walk(db, static_cast<std::size_t>(-1), true);
        std::printf("IOv2's %%Z walk, second run over the first 50 zones (caching?):\n");
        walk(db, 50, false);
    }
    catch (const std::exception& e)
    {
        std::printf("get_tzdb() throws: %s\n", e.what());
    }
#endif
    return 0;
}
