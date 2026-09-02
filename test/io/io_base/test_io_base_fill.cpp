// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ios_base::fill: where the character comes from, and which characters are
 * allowed to be it.
 *
 * The fill is stream state, not locale state. A stream takes its initial fill
 * from the locale it is built with, but from then on it is the caller's to set,
 * and changing the locale must not reach back in and change it -- which is what
 * the first two tests pin down, using a ctype whose widen(' ') is deliberately
 * not a space so that a re-derived fill would be visible.
 *
 * The rest is the vetting rule: a fill is refused when it would change the
 * number the padded field reads as. Which characters those are depends on the
 * base, because under hex the letters a-f are digits, and on where the padding
 * lands, because a run past the value can never be read back into it.
 */
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace IOv2;

namespace
{
    // widen(' ') answers with a tab, so a fill that were re-derived from the
    // locale would stop being a space and the difference would be visible.
    template <typename C>
    struct tabby_ctype : ctype_conf<C>
    {
        tabby_ctype() : ctype_conf<C>("C") {}
        C widen(char c) const override { return (c == ' ') ? C('\t') : C(c); }
    };
}

TEST(IosBaseFill, ChangingTheLocaleDoesNotChangeTheFill)
{
    ostream out{mem_device{""}};

    ASSERT_EQ(out.fill(), ' ');

    // A locale whose ctype would widen ' ' differently leaves the fill alone.
    out.locale(locale<char>().involve(std::make_shared<tabby_ctype<char>>()));
    EXPECT_EQ(out.fill(), ' ');

    // And a fill the caller chose survives going back to an ordinary locale.
    out.fill('*');
    out.locale(locale<char>{});
    EXPECT_EQ(out.fill(), '*');
}

TEST(IosBaseFill, TheSameHoldsForAWideStream)
{
    ostream out{mem_device{L""}};

    ASSERT_EQ(out.fill(), L' ');

    out.locale(locale<wchar_t>().involve(std::make_shared<tabby_ctype<wchar_t>>()));
    EXPECT_EQ(out.fill(), L' ');

    out.fill(L'*');
    out.locale(locale<wchar_t>{});
    EXPECT_EQ(out.fill(), L'*');
}

// A fill character is rejected only where padding is actually written, and the
// rejection reaches the stream as an ordinary formatting failure: strfailbit, and
// an exception only if that bit is masked in.
TEST(IosBaseFill, AFillIsVettedOnlyWhereItWouldBeWritten)
{
    ostream out{mem_device{""}};
    out << setfill('0');

    // Nothing to pad, so the sticky fill is never written and never vetted.
    out << 42;
    EXPECT_TRUE(out.good());

    // Zero-padding a negative number to the right would write "00000-42", which
    // reads as no number at all.
    out << setw(8) << right << -42;
    EXPECT_FALSE(out.good());
    EXPECT_TRUE(out.rdstate() & ios_defs::strfailbit);

    out.clear();
    // `internal` puts the same zeros where they read as leading zeros.
    out << setw(8) << internal << -42;
    EXPECT_TRUE(out.good());

    auto [dev, err] = out.detach();
    EXPECT_EQ(dev.str(), "42-0000042");
}

// With strfailbit masked in, the same rejection is reported as an exception.
TEST(IosBaseFill, TheRejectionThrowsWhenStrfailbitIsMasked)
{
    ostream out{mem_device{""}};
    out.exceptions(ios_defs::strfailbit);
    out << setfill('9') << setw(8);

    EXPECT_THROW(out << 42, stream_error);
    EXPECT_TRUE(out.rdstate() & ios_defs::strfailbit);
}

namespace
{
    // Writes `v` into a fresh stream under `base` with the given fill, width and
    // adjustment, and reports whether the insertion was accepted. `text` receives what
    // was written, so an accepted case can be checked for content as well.
    bool fill_accepted(char fill, ios_defs::fmtflags base, ios_defs::fmtflags adjust,
                       unsigned long v, std::string& text, ios_defs::fmtflags extra = {})
    {
        ostream out{mem_device{""}};
        out.setf(base, ios_defs::basefield);
        out.setf(adjust, ios_defs::adjustfield);
        if (extra != ios_defs::fmtflags{})
            out.setf(extra);
        out << setfill(fill) << setw(8) << v;
        const bool ok = out.good();
        auto [dev, err] = out.detach();
        text = dev.str();
        return ok;
    }
}

// Under hex the six letter digits read as part of the value just as '1'-'9' do:
// setfill('f') on 0xab would write "ffffffab", which reads back as 4294967211.
// Both cases are rejected whatever `uppercase` says, because the extractor takes
// mixed-case hex.
TEST(IosBaseFill, UnderHexTheLetterDigitsAreDigitsToo)
{
    std::string text;

    for (char f : {'a', 'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C', 'D', 'E', 'F'})
        for (auto adjust : {ios_defs::right, ios_defs::left, ios_defs::internal})
        {
            EXPECT_FALSE(fill_accepted(f, ios_defs::hex, adjust, 0xab, text)) << "fill " << f;
            EXPECT_FALSE(fill_accepted(f, ios_defs::hex, adjust, 0xab, text,
                                       ios_defs::uppercase))
                << "fill " << f << " with uppercase";
        }
}

// '0' keeps working where it reads as a leading zero, so the tightening above did
// not swallow the one digit that is legitimate.
TEST(IosBaseFill, ZeroStaysUsableWhereItReadsAsALeadingZero)
{
    std::string text;

    EXPECT_TRUE(fill_accepted('0', ios_defs::hex, ios_defs::right, 0xab, text));
    EXPECT_EQ(text, "000000ab");
    EXPECT_TRUE(fill_accepted('0', ios_defs::hex, ios_defs::internal, 0xab, text));
    EXPECT_EQ(text, "000000ab");
    EXPECT_FALSE(fill_accepted('0', ios_defs::hex, ios_defs::left, 0xab, text));

    // With showbase the prefix sits between a right-adjusted fill and the digits, so
    // only `internal` still reads as leading zeros.
    EXPECT_FALSE(fill_accepted('0', ios_defs::hex, ios_defs::right, 0xab, text,
                               ios_defs::showbase));
    EXPECT_TRUE(fill_accepted('0', ios_defs::hex, ios_defs::internal, 0xab, text,
                              ios_defs::showbase));
    EXPECT_EQ(text, "0x0000ab");
}

TEST(IosBaseFill, TheDigitSetFollowsTheBase)
{
    std::string text;

    // Outside hex those letters are not digits and stay usable.
    EXPECT_TRUE(fill_accepted('f', ios_defs::dec, ios_defs::right, 42, text));
    EXPECT_EQ(text, "ffffff42");
    EXPECT_TRUE(fill_accepted('f', ios_defs::oct, ios_defs::right, 0777, text));
    EXPECT_EQ(text, "fffff777");

    // The test asks whether a character looks like a digit to a reader, not whether the
    // base would accept it: '8' and '9' are not octal digits but "88888777" still reads
    // as a number, so they stay rejected under oct.
    for (char f : {'8', '9'})
        for (auto adjust : {ios_defs::right, ios_defs::left, ios_defs::internal})
            EXPECT_FALSE(fill_accepted(f, ios_defs::oct, adjust, 0777, text)) << "fill " << f;

    // Ordinary non-digit fills are untouched in every base.
    for (auto base : {ios_defs::dec, ios_defs::oct, ios_defs::hex})
    {
        EXPECT_TRUE(fill_accepted('*', base, ios_defs::right, 42, text));
        EXPECT_TRUE(fill_accepted(' ', base, ios_defs::right, 42, text));
    }
}

// The atom table is widened through the same ctype, so the criterion has to hold on
// a wide stream too; char is the only width the cases above cover.
TEST(IosBaseFill, TheSameCriterionHoldsOnAWideStream)
{
    {
        ostream out{mem_device{L""}};
        out.setf(ios_defs::hex, ios_defs::basefield);
        out << setfill(L'f') << setw(8) << 0xabUL;
        EXPECT_FALSE(out.good());
        EXPECT_TRUE(out.rdstate() & ios_defs::strfailbit);
    }
    {
        ostream out{mem_device{L""}};
        out.setf(ios_defs::hex, ios_defs::basefield);
        out << setfill(L'0') << setw(8) << 0xabUL;
        EXPECT_TRUE(out.good());
        auto [dev, err] = out.detach();
        EXPECT_EQ(dev.str(), L"000000ab");
    }
    {
        ostream out{mem_device{L""}};
        out << setfill(L'f') << setw(8) << 42UL;
        EXPECT_TRUE(out.good());
        auto [dev, err] = out.detach();
        EXPECT_EQ(dev.str(), L"ffffff42");
    }
}
