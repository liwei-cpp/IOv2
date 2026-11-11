// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A get_main() that fails after it has delivered characters hands them over, and the
 * next get throws the error. Each converter below calls the layer below more than once
 * per get, so a device error on a later call used to throw away what the earlier calls
 * had already decoded -- and iochannel::getn reported 0 for characters it had written.
 *
 * The device stops short of a chosen byte, fails once when asked for it, then carries on
 * from that byte, so the whole text must still come out in order.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/cvt/crypt/chacha20_cvt.h>
#include <IOv2/cvt/crypt/vigenere_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <support/fail_once_device.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

namespace
{
    std::string sample()
    {
        std::string out;
        for (int i = 0; i < 60; ++i)
            out += "the quick brown fox " + std::to_string(i) + "\n";
        return out;
    }

    template <typename Writer>
    std::string encode(Writer&& w, const std::string& plain)
    {
        EXPECT_EQ(w.bos(), io_status::output);
        w.main_cont_beg();
        w.put(plain.data(), plain.size());
        auto [dev, err] = w.detach();
        EXPECT_FALSE(err);
        return dev.str();
    }
}

TEST(GetMainDeferredError, Vigenere)
{
    const std::string plain = sample();
    const std::string ext = encode(Crypt::Classic::vigenere_cvt{no_rb_root_cvt{mem_device("")}, "liwei"}, plain);
    Crypt::Classic::vigenere_cvt obj{no_rb_root_cvt{fail_once_device(ext, ext.size() / 2)}, "liwei"};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, plain);
}

TEST(GetMainDeferredError, Chacha20)
{
    const std::string plain = sample();
    const std::string ext = encode(Crypt::chacha20_cvt{no_rb_root_cvt{mem_device("")}, "liwei"}, plain);
    Crypt::chacha20_cvt obj{no_rb_root_cvt{fail_once_device(ext, ext.size() / 2)}, "liwei"};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, plain);
}

TEST(GetMainDeferredError, Zlib)
{
    const std::string plain = sample();
    const std::string ext = encode(Comp::zlib_cvt{no_rb_root_cvt{mem_device("")}}, plain);
    Comp::zlib_cvt obj{no_rb_root_cvt{fail_once_device(ext, ext.size() / 2)}};
    EXPECT_EQ(obj.bos(), io_status::input);
    expect_the_prefix_then_the_error_then_the_rest(obj, plain);
}

