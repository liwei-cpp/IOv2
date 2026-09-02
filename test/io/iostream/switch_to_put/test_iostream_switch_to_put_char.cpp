// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/comp/zlib_cvt.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/injectable_device.h>
#include <support/no_seek_device.h>

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

namespace
{
    std::string s_e_lit = []()
    {
        std::string e_lit; e_lit.resize(4102);
        for (int i = 0; i < 4102; i += 7)
        {
            e_lit[i+0] = '\xE6';
            e_lit[i+1] = '\x9D';
            e_lit[i+2] = '\x8E';
            e_lit[i+3] = '\xE4';
            e_lit[i+4] = '\xBC';
            e_lit[i+5] = '\x9F';
            e_lit[i+6] = (i / 7) % 127 + 1;
        }
        return e_lit;
    }();
}

TEST(IostreamSwitchToPutChar, SwitchingOnAFreshStreamIsAlwaysAllowed)
{
    {
        IOv2::iostream str(IOv2::mem_device{""});
        str.switch_to_put();
        EXPECT_TRUE(static_cast<bool>(str));
    }

    {
        IOv2::iostream str(IOv2::mem_device{"abcde"});
        str.switch_to_put();
        EXPECT_TRUE(static_cast<bool>(str));
    }
}

TEST(IostreamSwitchToPutChar, APipelineThatCannotSwitchIsRejectedAtTheDeclaration)
{
    IOv2::ostream ostr(IOv2::mem_device{""},
                       IOv2::Comp::zlib_cvt_creator<char>{6});
    ostr << s_e_lit;
    EXPECT_TRUE(static_cast<bool>(ostr));
    auto [dev, err] = ostr.detach();
    std::string compress_res = dev.str();

    EXPECT_FALSE(compress_res.empty());
    EXPECT_TRUE(compress_res.size() < s_e_lit.size());

    // A zlib pipeline cannot change direction (support_io_switch is false), so an iostream over
    // one is rejected at the declaration level by base_streambuf's creator constructor
    // (io_concepts.h: cvt_fits_direction). What this case used to do -- build such an iostream
    // and drive switch_to_put() into a run-time cvtfailbit -- is no longer expressible, so the
    // rejection itself is what gets pinned down here.
    static_assert(!std::is_constructible_v<IOv2::iostream<IOv2::mem_device<char>, char>,
                                           IOv2::mem_device<char>,
                                           IOv2::Comp::zlib_cvt_creator<char>>);
    // The single-direction halves stay legal: zlib supports get and put, just not switching.
    static_assert(std::is_constructible_v<IOv2::ostream<IOv2::mem_device<char>, char>,
                                          IOv2::mem_device<char>,
                                          IOv2::Comp::zlib_cvt_creator<char>>);
    static_assert(std::is_constructible_v<IOv2::istream<IOv2::mem_device<char>, char>,
                                          IOv2::mem_device<char>,
                                          IOv2::Comp::zlib_cvt_creator<char>>);
}

TEST(IostreamSwitchToPutChar, SwitchingAfterWritingThroughAConverterKeepsTheStreamGood)
{
    IOv2::iostream str(IOv2::mem_device{""},
                       IOv2::code_cvt_creator<char, wchar_t>("C"));
    str << L"abcde";
    EXPECT_TRUE(static_cast<bool>(str));

    str.seek(0);
    EXPECT_TRUE(static_cast<bool>(str));

    str.switch_to_put();
    EXPECT_TRUE(static_cast<bool>(str));
}

namespace
{
    // Reads one word out of "ab cdef", which stops at the space and leaves that space in the
    // read buffer. A non-empty read buffer is the precondition for switch_to_put() having to
    // reposition at all, and one ordinary extraction is enough to create it -- no explicit
    // peek() or putback() is needed.
    template <class TStream>
    void read_one_word(TStream& str)
    {
        std::string w;
        str >> w;
        EXPECT_TRUE(static_cast<bool>(str));
        EXPECT_EQ(w, "ab");
    }
}

// switch_to_put() when the DEVICE fails underneath it. Two things a state-bit check alone cannot
// reach: that the repositioning really happens before the read buffer is cleared -- injecting a
// dseek failure cannot be told apart from failing before dseek was reached, hence the call counts
// -- and that the bit is devfailbit, not cvtfailbit, because switch_to_put() catches cvt_error,
// which a device_error does not match, so it travels out unwrapped and the contextual "cannot
// reposition N buffered/put-back character(s)" message is not generated here.
TEST(IostreamSwitchToPutChar, ADeviceFailureDuringTheSwitchIsReportedAsDevfailbit)
{
    // a dseek failure: the seek was reached, exactly one bit is set, and nothing is lost
    {
        injectable_device<char> dev{"ab cdef"};
        auto st = dev.shared_state();
        IOv2::iostream str(std::move(dev));
        read_one_word(str);

        const auto pos_before  = str.tell();
        const auto seek_before = st->dseek;
        const auto tell_before = st->dtell;

        st->fail_dseek = true;
        str.switch_to_put();
        st->fail_dseek = false;

        EXPECT_EQ(st->dseek, seek_before + 1);      // dseek was actually reached
        EXPECT_EQ(st->dtell, tell_before + 1);
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::devfailbit);   // exactly one bit

        str.clear();
        EXPECT_EQ(str.tell(), pos_before);          // position survives the failed switch
        EXPECT_EQ(str.peek(), ' ');                 // and so does the buffered delimiter
    }

    // a dtell failure: the seek must NOT have been reached, which fixes the order
    {
        injectable_device<char> dev{"ab cdef"};
        auto st = dev.shared_state();
        IOv2::iostream str(std::move(dev));
        read_one_word(str);

        const auto seek_before = st->dseek;
        const auto tell_before = st->dtell;

        st->fail_dtell = true;
        str.switch_to_put();
        st->fail_dtell = false;

        EXPECT_EQ(st->dtell, tell_before + 1);
        EXPECT_EQ(st->dseek, seek_before);          // tell comes first, and it never got past it
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::devfailbit);
    }

    // a character substituted by putback() is preserved just the same
    {
        injectable_device<char> dev{"ab cdef"};
        auto st = dev.shared_state();
        IOv2::iostream str(std::move(dev));
        read_one_word(str);
        str.putback('Z');

        const auto seek_before = st->dseek;
        st->fail_dseek = true;
        str.switch_to_put();
        st->fail_dseek = false;

        EXPECT_EQ(st->dseek, seek_before + 1);
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::devfailbit);

        str.clear();
        EXPECT_EQ(str.peek(), 'Z');
    }

    // on an already-failed stream the call returns early: it touches neither primitive
    {
        injectable_device<char> dev{"ab cdef"};
        auto st = dev.shared_state();
        IOv2::iostream str(std::move(dev));
        read_one_word(str);

        st->fail_dseek = true;
        str.switch_to_put();                       // fails, leaving the stream in a bad state
        st->fail_dseek = false;
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::devfailbit);

        const auto state_before = str.rdstate();
        const auto seek_before  = st->dseek;
        const auto tell_before  = st->dtell;

        str.switch_to_put();                       // second call, stream still failed

        EXPECT_EQ(str.rdstate(), state_before);
        EXPECT_EQ(st->dseek, seek_before);
        EXPECT_EQ(st->dtell, tell_before);
    }
}

// switch_to_put() when the CONVERTER cannot reposition -- the other half of the same
// @warning in iostream.h. no_seek_device gets there without any fake converter kernel; see
// the comment on that device for why. Here the bit is cvtfailbit, and because the throw comes
// before the read buffer is cleared, the buffered content is kept rather than discarded.
TEST(IostreamSwitchToPutChar, AConverterFailureDuringTheSwitchIsReportedAsCvtfailbit)
{
    // a non-empty read buffer forces a reposition the pipeline cannot do
    {
        IOv2::iostream str(no_seek_device<char>{"ab cdef"});
        read_one_word(str);

        str.switch_to_put();
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::cvtfailbit);   // exactly one bit

        str.clear();
        EXPECT_EQ(str.peek(), ' ');   // thrown before the clear, so the delimiter is still there
    }

    // an empty read buffer needs no reposition at all, so the switch succeeds
    {
        IOv2::iostream str(no_seek_device<char>{"abcdef"});
        str.switch_to_put();
        EXPECT_TRUE(static_cast<bool>(str));
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::goodbit);
    }

    // clear() really clears it: the next switch sets the bit again rather than sticking
    {
        IOv2::iostream str(no_seek_device<char>{"ab cdef"});
        read_one_word(str);

        str.switch_to_put();
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::cvtfailbit);
        str.clear();
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::goodbit);
        str.switch_to_put();
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::cvtfailbit);
    }

    // the original exception is stored, so what comes back out carries the context of the
    // failure rather than clear()'s generic fallback message
    {
        IOv2::iostream str(no_seek_device<char>{"ab cdef"});
        read_one_word(str);
        str.switch_to_put();
        EXPECT_EQ(str.rdstate(), IOv2::ios_defs::cvtfailbit);

        bool threw = false;
        try
        {
            str.exceptions(IOv2::ios_defs::cvtfailbit);
        }
        catch (const IOv2::cvt_error& e)
        {
            threw = true;
            EXPECT_NE(std::string(e.what()).find("cannot reposition"), std::string::npos);
        }
        EXPECT_TRUE(threw);
    }
}
