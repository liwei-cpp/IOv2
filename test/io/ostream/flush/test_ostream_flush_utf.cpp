// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * flush on the two character types the char and wchar_t files do not reach.
 *
 * flush carries no character of its own, so the per-type claims are narrow: that the buffered
 * text reaches the device intact whatever its code-unit width, and that a device refusing to
 * flush still lands on devfailbit rather than on some type-dependent path. Everything else about
 * flush -- idempotence, tie cycles, concurrency -- is character-type-independent and stays in the
 * char and wchar_t files.
 *
 * locale<char8_t>("C") throws from collate_conf and a default-constructed one follows the
 * environment, so the char8_t cases name "C.UTF-8" explicitly.
 */
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/failing_device.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(OstreamFlushUtf, BufferedTextReachesTheDeviceIntact)
{
    {
        auto os = ostream(mem_device{std::u8string()}, locale<char8_t>("C.UTF-8"));
        os << u8"中é漢字ξ";
        os.flush();

        EXPECT_TRUE(os.good());
        EXPECT_EQ(os.device().str(), std::u8string(u8"中é漢字ξ"));
        EXPECT_EQ(os.device().str().size(), 3u + 2u + 3u + 3u + 2u);
    }
    {
        auto os = ostream(mem_device{std::u32string()}, locale<char32_t>("C"));
        os << U"中é漢字ξ";
        os.flush();

        EXPECT_TRUE(os.good());
        EXPECT_EQ(os.device().str(), std::u32string(U"中é漢字ξ"));
        EXPECT_EQ(os.device().str().size(), 5u);
    }
}

TEST(OstreamFlushUtf, FlushingWithNothingPendingLeavesTheDeviceAlone)
{
    const std::u8string seeded(u8"already there");

    auto os = ostream(mem_device{seeded}, locale<char8_t>("C.UTF-8"));
    os.flush();

    EXPECT_TRUE(os.good());
    EXPECT_EQ(os.device().str(), seeded);
}

// The device's refusal reaches devfailbit the same way it does on the narrow types; nothing about
// the code-unit width changes which bit is set.
TEST(OstreamFlushUtf, ADeviceThatRefusesToFlushIsReportedAsDevfailbit)
{
    {
        ostream out(failing_device<char8_t>{std::u8string(), true}, locale<char8_t>("C.UTF-8"));
        out << u8"abc";

        EXPECT_NO_THROW(out.flush());
        EXPECT_TRUE(out.rdstate() & ios_defs::devfailbit);
    }
    {
        ostream out(failing_device<char32_t>{std::u32string(), true}, locale<char32_t>("C"));
        out << U"abc";

        EXPECT_NO_THROW(out.flush());
        EXPECT_TRUE(out.rdstate() & ios_defs::devfailbit);
    }
}
