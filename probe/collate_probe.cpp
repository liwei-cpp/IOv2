// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// Collate port probe: what strcoll_l / wcscoll_l / strxfrm_l / wcsxfrm_l (and the
// UCRT's _l versions) do on each platform, as input to the collate port. Not a
// test and not for main: it prints what it sees and always exits 0.
// Windows: cl /std:c++latest /utf-8 /EHsc /W4 collate_probe.cpp
// POSIX:   c++ -std=c++23 collate_probe.cpp

#include <cerrno>
#include <climits>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cuchar>
#include <cwchar>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <locale.h>
#include <string.h>
#include <wchar.h>
#else
#include <langinfo.h>
#include <locale.h>
#if defined(__APPLE__)
#include <xlocale.h>
#include <xlocale/_langinfo.h>
#include <xlocale/_string.h>
#include <xlocale/_wchar.h>
#endif
#endif

namespace
{
#if defined(_WIN32)
    using loc_t = _locale_t;
    loc_t make_loc(const char* n) { return _create_locale(LC_ALL, n); }
    void  free_loc(loc_t l) { _free_locale(l); }
    int    x_strcoll(const char* a, const char* b, loc_t l) { return _strcoll_l(a, b, l); }
    int    x_wcscoll(const wchar_t* a, const wchar_t* b, loc_t l) { return _wcscoll_l(a, b, l); }
    size_t x_strxfrm(char* d, const char* s, size_t n, loc_t l) { return _strxfrm_l(d, s, n, l); }
    size_t x_wcsxfrm(wchar_t* d, const wchar_t* s, size_t n, loc_t l) { return _wcsxfrm_l(d, s, n, l); }
#else
    using loc_t = locale_t;
    loc_t make_loc(const char* n) { return newlocale(LC_ALL_MASK, n, nullptr); }
    void  free_loc(loc_t l) { freelocale(l); }
    int    x_strcoll(const char* a, const char* b, loc_t l) { return strcoll_l(a, b, l); }
    int    x_wcscoll(const wchar_t* a, const wchar_t* b, loc_t l) { return wcscoll_l(a, b, l); }
    size_t x_strxfrm(char* d, const char* s, size_t n, loc_t l) { return strxfrm_l(d, s, n, l); }
    size_t x_wcsxfrm(wchar_t* d, const wchar_t* s, size_t n, loc_t l) { return wcsxfrm_l(d, s, n, l); }
#endif

    int sgn(long long v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

    std::string show(const std::string& s)
    {
        std::string r;
        for (unsigned char c : s)
        {
            char buf[8];
            if (c >= 0x20 && c < 0x7F) { buf[0] = static_cast<char>(c); buf[1] = 0; }
            else std::snprintf(buf, sizeof buf, "\\x%02X", c);
            r += buf;
        }
        return r;
    }

    std::string show(const std::wstring& s)
    {
        std::string r;
        for (wchar_t c : s)
        {
            char buf[16];
            const auto u = static_cast<unsigned long>(c);
            if (u >= 0x20 && u < 0x7F) { buf[0] = static_cast<char>(u); buf[1] = 0; }
            else std::snprintf(buf, sizeof buf, "\\u%04lX", u);
            r += buf;
        }
        return r;
    }

    std::string ret_str(size_t r)
    {
        char buf[48];
        if (r == static_cast<size_t>(-1)) return "SIZE_MAX";
        if (r == static_cast<size_t>(-2)) return "-2";
        if (r == static_cast<size_t>(-3)) return "-3";
        if (r == static_cast<size_t>(INT_MAX)) return "INT_MAX";
        std::snprintf(buf, sizeof buf, "%zu", r);
        return buf;
    }

    // Narrow key; on failure returns false and reports errno/return.
    bool key_of(const std::string& s, loc_t l, std::string& key, std::string& note)
    {
        errno = 0;
        const size_t need = x_strxfrm(nullptr, s.c_str(), 0, l);
        const int e1 = errno;
        if (e1 != 0 || need == static_cast<size_t>(-1) || need == static_cast<size_t>(INT_MAX))
        {
            note = "xfrm(len) ret=" + ret_str(need) + " errno=" + std::to_string(e1);
            return false;
        }
        std::vector<char> buf(need + 1, '\x7F');
        errno = 0;
        const size_t got = x_strxfrm(buf.data(), s.c_str(), buf.size(), l);
        const int e2 = errno;
        if (got != need || e2 != 0)
            note = "len=" + ret_str(need) + " then ret=" + ret_str(got) + " errno=" + std::to_string(e2);
        key.assign(buf.data(), got);
        if (buf[got] != '\0')
            note += " [no NUL after key]";
        return true;
    }

    bool key_of(const std::wstring& s, loc_t l, std::wstring& key, std::string& note)
    {
        errno = 0;
        const size_t need = x_wcsxfrm(nullptr, s.c_str(), 0, l);
        const int e1 = errno;
        if (e1 != 0 || need == static_cast<size_t>(-1) || need == static_cast<size_t>(INT_MAX))
        {
            note = "xfrm(len) ret=" + ret_str(need) + " errno=" + std::to_string(e1);
            return false;
        }
        std::vector<wchar_t> buf(need + 1, static_cast<wchar_t>(0x7F7F));
        errno = 0;
        const size_t got = x_wcsxfrm(buf.data(), s.c_str(), buf.size(), l);
        const int e2 = errno;
        if (got != need || e2 != 0)
            note = "len=" + ret_str(need) + " then ret=" + ret_str(got) + " errno=" + std::to_string(e2);
        key.assign(buf.data(), got);
        if (buf[got] != L'\0')
            note += " [no NUL after key]";
        return true;
    }

    int key_cmp(const std::string& a, const std::string& b) { return sgn(std::strcmp(a.c_str(), b.c_str())); }
    int key_cmp(const std::wstring& a, const std::wstring& b) { return sgn(std::wcscmp(a.c_str(), b.c_str())); }

    int coll(const std::string& a, const std::string& b, loc_t l, int& raw, int& e)
    {
        errno = 0;
        raw = x_strcoll(a.c_str(), b.c_str(), l);
        e = errno;
        return sgn(raw);
    }

    int coll(const std::wstring& a, const std::wstring& b, loc_t l, int& raw, int& e)
    {
        errno = 0;
        raw = x_wcscoll(a.c_str(), b.c_str(), l);
        e = errno;
        return sgn(raw);
    }

    template <typename S>
    void pair_line(const char* label, const S& a, const S& b, loc_t l)
    {
        int raw = 0, e = 0;
        const int c = coll(a, b, l, raw, e);
        S ka, kb;
        std::string na, nb;
        const bool oka = key_of(a, l, ka, na);
        const bool okb = key_of(b, l, kb, nb);
        std::printf("    %-22s %-16s vs %-16s coll=%2d (raw=%d errno=%d)", label, show(a).c_str(), show(b).c_str(), c, raw, e);
        if (oka && okb)
        {
            const int k = key_cmp(ka, kb);
            std::printf("  key=%2d%s", k, k == c ? "" : "  <-- MISMATCH");
        }
        if (!na.empty()) std::printf("  [lhs %s]", na.c_str());
        if (!nb.empty()) std::printf("  [rhs %s]", nb.c_str());
        std::printf("\n");
    }

    void dump_key(const char* label, const std::string& s, loc_t l)
    {
        std::string k, note;
        if (!key_of(s, l, k, note)) { std::printf("    key %-14s FAILED %s\n", label, note.c_str()); return; }
        std::printf("    key %-14s len=%zu:", label, k.size());
        for (unsigned char c : k) std::printf(" %02X", c);
        std::printf("%s%s\n", note.empty() ? "" : "  ", note.c_str());
    }

    void dump_key(const char* label, const std::wstring& s, loc_t l)
    {
        std::wstring k;
        std::string note;
        if (!key_of(s, l, k, note)) { std::printf("    wkey %-13s FAILED %s\n", label, note.c_str()); return; }
        std::printf("    wkey %-13s len=%zu:", label, k.size());
        for (wchar_t c : k) std::printf(" %lX", static_cast<unsigned long>(c));
        std::printf("%s%s\n", note.empty() ? "" : "  ", note.c_str());
    }

    // What the locale says about itself.
    void describe(const char* name, loc_t l)
    {
#if defined(_WIN32)
        (void)l;
        const int mode = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        const std::string old = std::setlocale(LC_ALL, nullptr);
        if (const char* r = std::setlocale(LC_ALL, name))
        {
            const wchar_t* coll_name = ___lc_locale_name_func()[LC_COLLATE];
            const wchar_t* ctype_name = ___lc_locale_name_func()[LC_CTYPE];
            std::printf("  setlocale=\"%s\" ctype_cp=%u collate_cp=%u MB_CUR_MAX=%d collate_bcp47=%ls ctype_bcp47=%ls\n",
                        r, ___lc_codepage_func(), ___lc_collate_cp_func(), static_cast<int>(MB_CUR_MAX),
                        coll_name ? coll_name : L"(null)", ctype_name ? ctype_name : L"(null)");
        }
        std::setlocale(LC_ALL, old.c_str());
        _configthreadlocale(mode);
#else
        std::printf("  CODESET=%s\n", nl_langinfo_l(CODESET, l));
        (void)name;
#endif
    }

    const char* names[] = {
        "C", "POSIX", "C.UTF-8", "C.utf8", ".UTF-8", "en_US.UTF-8", "de_DE.UTF-8", "de-DE", "de-DE.UTF-8",
        "German_Germany.1252", "de_DE.1252", "de_DE.ISO8859-1", "de_DE.ISO8859-15", "de_DE", "sv_SE.UTF-8",
        "zh_CN.UTF-8", "zh_CN.GB18030", ".936", ".1252",
    };

    void collate_suite(const char* name, bool latin1)
    {
        loc_t l = make_loc(name);
        std::printf("=== collate under \"%s\"%s\n", name, l ? "" : "  (refused)");
        if (!l) return;
        describe(name, l);

        // ä Ä in the locale's own narrow encoding.
        const std::string ae = latin1 ? "\xE4" : "\xC3\xA4";
        const std::string AE = latin1 ? "\xC4" : "\xC3\x84";
        std::printf("  narrow:\n");
        pair_line("a vs B", std::string("a"), std::string("B"), l);
        pair_line("a vs A", std::string("a"), std::string("A"), l);
        pair_line("ae vs b", ae, std::string("b"), l);
        pair_line("AE vs B", AE, std::string("B"), l);
        pair_line("ae vs a", ae, std::string("a"), l);
        pair_line("ae vs az", ae, std::string("az"), l);
        pair_line("co-op vs coat", std::string("co-op"), std::string("coat"), l);
        pair_line("co-op vs coop", std::string("co-op"), std::string("coop"), l);
        pair_line("it's vs itch", std::string("it's"), std::string("itch"), l);
        pair_line("a b vs ab", std::string("a b"), std::string("ab"), l);
        pair_line("empty vs a", std::string(""), std::string("a"), l);
        pair_line("a vs ab", std::string("a"), std::string("ab"), l);
        if (!latin1)
        {
            pair_line("U+082F vs U+1CF7", std::string("a\xE0\xA0\xAFz"), std::string("a\xE1\xB3\xB7z"), l);
            pair_line("U+200B ignorable", std::string("a\xE2\x80\x8B" "b"), std::string("ab"), l);
            pair_line("invalid FF", std::string("a\xFF"), std::string("a"), l);
            pair_line("truncated C3", std::string("a\xC3"), std::string("a"), l);
            pair_line("ED A0 80 (D800)", std::string("a\xED\xA0\x80"), std::string("a"), l);
            pair_line("U+1F600 vs U+FFFD", std::string("\xF0\x9F\x98\x80"), std::string("\xEF\xBF\xBD"), l);
        }
        else
        {
            pair_line("byte 81 vs a", std::string("\x81"), std::string("a"), l);
        }
        dump_key("\"a\"", std::string("a"), l);
        dump_key("\"ab\"", std::string("ab"), l);
        dump_key("\"\"", std::string(""), l);

        // A too-small buffer: what comes back, and does it say the length needed?
        {
            char small[2] = {'\x7F', '\x7F'};
            errno = 0;
            const size_t r = x_strxfrm(small, "abcdef", 2, l);
            std::printf("    strxfrm(\"abcdef\", n=2) ret=%s errno=%d buf=%02X %02X\n", ret_str(r).c_str(), errno,
                        static_cast<unsigned char>(small[0]), static_cast<unsigned char>(small[1]));
        }

        std::printf("  wide:\n");
        pair_line("a vs B", std::wstring(L"a"), std::wstring(L"B"), l);
        pair_line("ae vs b", std::wstring(L"ä"), std::wstring(L"b"), l);
        pair_line("ae vs a", std::wstring(L"ä"), std::wstring(L"a"), l);
        pair_line("co-op vs coat", std::wstring(L"co-op"), std::wstring(L"coat"), l);
        pair_line("U+082F vs U+1CF7", std::wstring(L"a࠯z"), std::wstring(L"a᳷z"), l);
        pair_line("U+1F600 vs U+FFFD", std::wstring(L"\U0001F600"), std::wstring(L"�"), l);
        pair_line("U+1F600 vs U+FFFF", std::wstring(L"\U0001F600"), std::wstring(L"￿"), l);
        pair_line("U+1F600 vs U+1F601", std::wstring(L"\U0001F600"), std::wstring(L"\U0001F601"), l);
        pair_line("U+20000 vs U+4E00", std::wstring(L"\U00020000"), std::wstring(L"一"), l);
        if (sizeof(wchar_t) == 2)
        {
            pair_line("lone D800", std::wstring(L"a") + static_cast<wchar_t>(0xD800), std::wstring(L"a"), l);
            pair_line("lone DC00", std::wstring(L"a") + static_cast<wchar_t>(0xDC00), std::wstring(L"a"), l);
            pair_line("D800 vs DC00",
                      std::wstring(1, static_cast<wchar_t>(0xD800)), std::wstring(1, static_cast<wchar_t>(0xDC00)), l);
        }
        pair_line("U+FFFE nonchar", std::wstring(L"a") + static_cast<wchar_t>(0xFFFE), std::wstring(L"a"), l);
        dump_key("L\"a\"", std::wstring(L"a"), l);
        dump_key("L\"ab\"", std::wstring(L"ab"), l);
        free_loc(l);
    }

    // mbrtoc32 / mbrtoc16 under a thread locale that is not UTF-8.
    void mbrtoc_suite(const char* name)
    {
        std::printf("=== mbrtoc under \"%s\"", name);
        if (!std::setlocale(LC_ALL, name)) { std::printf("  (refused)\n"); return; }
        std::printf("  MB_CUR_MAX=%d\n", static_cast<int>(MB_CUR_MAX));
        struct In { const char* label; const char* s; size_t n; };
        const In ins[] = {
            {"C3 A9 (e-acute utf8)", "\xC3\xA9", 2},
            {"E9 (e-acute latin1)", "\xE9", 1},
            {"C0 EE (U+674E gbk)", "\xC0\xEE", 2},
            {"F0 9F 98 80 (U+1F600)", "\xF0\x9F\x98\x80", 4},
            {"E2 82 AC (euro utf8)", "\xE2\x82\xAC", 3},
            {"80 (euro cp1252)", "\x80", 1},
        };
        for (const In& in : ins)
        {
            char32_t c32 = 0xABCDu;
            std::mbstate_t st{};
            errno = 0;
            const size_t r32 = std::mbrtoc32(&c32, in.s, in.n, &st);
            const int e32 = errno;
            char16_t c16 = 0xABCD;
            std::mbstate_t st2{};
            errno = 0;
            const size_t r16 = std::mbrtoc16(&c16, in.s, in.n, &st2);
            const int e16 = errno;
            wchar_t wc = 0xABCD;
            std::mbstate_t st3{};
            const size_t rw = std::mbrtowc(&wc, in.s, in.n, &st3);
            std::printf("  %-24s mbrtoc32 r=%s c=%lX e=%d | mbrtoc16 r=%s c=%X e=%d | mbrtowc r=%s w=%lX\n", in.label,
                        ret_str(r32).c_str(), static_cast<unsigned long>(c32), e32, ret_str(r16).c_str(),
                        static_cast<unsigned>(c16), e16, ret_str(rw).c_str(), static_cast<unsigned long>(wc));
        }
        // And back: c32rtomb of U+00E9.
        char out[16] = {};
        std::mbstate_t st{};
        errno = 0;
        const size_t r = std::c32rtomb(out, U'é', &st);
        std::printf("  c32rtomb U+00E9 r=%s e=%d bytes=", ret_str(r).c_str(), errno);
        for (size_t i = 0; r != static_cast<size_t>(-1) && i < r; ++i)
            std::printf("%02X ", static_cast<unsigned char>(out[i]));
        std::printf("\n");
        std::setlocale(LC_ALL, "C");
    }
}

int main()
{
    std::printf("sizeof(wchar_t)=%zu\n", sizeof(wchar_t));
    std::printf("=== locale names\n");
    for (const char* n : names)
    {
        loc_t l = make_loc(n);
        std::printf("  %-22s %s\n", n, l ? "accepted" : "refused");
        if (l) { describe(n, l); free_loc(l); }
    }

    collate_suite("C", false);
    collate_suite("C.UTF-8", false);
    collate_suite(".UTF-8", false);
    collate_suite("en_US.UTF-8", false);
    collate_suite("de_DE.UTF-8", false);
    collate_suite("de-DE", true);
    collate_suite("German_Germany.1252", true);
    collate_suite("de_DE.ISO8859-1", true);
    collate_suite("de_DE.ISO8859-15", true);

    mbrtoc_suite("C");
    mbrtoc_suite(".1252");
    mbrtoc_suite("German_Germany.1252");
    mbrtoc_suite(".936");
    mbrtoc_suite("de_DE.ISO8859-1");
    mbrtoc_suite("zh_CN.GB18030");
    mbrtoc_suite("de_DE.UTF-8");
    return 0;
}
