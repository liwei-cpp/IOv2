// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ostream<wchar_t>::flush.
 *
 * flush pushes whatever the stream is holding at the device and reports what
 * the device says about it. The interesting cases are all about what happens
 * when there is nothing to push, when the device refuses, and when several
 * threads ask at once -- flush is the one output operation callers reach for
 * from more than one thread, so contention on it has to be safe.
 *
 * The tie cases live here because tie is what makes a flush happen implicitly:
 * a cycle among tied streams would make one flush recurse forever, so cycles
 * are refused when the tie is set rather than when it is followed.
 */
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/ostream.h>
#include <io/traits/char_and_str.h>

#include <support/failing_device.h>

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace IOv2;

static_assert(std::is_copy_constructible_v<decltype(ostream(mem_device<wchar_t>{}))>,
              "IOv2::ostream<mem_device<wchar_t>> must remain copy-constructible");
static_assert(std::is_copy_constructible_v<decltype(iostream(mem_device<wchar_t>{}))>,
              "IOv2::iostream<mem_device<wchar_t>> must remain copy-constructible");

TEST(OstreamFlushWchar, FlushingAStreamWithNothingPendingLeavesTheDeviceAlone)
{
    auto helper = []<template <typename, typename> class T>()
    {
        const std::wstring seeded(L"already there");

        T with(mem_device{seeded});
        T without(mem_device{L""});

        with.flush();
        EXPECT_EQ(with.device().str(), seeded);
        EXPECT_TRUE(with.good());

        without.flush();
        EXPECT_TRUE(without.device().str().empty());
        EXPECT_TRUE(without.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamFlushWchar, FlushIsIdempotent)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{L""});
        os << L"payload";

        os.flush();

        const std::wstring after_first = os.device().str();
        os.flush();
        os.flush();
        EXPECT_EQ(os.device().str(), after_first);
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamFlushWchar, ATieCycleIsRejectedWhenTheTieIsSet)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T a(mem_device{L""});
        T b(mem_device{L""});

        // A self-tie is the length-1 cycle and is rejected: strfailbit, no throw by default.
        a.tie(&a);
        EXPECT_TRUE(a.rdstate() & ios_defs::strfailbit);
        EXPECT_EQ(a.tie(), nullptr);
        a.clear();

        a.tie(&b);

        // Closing the cycle a -> b -> a is rejected at set time; b stays untied.
        b.tie(&a);
        EXPECT_TRUE(b.rdstate() & ios_defs::strfailbit);
        EXPECT_EQ(b.tie(), nullptr);
        EXPECT_EQ(a.tie(), &b);
        b.clear();

        a.tie(nullptr);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// An explicit flush() whose device dflush() throws routes the error through the flusher's
// own try/catch into handle_exception -> devfailbit. With no exception mask set it does not
// throw. This drives the catch branch of out_flusher::flush() itself (distinct from the
// out_sentry destructor's unitbuf flush).
TEST(OstreamFlushWchar, ADeviceThatRefusesToFlushIsReportedAsDevfailbit)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T out(failing_device<wchar_t>{std::wstring(L""), true});
        out << L"abc";                 // buffered; not yet flushed to the device

        EXPECT_NO_THROW(out.flush()); // device dflush() throws -> caught -> devfailbit
        EXPECT_TRUE(out.rdstate() & ios_defs::devfailbit);
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamFlushWchar, ConcurrentFlushesDoNotCorruptTheStream)
{
    auto helper = []<template <typename, typename> class T>()
    {
        constexpr size_t thread_num = 8;
        constexpr size_t loop_num   = 2000;

        T                        os{mem_device{L""}};
        std::vector<std::thread> threads;
        threads.reserve(thread_num);

        for (size_t i = 0; i < thread_num; ++i)
            threads.emplace_back([&os]() {
                for (size_t n = 0; n < loop_num; ++n)
                    os.flush();
            });

        for (auto& t : threads)
            t.join();

        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}
