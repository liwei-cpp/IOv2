// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The standard stream objects' own entry points -- sync_with_stdio(), reset(),
 * code() and switch_code() -- replace or reconfigure the streambuf underneath a
 * stream that another thread may be using. Every one of them must take io_mutex()
 * like the rest of the stream API, so that the reader / writer on the other thread
 * sees either the old configuration or the new one, never a half-replaced kernel.
 *
 * Without the lock, switch_code() frees the codecvt kernel's locale_t while the
 * writer is inside wcrtomb() on it (a SEGV, not just a torn value), and
 * sync_with_stdio() move-assigns the whole istreambuf under a running extraction.
 * These tests give ThreadSanitizer (the gcc-tsan preset) those interleavings and
 * assert, in every mode, that the streams come out consistent.
 *
 * Everything the threads exchange is literal ASCII, so it encodes identically
 * under every code the switcher cycles through -- what the reader gets back does
 * not depend on where the switch landed. The writer must not format numbers: under
 * a locale with digit grouping (fr_CA, de_DE, ...) `wcout << 1000` inserts a
 * non-ASCII separator, which the GBK leg cannot encode, and the resulting taint
 * would make the next switch_code() throw for a reason that has nothing to do
 * with locking. sync_with_stdio() may discard
 * input it had buffered when switching back to the unbuffered mode (documented in
 * streambuf.h), so the reader's total is not asserted, only that it reaches EOF
 * and that nothing else went wrong.
 *
 * An extraction holds io_mutex() while it waits in read(), so a reader parked on an
 * empty pipe would hold the toggler off forever. The main thread therefore keeps
 * feeding the pipe until the toggler is done, and only then closes it.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>

namespace
{
    constexpr int kInserts = 4000;
    constexpr int kSwitches = 200;
    constexpr int kChunk = 100;   // lines per feed

    std::string numbered_lines(int n)
    {
        std::string s;
        for (int i = 1; i <= n; ++i) s += std::to_string(i) + '\n';
        return s;
    }

    // Feeds the pipe until `done` is set, then closes it so the reader hits EOF.
    void feed_until_done(pipe_iguard& in, const std::atomic<bool>& done)
    {
        const std::string chunk = numbered_lines(kChunk);
        while (!done.load()) in.feed(chunk);
        in.close_write();
    }

    const char* const kCodeA = "en_US.UTF-8";
    const char* const kCodeB = "zh_CN.GBK";
}

TEST(StdObjectsSync, WcoutSwitchCodeAndResetAgainstInsertion)
{
    oguard<true> out;
    IOv2::wcout.reset();
    const std::string original = IOv2::wcout.switch_code(kCodeA);

    std::thread writer([] {
        for (int i = 0; i < kInserts; ++i)
            IOv2::wcout << L"ab" << static_cast<wchar_t>(L'0' + i % 10) << L'\n';
    });
    std::thread switcher([] {
        for (int i = 0; i < kSwitches; ++i)
        {
            IOv2::wcout.switch_code((i & 1) != 0 ? kCodeB : kCodeA);
            (void)IOv2::wcout.code();
            if (i % 50 == 0) IOv2::wcout.reset();
        }
    });
    writer.join();
    switcher.join();

    const std::string code = IOv2::wcout.code();
    EXPECT_TRUE(code == kCodeA || code == kCodeB) << code;
    EXPECT_TRUE(IOv2::wcout.good());

    IOv2::wcout.switch_code(original);
}

TEST(StdObjectsSync, CinSyncWithStdioAndResetAgainstExtraction)
{
    pipe_iguard in(numbered_lines(kChunk));
    IOv2::cin.reset();
    std::atomic<bool> done{false};

    std::thread reader([] {
        int x = 0;
        while (IOv2::cin >> x) {}
    });
    std::thread toggler([&done] {
        for (int i = 0; i < kSwitches; ++i)
        {
            IOv2::cin.sync_with_stdio((i & 1) == 0);
            if (i % 50 == 0) IOv2::cin.reset();
        }
        done = true;
    });
    feed_until_done(in, done);
    toggler.join();
    reader.join();

    EXPECT_TRUE(IOv2::cin.eof());
    IOv2::cin.sync_with_stdio(true);
    IOv2::cin.reset();
}

TEST(StdObjectsSync, WcinSwitchCodeAndSyncWithStdioAgainstExtraction)
{
    pipe_iguard in(numbered_lines(kChunk));
    IOv2::wcin.reset();
    const std::string original = IOv2::wcin.switch_code(kCodeA);
    std::atomic<bool> done{false};

    std::thread reader([] {
        int x = 0;
        while (IOv2::wcin >> x) {}
    });
    std::thread switcher([&done] {
        for (int i = 0; i < kSwitches; ++i)
        {
            IOv2::wcin.switch_code((i & 1) != 0 ? kCodeB : kCodeA);
            (void)IOv2::wcin.code();
            IOv2::wcin.sync_with_stdio((i & 2) == 0);
        }
        done = true;
    });
    feed_until_done(in, done);
    switcher.join();
    reader.join();

    EXPECT_TRUE(IOv2::wcin.eof());
    const std::string code = IOv2::wcin.code();
    EXPECT_TRUE(code == kCodeA || code == kCodeB) << code;

    IOv2::wcin.sync_with_stdio(true);
    IOv2::wcin.reset();
    IOv2::wcin.switch_code(original);
}
