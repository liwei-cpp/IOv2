// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// Timeio port probe: what the timeio facet could read from the platform, as input to the
// timeio port. Not a test and not for main: it prints what it sees and always exits 0.
//   1. per locale: the C library's own data (POSIX nl_langinfo; Windows NLS, its calendars,
//      and the UCRT's _Getdays/_Getmonths), what the C library's strftime renders for two
//      fixed instants, and what the platform's C++ library makes of %c %x %X %r when it
//      writes them with time_put and reads them back with time_get
//   2. --scan, over every locale the system has (POSIX: names on stdin; Windows:
//      EnumSystemLocalesEx). POSIX: formats IOv2 would reject, empty items, era and
//      alternative digits, %B against %OB. Windows: the tokens every date/time picture
//      uses, calendar types, native digits, genitive month names
// Built with -DPROBE_STRICT_POSIX=1 -D_POSIX_C_SOURCE=200809L (as IOv2 builds), it only
// reports which langinfo items stay visible.
// Windows: cl /std:c++latest /utf-8 /EHsc /W4 timeio_probe.cpp
// POSIX:   c++ -std=c++20 timeio_probe.cpp; `locale -a | ./a.out --scan` scans

#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <locale.h>
#else
#include <langinfo.h>
#include <locale.h>
#if defined(__APPLE__) && !PROBE_STRICT_POSIX
#include <xlocale.h>
#endif
#endif

#if PROBE_STRICT_POSIX
// Which langinfo items a strict _POSIX_C_SOURCE leaves visible.
int main()
{
#if defined(_POSIX_C_SOURCE)
    std::printf("_POSIX_C_SOURCE=%ld\n", static_cast<long>(_POSIX_C_SOURCE));
#endif
    std::printf("langinfo items visible under the strict build:\n");
#if defined(ERA)
    std::printf("  ERA          yes\n");
#else
    std::printf("  ERA          NO\n");
#endif
#if defined(ERA_D_FMT)
    std::printf("  ERA_D_FMT    yes\n");
#else
    std::printf("  ERA_D_FMT    NO\n");
#endif
#if defined(ERA_T_FMT)
    std::printf("  ERA_T_FMT    yes\n");
#else
    std::printf("  ERA_T_FMT    NO\n");
#endif
#if defined(ERA_D_T_FMT)
    std::printf("  ERA_D_T_FMT  yes\n");
#else
    std::printf("  ERA_D_T_FMT  NO\n");
#endif
#if defined(ALT_DIGITS)
    std::printf("  ALT_DIGITS   yes\n");
#else
    std::printf("  ALT_DIGITS   NO\n");
#endif
#if defined(T_FMT_AMPM)
    std::printf("  T_FMT_AMPM   yes\n");
#else
    std::printf("  T_FMT_AMPM   NO\n");
#endif
#if defined(AM_STR)
    std::printf("  AM_STR       yes\n");
#else
    std::printf("  AM_STR       NO\n");
#endif
#if defined(D_T_FMT)
    std::printf("  D_T_FMT      yes\n");
#else
    std::printf("  D_T_FMT      NO\n");
#endif
#if defined(ABMON_1)
    std::printf("  ABMON_1      yes\n");
#else
    std::printf("  ABMON_1      NO\n");
#endif
#if defined(ALTMON_1)
    std::printf("  ALTMON_1     yes\n");
#else
    std::printf("  ALTMON_1     NO\n");
#endif
#if defined(_NL_TIME_ERA_NUM_ENTRIES)
    std::printf("  _NL_TIME_ERA_NUM_ENTRIES (macro) yes\n");
#else
    std::printf("  _NL_TIME_ERA_NUM_ENTRIES (macro) no (glibc has it as an enumerator only)\n");
#endif
    return 0;
}
#else

namespace
{
// ---------------------------------------------------------------- printing

// UTF-8 passes through, except controls, quotes and the invisible or space-like code
// points, which show as <U+XXXX>; bytes that are not UTF-8 show as \xNN.
std::string show_utf8(std::string_view s)
{
    std::string r = "\"";
    std::size_t i = 0;
    while (i < s.size())
    {
        const auto b = static_cast<unsigned char>(s[i]);
        std::size_t len = b < 0x80 ? 1 : (b >> 5) == 0x6 ? 2 : (b >> 4) == 0xE ? 3 : (b >> 3) == 0x1E ? 4 : 0;
        bool ok = len != 0 && i + len <= s.size();
        for (std::size_t k = 1; ok && k < len; ++k)
            ok = (static_cast<unsigned char>(s[i + k]) & 0xC0) == 0x80;
        char buf[16];
        if (!ok)
        {
            std::snprintf(buf, sizeof buf, "\\x%02X", b);
            r += buf;
            ++i;
            continue;
        }
        std::uint32_t cp = len == 1 ? b : (b & (0x7F >> len));
        for (std::size_t k = 1; k < len; ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        const bool odd = cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0xA0) || cp == 0xAD
                      || cp == 0x061C || (cp >= 0x2000 && cp <= 0x200F) || (cp >= 0x2028 && cp <= 0x202F)
                      || (cp >= 0x205F && cp <= 0x206F) || cp == 0x3000 || cp == 0xFEFF
                      || cp == '"' || cp == '\\';
        if (odd)
        {
            std::snprintf(buf, sizeof buf, "<U+%04X>", static_cast<unsigned>(cp));
            r += buf;
        }
        else
            r.append(s.substr(i, len));
        i += len;
    }
    return r + "\"";
}

// Every non-ASCII byte as \xNN: for text in a legacy encoding.
std::string show_bytes(std::string_view s)
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

std::string show(std::string_view s, bool utf8) { return utf8 ? show_utf8(s) : show_bytes(s); }

// ---------------------------------------------------------------- shared

// The two instants every strftime line renders.
//   t1 2026-03-05 07:08:09 Thursday: single-digit fields show whether a locale pads
//   t2 2061-12-31 23:55:59 Saturday: libc++'s analysis instant, every field distinct
std::tm make_tm(int y, int mon, int d, int h, int mi, int s, int wday, int yday)
{
    std::tm t{};
    t.tm_year = y - 1900; t.tm_mon = mon - 1; t.tm_mday = d;
    t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
    t.tm_wday = wday; t.tm_yday = yday; t.tm_isdst = 0;
    return t;
}
const std::tm t1 = make_tm(2026, 3, 5, 7, 8, 9, 4, 63);
const std::tm t2 = make_tm(2061, 12, 31, 23, 55, 59, 6, 364);

std::string tm_fields(const std::tm& t)
{
    char buf[96];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d wday=%d",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, t.tm_wday);
    return buf;
}

// IOv2's normalize_time_format (timeio_details.h), copied: what IOv2 would keep of a
// locale-supplied format, or nullopt where it falls back to the C format.
[[maybe_unused]] std::optional<std::string> iov2_normalize(const std::string& fmt)
{
    constexpr std::string_view supported = "%ABCDFGHIMRSTUVWXYZabcdeghjmnprtuwxyz";
    std::string out;
    for (std::size_t i = 0; i < fmt.size(); ++i)
    {
        if (fmt[i] != '%') { out += fmt[i]; continue; }
        std::size_t j = i + 1;
        while (j < fmt.size() && (fmt[j] == '-' || fmt[j] == '_' || fmt[j] == '0' || fmt[j] == '^' || fmt[j] == '#'))
            ++j;
        while (j < fmt.size() && fmt[j] >= '0' && fmt[j] <= '9') ++j;
        char modifier = '\0';
        if (j < fmt.size() && (fmt[j] == 'E' || fmt[j] == 'O')) modifier = fmt[j++];
        if (j >= fmt.size()) { out.append(fmt, i, std::string::npos); break; }
        char spec = fmt[j];
        if (spec == 'l') spec = 'I';
        else if (spec == 'k') spec = 'H';
        else if (spec == 'P') spec = 'p';
        if (supported.find(spec) == std::string_view::npos) return std::nullopt;
        out += '%';
        if (modifier != '\0') out += modifier;
        out += spec;
        i = j;
    }
    return out;
}

// The platform's own C++ library: time_put writes t1 with each locale format, time_get
// reads it back with the same format.
void cxx_roundtrip(const char* name, bool utf8)
{
    std::printf("  C++ library (time_put then time_get on t1):\n");
    try
    {
        const std::locale loc(name);
        const auto& tp = std::use_facet<std::time_put<char>>(loc);
        const auto& tg = std::use_facet<std::time_get<char>>(loc);
        static const char* const order[] = {"no_order", "dmy", "mdy", "ymd", "ydm"};
        std::printf("    date_order() = %s\n", order[tg.date_order()]);
        for (const char* fmt : {"%c", "%x", "%X", "%r", "%p", "%B", "%b", "%A"})
        {
            std::ostringstream os;
            os.imbue(loc);
            tp.put(std::ostreambuf_iterator<char>(os), os, ' ', &t1, fmt, fmt + std::strlen(fmt));
            const std::string text = os.str();
            std::istringstream is(text);
            is.imbue(loc);
            std::tm back{};
            back.tm_year = -1000; back.tm_mon = 99; back.tm_mday = 99; back.tm_hour = 99; back.tm_min = 99; back.tm_sec = 99; back.tm_wday = 9;
            std::ios_base::iostate err = std::ios_base::goodbit;
            const auto end = tg.get(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), is, err,
                                    &back, fmt, fmt + std::strlen(fmt));
            std::string rest(end, std::istreambuf_iterator<char>());
            std::printf("    %-3s put %-40s get%s%s -> %s%s\n", fmt, show(text, utf8).c_str(),
                        (err & std::ios_base::failbit) ? " FAIL" : " ok",
                        (err & std::ios_base::eofbit) ? "+eof" : "", tm_fields(back).c_str(),
                        rest.empty() ? "" : (" left " + show(rest, utf8)).c_str());
        }
    }
    catch (const std::exception& e)
    {
        std::printf("    std::locale(\"%s\") throws: %s\n", name, e.what());
    }
}

const char* const strftime_specs[] = {
    "%c", "%x", "%X", "%r", "%p", "%Ec", "%Ex", "%EX", "%EC", "%Ey", "%EY", "%Od", "%OH", "%Oy",
    "%B", "%OB", "%b", "%Ob", "%A", "%a",
#if defined(_WIN32)
    "%#c", "%#x",
#endif
};

#if defined(_WIN32)
// ================================================================ Windows

std::string utf8_of(std::wstring_view w)
{
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string r(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), r.data(), n, nullptr, nullptr);
    return r;
}

std::string narrow_of(std::wstring_view w, unsigned cp)
{
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(cp, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string r(static_cast<std::size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(cp, 0, w.data(), static_cast<int>(w.size()), r.data(), n, nullptr, nullptr);
    return r;
}

std::string w(std::wstring_view s) { return show_utf8(utf8_of(s)); }

std::wstring nls(const wchar_t* loc, LCTYPE type)
{
    const int n = GetLocaleInfoEx(loc, type, nullptr, 0);
    if (n <= 0) return L"(error " + std::to_wstring(GetLastError()) + L")";
    std::wstring r(static_cast<std::size_t>(n), L'\0');
    GetLocaleInfoEx(loc, type, r.data(), n);
    r.resize(static_cast<std::size_t>(n - 1));
    return r;
}

void ignore_invalid_parameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {}

// The tokens of an NLS date/time picture: a run of one of d M y g h H m s t, a quoted
// literal (with '' as a quote inside it), or any other single character.
std::vector<std::wstring> picture_tokens(const std::wstring& p)
{
    std::vector<std::wstring> out;
    for (std::size_t i = 0; i < p.size();)
    {
        const wchar_t c = p[i];
        if (c == L'\'')
        {
            std::size_t j = i + 1;
            std::wstring lit;
            while (j < p.size())
            {
                if (p[j] == L'\'')
                {
                    if (j + 1 < p.size() && p[j + 1] == L'\'') { lit += L'\''; j += 2; continue; }
                    ++j;
                    break;
                }
                lit += p[j++];
            }
            out.push_back(L"'" + lit + L"'");
            i = j;
            continue;
        }
        if (std::wcschr(L"dMyghHmst", c))
        {
            std::size_t j = i;
            while (j < p.size() && p[j] == c) ++j;
            out.push_back(p.substr(i, j - i));
            i = j;
            continue;
        }
        out.emplace_back(1, c);
        ++i;
    }
    return out;
}

// The draft translation of a picture into strftime's grammar: d/dd -> %d, ddd -> %a,
// dddd -> %A, M/MM -> %m, MMM -> %b, MMMM -> %B, y/yy -> %y, yyy+ -> %Y, h/hh -> %I,
// H/HH -> %H, m/mm -> %M, s/ss -> %S, t/tt -> %p; literals with % doubled; g/gg (era)
// untranslatable.
std::optional<std::wstring> draft_translate(const std::wstring& p, std::wstring* bad = nullptr)
{
    std::wstring out;
    for (const auto& t : picture_tokens(p))
    {
        auto lit = [&out](std::wstring_view s) { for (wchar_t c : s) { if (c == L'%') out += L"%%"; else out += c; } };
        if (t[0] == L'\'') { lit(std::wstring_view(t).substr(1, t.size() - 2)); continue; }
        const std::size_t n = t.size();
        switch (t[0])
        {
        case L'd': out += n <= 2 ? L"%d" : n == 3 ? L"%a" : L"%A"; break;
        case L'M': out += n <= 2 ? L"%m" : n == 3 ? L"%b" : L"%B"; break;
        case L'y': out += n <= 2 ? L"%y" : L"%Y"; break;
        case L'h': out += L"%I"; break;
        case L'H': out += L"%H"; break;
        case L'm': out += L"%M"; break;
        case L's': out += L"%S"; break;
        case L't': out += L"%p"; break;
        case L'g': if (bad) *bad = t; return std::nullopt;
        default:
            if ((t[0] >= L'a' && t[0] <= L'z') || (t[0] >= L'A' && t[0] <= L'Z')) { if (bad) *bad = t; return std::nullopt; }
            lit(t);
        }
    }
    return out;
}

struct cal_ctx { std::vector<std::pair<CALID, std::wstring>> list; std::vector<std::wstring> strings; };

BOOL CALLBACK cal_names(LPWSTR s, CALID id, LPWSTR, LPARAM lp)
{
    reinterpret_cast<cal_ctx*>(lp)->list.emplace_back(id, s);
    return TRUE;
}

BOOL CALLBACK cal_strings(LPWSTR s, CALID, LPWSTR, LPARAM lp)
{
    reinterpret_cast<cal_ctx*>(lp)->strings.emplace_back(s);
    return TRUE;
}

std::vector<std::wstring> cal_enum(const wchar_t* loc, CALID cal, CALTYPE type)
{
    cal_ctx ctx;
    EnumCalendarInfoExEx(cal_strings, loc, cal, nullptr, type, reinterpret_cast<LPARAM>(&ctx));
    return ctx.strings;
}

void print_nls(const wchar_t* bcp47)
{
    std::printf("  NLS (GetLocaleInfoEx on %s):\n", utf8_of(bcp47).c_str());
    const struct { const char* name; LCTYPE t; } items[] = {
        {"SSHORTDATE", LOCALE_SSHORTDATE}, {"SLONGDATE", LOCALE_SLONGDATE}, {"SYEARMONTH", LOCALE_SYEARMONTH},
        {"SMONTHDAY", LOCALE_SMONTHDAY}, {"STIMEFORMAT", LOCALE_STIMEFORMAT}, {"SSHORTTIME", LOCALE_SSHORTTIME},
        {"S1159", LOCALE_S1159}, {"S2359", LOCALE_S2359}, {"ILDATE", LOCALE_ILDATE},
        {"ICALENDARTYPE", LOCALE_ICALENDARTYPE}, {"IFIRSTDAYOFWEEK", LOCALE_IFIRSTDAYOFWEEK},
        {"SNATIVEDIGITS", LOCALE_SNATIVEDIGITS}, {"IDIGITSUBSTITUTION", LOCALE_IDIGITSUBSTITUTION},
    };
    for (const auto& it : items)
    {
        const std::wstring v = nls(bcp47, it.t);
        std::printf("    %-18s %s", it.name, w(v).c_str());
        if (it.t == LOCALE_SSHORTDATE || it.t == LOCALE_SLONGDATE || it.t == LOCALE_STIMEFORMAT
            || it.t == LOCALE_SSHORTTIME || it.t == LOCALE_SYEARMONTH)
        {
            std::wstring bad;
            const auto tr = draft_translate(v, &bad);
            std::printf("  -> %s", tr ? w(*tr).c_str() : ("(untranslatable " + utf8_of(bad) + ")").c_str());
        }
        std::printf("\n");
    }
    std::printf("    days (Sunday first):     ");
    for (int i = 0; i < 7; ++i)
        std::printf("%s ", w(nls(bcp47, static_cast<LCTYPE>(LOCALE_SDAYNAME1 + (i + 6) % 7))).c_str());
    std::printf("\n    abbr days:               ");
    for (int i = 0; i < 7; ++i)
        std::printf("%s ", w(nls(bcp47, static_cast<LCTYPE>(LOCALE_SABBREVDAYNAME1 + (i + 6) % 7))).c_str());
    std::printf("\n    months:                  ");
    for (int i = 0; i < 13; ++i) std::printf("%s ", w(nls(bcp47, LOCALE_SMONTHNAME1 + i)).c_str());
    std::printf("\n    months (genitive):       ");
    for (int i = 0; i < 13; ++i)
        std::printf("%s ", w(nls(bcp47, (LOCALE_SMONTHNAME1 + i) | LOCALE_RETURN_GENITIVE_NAMES)).c_str());
    std::printf("\n    abbr months:             ");
    for (int i = 0; i < 13; ++i) std::printf("%s ", w(nls(bcp47, LOCALE_SABBREVMONTHNAME1 + i)).c_str());
    std::printf("\n    abbr months (genitive):  ");
    for (int i = 0; i < 13; ++i)
        std::printf("%s ", w(nls(bcp47, (LOCALE_SABBREVMONTHNAME1 + i) | LOCALE_RETURN_GENITIVE_NAMES)).c_str());
    std::printf("\n");

    cal_ctx cals;
    EnumCalendarInfoExEx(cal_names, bcp47, ENUM_ALL_CALENDARS, nullptr, CAL_SCALNAME, reinterpret_cast<LPARAM>(&cals));
    for (const auto& [id, name] : cals.list)
    {
        std::printf("    calendar %lu %s\n", static_cast<unsigned long>(id), w(name).c_str());
        const struct { const char* name; CALTYPE t; } citems[] = {
            {"SERASTRING", CAL_SERASTRING}, {"SABBREVERASTRING", CAL_SABBREVERASTRING},
            {"IYEAROFFSETRANGE", CAL_IYEAROFFSETRANGE}, {"SSHORTDATE", CAL_SSHORTDATE}, {"SLONGDATE", CAL_SLONGDATE},
        };
        for (const auto& c : citems)
        {
            std::printf("      %-17s", c.name);
            for (const auto& s : cal_enum(bcp47, id, c.t)) std::printf(" %s", w(s).c_str());
            std::printf("\n");
        }
    }
}

void print_japanese_era_registry()
{
    HKEY key{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Nls\\Calendars\\Japanese\\Eras",
                      0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        std::printf("Japanese era registry key: not present\n");
        return;
    }
    std::printf("Japanese era registry key (value name = start date, data = names):\n");
    for (DWORD i = 0;; ++i)
    {
        wchar_t name[128]; DWORD nlen = 128;
        wchar_t data[256]; DWORD dlen = sizeof data;
        DWORD type = 0;
        if (RegEnumValueW(key, i, name, &nlen, nullptr, &type, reinterpret_cast<BYTE*>(data), &dlen) != ERROR_SUCCESS)
            break;
        std::printf("  %s = %s\n", utf8_of(name).c_str(),
                    type == REG_SZ ? w(std::wstring(data, dlen / sizeof(wchar_t)).c_str()).c_str() : "(not REG_SZ)");
    }
    RegCloseKey(key);
}

void probe_locale(const char* name)
{
    std::printf("\n=== %s ===\n", name);
    if (!std::setlocale(LC_ALL, name))
    {
        std::printf("  setlocale fails\n");
        return;
    }
    const wchar_t* lc_time = ___lc_locale_name_func()[LC_TIME];
    const unsigned cp = ___lc_codepage_func();
    std::printf("  setlocale(LC_TIME) = %s, LC_TIME BCP-47 = %s, code page = %u\n",
                std::setlocale(LC_TIME, nullptr), lc_time ? utf8_of(lc_time).c_str() : "(null: C)", cp);
    const bool utf8 = cp == CP_UTF8;

    // What the MSVC STL's time_get reads its names from.
    if (char* d = _Getdays()) { std::printf("  _Getdays()     %s\n", show(d, utf8).c_str()); std::free(d); }
    if (char* m = _Getmonths()) { std::printf("  _Getmonths()   %s\n", show(m, utf8).c_str()); std::free(m); }
    if (wchar_t* m = _W_Getmonths()) { std::printf("  _W_Getmonths() %s\n", w(m).c_str()); std::free(m); }

    if (lc_time)
    {
        print_nls(lc_time);
        // Is the UCRT's narrow text the NLS wide text in the locale's code page?
        int same = 0, differ = 0;
        for (int i = 0; i < 12; ++i)
        {
            std::tm t = t1;
            t.tm_mon = i;
            char nb[128]; wchar_t wb[128];
            std::strftime(nb, sizeof nb, "%B", &t);
            std::wcsftime(wb, 128, L"%B", &t);
            const std::string conv = narrow_of(wb, cp);
            (conv == nb ? same : differ)++;
            if (conv != nb)
                std::printf("    narrow %%B month %d: strftime %s, WideCharToMultiByte(cp) %s\n", i + 1,
                            show_bytes(nb).c_str(), show_bytes(conv).c_str());
        }
        std::printf("  narrow %%B equals WideCharToMultiByte(code page, wcsftime %%B): %d of 12\n", same);
    }

    std::printf("  UCRT wcsftime (t1 | t2):\n");
    for (const char* spec : strftime_specs)
    {
        std::wstring wspec(spec, spec + std::strlen(spec));
        wchar_t b1[256] = {}, b2[256] = {};
        const std::size_t n1 = std::wcsftime(b1, 256, wspec.c_str(), &t1);
        const std::size_t n2 = std::wcsftime(b2, 256, wspec.c_str(), &t2);
        std::printf("    %-4s %s | %s%s\n", spec, w(b1).c_str(), w(b2).c_str(),
                    (n1 == 0 || n2 == 0) ? "  (returned 0)" : "");
    }
    std::setlocale(LC_ALL, "C");
    cxx_roundtrip(name, utf8);
}

struct scan_names { std::vector<std::wstring> v; };
BOOL CALLBACK collect_locale(LPWSTR name, DWORD, LPARAM lp)
{
    if (name && *name) reinterpret_cast<scan_names*>(lp)->v.emplace_back(name);
    return TRUE;
}

void scan()
{
    scan_names names;
    EnumSystemLocalesEx(collect_locale, LOCALE_WINDOWS, reinterpret_cast<LPARAM>(&names), nullptr);
    std::printf("=== scan: %zu Windows locales ===\n", names.v.size());

    const struct { const char* name; LCTYPE t; } pics[] = {
        {"SSHORTDATE", LOCALE_SSHORTDATE}, {"SLONGDATE", LOCALE_SLONGDATE},
        {"STIMEFORMAT", LOCALE_STIMEFORMAT}, {"SSHORTTIME", LOCALE_SSHORTTIME},
    };
    for (const auto& pic : pics)
    {
        std::map<std::string, int> hist;
        std::vector<std::string> untranslatable;
        for (const auto& loc : names.v)
        {
            const std::wstring v = nls(loc.c_str(), pic.t);
            std::set<std::string> seen;
            for (const auto& tok : picture_tokens(v))
            {
                std::string key;
                if (tok[0] == L'\'') key = "'literal'";
                else if (std::wcschr(L"dMyghHmst", tok[0])) key = utf8_of(tok);
                else if ((tok[0] >= L'a' && tok[0] <= L'z') || (tok[0] >= L'A' && tok[0] <= L'Z')) key = "other letter " + utf8_of(tok);
                else if (tok[0] == L'%') key = "%";
                else continue;
                seen.insert(key);
            }
            for (const auto& k : seen) ++hist[k];
            std::wstring bad;
            if (!draft_translate(v, &bad)) untranslatable.push_back(utf8_of(loc) + " " + show_utf8(utf8_of(v)));
        }
        std::printf("%s token -> number of locales using it:\n ", pic.name);
        for (const auto& [k, n] : hist) std::printf(" %s=%d", k.c_str(), n);
        std::printf("\n  untranslatable by the draft (%zu):", untranslatable.size());
        for (std::size_t i = 0; i < untranslatable.size() && i < 40; ++i) std::printf("\n    %s", untranslatable[i].c_str());
        std::printf("\n");
    }

    std::map<std::wstring, int> caltype;
    std::vector<std::string> non_gregorian, digits, genitive, empty_ampm;
    for (const auto& loc : names.v)
    {
        const std::wstring ct = nls(loc.c_str(), LOCALE_ICALENDARTYPE);
        ++caltype[ct];
        if (ct != L"1") non_gregorian.push_back(utf8_of(loc) + "=" + utf8_of(ct));
        const std::wstring nd = nls(loc.c_str(), LOCALE_SNATIVEDIGITS);
        if (nd != L"0123456789") digits.push_back(utf8_of(loc) + " " + show_utf8(utf8_of(nd)));
        int diff = 0;
        for (int i = 0; i < 12; ++i)
            if (nls(loc.c_str(), LOCALE_SMONTHNAME1 + i) != nls(loc.c_str(), (LOCALE_SMONTHNAME1 + i) | LOCALE_RETURN_GENITIVE_NAMES))
                ++diff;
        if (diff) genitive.push_back(utf8_of(loc) + "(" + std::to_string(diff) + ")");
        if (nls(loc.c_str(), LOCALE_S1159).empty() || nls(loc.c_str(), LOCALE_S2359).empty()) empty_ampm.push_back(utf8_of(loc));
    }
    std::printf("ICALENDARTYPE ->");
    for (const auto& [k, n] : caltype) std::printf(" %s=%d", utf8_of(k).c_str(), n);
    auto list = [](const char* title, const std::vector<std::string>& v, std::size_t cap) {
        std::printf("\n%s (%zu):", title, v.size());
        for (std::size_t i = 0; i < v.size() && i < cap; ++i) std::printf(" %s", v[i].c_str());
        std::printf("\n");
    };
    list("non-Gregorian default calendar", non_gregorian, 200);
    list("native digits other than 0-9", digits, 80);
    list("genitive month names differ (count of months)", genitive, 200);
    list("empty AM or PM", empty_ampm, 200);
}

#else
// ================================================================ POSIX

const nl_item day_items[7] = {DAY_1, DAY_2, DAY_3, DAY_4, DAY_5, DAY_6, DAY_7};
const nl_item abday_items[7] = {ABDAY_1, ABDAY_2, ABDAY_3, ABDAY_4, ABDAY_5, ABDAY_6, ABDAY_7};
const nl_item mon_items[12] = {MON_1, MON_2, MON_3, MON_4, MON_5, MON_6, MON_7, MON_8, MON_9, MON_10, MON_11, MON_12};
const nl_item abmon_items[12] = {ABMON_1, ABMON_2, ABMON_3, ABMON_4, ABMON_5, ABMON_6,
                                 ABMON_7, ABMON_8, ABMON_9, ABMON_10, ABMON_11, ABMON_12};

struct fmt_item { const char* name; nl_item id; };
const fmt_item fmt_items[] = {
    {"D_T_FMT", D_T_FMT}, {"D_FMT", D_FMT}, {"T_FMT", T_FMT}, {"T_FMT_AMPM", T_FMT_AMPM},
    {"ERA_D_T_FMT", ERA_D_T_FMT}, {"ERA_D_FMT", ERA_D_FMT}, {"ERA_T_FMT", ERA_T_FMT},
};

std::string li(nl_item i) { const char* p = nl_langinfo(i); return p ? p : "(null)"; }

std::string ftime(const char* spec, const std::tm& t)
{
    char buf[256];
    const std::size_t n = std::strftime(buf, sizeof buf, spec, &t);
    return std::string(buf, n);
}

// ALT_DIGITS: glibc stores up to 100 NUL-separated strings ending in an empty one; POSIX
// says ';'-separated.
std::vector<std::string> alt_digits()
{
    std::vector<std::string> r;
    const char* p = nl_langinfo(ALT_DIGITS);
    if (!p) return r;
#if defined(__GLIBC__)
    for (int i = 0; i < 100 && *p; ++i) { r.emplace_back(p); p += std::strlen(p) + 1; }
#else
    std::string s = p;
    std::size_t b = 0;
    while (!s.empty() && b <= s.size())
    {
        const std::size_t e = s.find(';', b);
        r.push_back(s.substr(b, e == std::string::npos ? std::string::npos : e - b));
        if (e == std::string::npos) break;
        b = e + 1;
    }
#endif
    return r;
}

int era_count()
{
#if defined(__GLIBC__)
    return static_cast<int>(reinterpret_cast<std::intptr_t>(nl_langinfo(_NL_TIME_ERA_NUM_ENTRIES)));
#else
    return -1;
#endif
}

void probe_locale(const char* name)
{
    std::printf("\n=== %s ===\n", name);
    locale_t loc = newlocale(LC_ALL_MASK, name, static_cast<locale_t>(nullptr));
    if (!loc)
    {
        std::printf("  not available\n");
        return;
    }
    const locale_t old = uselocale(loc);
    const std::string codeset = li(CODESET);
    const bool utf8 = codeset == "UTF-8" || codeset == "utf8";
    std::printf("  CODESET %s\n", codeset.c_str());
    for (const auto& it : fmt_items)
    {
        const std::string v = li(it.id);
        const auto n = iov2_normalize(v);
        std::printf("    %-12s %-34s IOv2 keeps %s\n", it.name, show(v, utf8).c_str(),
                    n ? show(*n, utf8).c_str() : "(nothing: falls back to C)");
    }
    std::printf("    AM_STR %s  PM_STR %s\n", show(li(AM_STR), utf8).c_str(), show(li(PM_STR), utf8).c_str());
    std::printf("    ERA %s (glibc entries: %d)\n", show(li(ERA), utf8).c_str(), era_count());
    const auto ad = alt_digits();
    std::printf("    ALT_DIGITS raw %s, %zu entries:", show(li(ALT_DIGITS).substr(0, 40), utf8).c_str(), ad.size());
    for (std::size_t i = 0; i < ad.size() && i < 12; ++i) std::printf(" %s", show(ad[i], utf8).c_str());
    std::printf("\n    days:        ");
    for (const auto i : day_items) std::printf("%s ", show(li(i), utf8).c_str());
    std::printf("\n    abbr days:   ");
    for (const auto i : abday_items) std::printf("%s ", show(li(i), utf8).c_str());
    std::printf("\n    months:      ");
    for (const auto i : mon_items) std::printf("%s ", show(li(i), utf8).c_str());
    std::printf("\n    abbr months: ");
    for (const auto i : abmon_items) std::printf("%s ", show(li(i), utf8).c_str());
    std::printf("\n    strftime %%OB:");
    for (int m = 0; m < 12; ++m) { std::tm t = t1; t.tm_mon = m; std::printf(" %s", show(ftime("%OB", t), utf8).c_str()); }
    std::printf("\n    strftime %%Ob:");
    for (int m = 0; m < 12; ++m) { std::tm t = t1; t.tm_mon = m; std::printf(" %s", show(ftime("%Ob", t), utf8).c_str()); }
    std::printf("\n  C library strftime (t1 | t2):\n");
    for (const char* spec : strftime_specs)
        std::printf("    %-4s %s | %s\n", spec, show(ftime(spec, t1), utf8).c_str(), show(ftime(spec, t2), utf8).c_str());
    uselocale(old);
    freelocale(loc);
    cxx_roundtrip(name, utf8);
}

void scan()
{
    std::string name;
    int total = 0, empty_ampm_fmt = 0, empty_am = 0, with_era = 0, with_alt = 0;
    std::map<std::string, int> specs;      // specifier -> locales using it in D_T_FMT
    std::vector<std::string> rejected, ob_differs, mon_eq_ob;
    std::map<std::string, int> codesets;
    while (std::getline(std::cin, name))
    {
        if (name.empty()) continue;
        locale_t loc = newlocale(LC_ALL_MASK, name.c_str(), static_cast<locale_t>(nullptr));
        if (!loc) continue;
        const locale_t old = uselocale(loc);
        ++total;
        ++codesets[li(CODESET)];
        for (const auto& it : fmt_items)
        {
            const std::string v = li(it.id);
            if (!iov2_normalize(v)) rejected.push_back(name + " " + it.name + " " + show_bytes(v));
        }
        {
            const std::string v = li(D_T_FMT);
            std::set<std::string> seen;
            for (std::size_t i = 0; i + 1 < v.size(); ++i)
                if (v[i] == '%')
                {
                    std::size_t j = i + 1;
                    while (j < v.size() && std::strchr("-_0^#123456789EO", v[j])) ++j;
                    if (j < v.size()) seen.insert(v.substr(i, j - i + 1));
                    i = j;
                }
            for (const auto& s : seen) ++specs[s];
        }
        if (li(T_FMT_AMPM).empty()) ++empty_ampm_fmt;
        if (li(AM_STR).empty()) ++empty_am;
        if (!li(ERA).empty() || era_count() > 0) ++with_era;
        if (!alt_digits().empty()) ++with_alt;
        int diff = 0;
        for (int m = 0; m < 12; ++m)
        {
            std::tm t = t1;
            t.tm_mon = m;
            if (ftime("%B", t) != ftime("%OB", t)) ++diff;
        }
        if (diff) ob_differs.push_back(name + "(" + std::to_string(diff) + ")");
        uselocale(old);
        freelocale(loc);
    }
    std::printf("=== scan: %d locales ===\n", total);
    std::printf("CODESET:");
    for (const auto& [k, n] : codesets) std::printf(" %s=%d", k.c_str(), n);
    std::printf("\nempty T_FMT_AMPM: %d, empty AM_STR: %d, with era: %d, with ALT_DIGITS: %d\n",
                empty_ampm_fmt, empty_am, with_era, with_alt);
    std::printf("specifiers in D_T_FMT -> locales:");
    for (const auto& [k, n] : specs) std::printf(" %s=%d", k.c_str(), n);
    std::printf("\nformats IOv2 rejects (%zu):", rejected.size());
    for (std::size_t i = 0; i < rejected.size() && i < 60; ++i) std::printf("\n  %s", rejected[i].c_str());
    std::printf("\n%%B differs from %%OB (%zu):", ob_differs.size());
    for (std::size_t i = 0; i < ob_differs.size() && i < 200; ++i) std::printf(" %s", ob_differs[i].c_str());
    std::printf("\n");
}
#endif
} // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32)
    _set_invalid_parameter_handler(ignore_invalid_parameter);
#endif
    if (argc > 1 && std::strcmp(argv[1], "--scan") == 0)
    {
        scan();
        return 0;
    }
#if defined(_WIN32)
    print_japanese_era_registry();
    const char* const locales[] = {
        "C", "en-US", "de-DE", "fr-FR", "es-ES", "ja-JP", "zh-CN", "zh-TW", "ko-KR", "ru-RU", "pl-PL",
        "cs-CZ", "uk-UA", "tr-TR", "fr-CA", "en-HK", "th-TH", "ar-SA", "fa-IR", "ps-AF", "hi-IN", "he-IL",
        "ja-JP.UTF-8", "ru-RU.UTF-8", "ja_JP.UTF-8", "Japanese_Japan.932", "Russian_Russia.1251",
        "French_France.1252", ".UTF-8",
    };
#else
    const char* const locales[] = {
        "C", "en_US.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "es_ES.UTF-8", "ja_JP.UTF-8", "zh_CN.UTF-8",
        "zh_TW.UTF-8", "ko_KR.UTF-8", "ru_RU.UTF-8", "pl_PL.UTF-8", "cs_CZ.UTF-8", "uk_UA.UTF-8",
        "tr_TR.UTF-8", "fr_CA.UTF-8", "en_HK.UTF-8", "th_TH.UTF-8", "ar_SA.UTF-8", "fa_IR.UTF-8",
        "ps_AF.UTF-8", "hi_IN.UTF-8", "he_IL.UTF-8",
        // legacy encodings, under either system's spelling
        "ja_JP.SJIS", "ja_JP.eucJP", "ja_JP.EUC-JP", "zh_CN.GB18030", "zh_CN.GBK", "ru_RU.KOI8-R",
        "de_DE.ISO8859-1", "de_DE.ISO-8859-1",
    };
#endif
    for (const char* name : locales) probe_locale(name);
    return 0;
}
#endif
