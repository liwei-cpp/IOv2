// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * Windows reads stdin in the CRT's text mode: `\r\n` arrives as `\n`. The
 * device probes with a 1-byte read (deof) and then tops up, so a CR may be the
 * last byte a read sees, with its LF still to come. Real Windows handles that
 * (all three cases pass in CI, with cl, clang-cl, debug, ASan and x86); under
 * Wine the 1-byte _read of a piped CRLF returns 0, taken as EOF -- Wine's
 * emulation, not the UCRT -- so that case skips itself under the local Wine
 * presets (IOV2_TEST_UNDER_WINE), and runs everywhere else.
 *
 * Every case expects all of "a\nb\nc": nothing lost, no early EOF.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/std_device.h>

#include <support/platform.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

namespace
{
    // Reads to EOF the way the stream layer does: probe with deof(), then
    // take what is there.
    std::string read_all_probing(std_input_device& d)
    {
        std::string res;
        char buf[64];
        while (!d.deof())
            res.append(buf, d.dget(buf, sizeof buf));
        return res;
    }

    std::string read_all_bytewise(std_input_device& d)
    {
        std::string res;
        char c = 0;
        while (d.dget(&c, 1) == 1)
            res += c;
        return res;
    }
}

TEST(StdDeviceWindows, APipedCrlfReadsBackWhenProbed)
{
    std::string got;
    {
        pipe_iguard g("a\r\nb\r\nc");
        g.close_write();
        std_input_device d;
        got = read_all_probing(d);
    }
    EXPECT_EQ(got, "a\nb\nc");
}

TEST(StdDeviceWindows, APipedCrlfReadsBackByteByByte)
{
    if (iov2_test::sys::env_is_set("IOV2_TEST_UNDER_WINE"))
        GTEST_SKIP() << "Wine's pipe emulation ends the 1-byte read of a CRLF early";

    std::string got;
    {
        pipe_iguard g("a\r\nb\r\nc");
        g.close_write();
        std_input_device d;
        got = read_all_bytewise(d);
    }
    EXPECT_EQ(got, "a\nb\nc");
}

// The control: the same bytes from a redirected file, where Wine already read
// them correctly. iguard writes in text mode, so "\n" lands on disk as "\r\n".
TEST(StdDeviceWindows, ARedirectedCrlfFileReadsBack)
{
    std::string got;
    {
        iguard g("a\nb\nc");
        std_input_device d;
        got = read_all_probing(d);
    }
    EXPECT_EQ(got, "a\nb\nc");
}
