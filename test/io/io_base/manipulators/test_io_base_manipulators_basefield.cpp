// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The basefield manipulators, and how a base interacts with everything else the
 * numeric facet does.
 *
 * Three rules meet here and can each be got wrong independently. showbase
 * prepends "0" for octal and "0x" for hex. Grouping applies to the digits and
 * not to that prefix, so the separator never lands between the "0x" and the
 * first digit. And internal adjustment puts the fill between the prefix and the
 * digits -- where there is a base indication to hold back at all, which "0x" is
 * and octal's leading "0" is not. That is also the only adjustment for which a
 * '0' fill is legal at all --
 * under left it would trail the value and under right it would sit in front of
 * the prefix, and both change what the field reads as.
 *
 * The separator here is '_' rather than a space so that a missing separator and
 * a padding space cannot be confused for one another.
 */
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace IOv2;

namespace
{
    template <typename C>
    struct grouped_conf : numeric_conf<C>
    {
        grouped_conf() : numeric_conf<C>("C") {}
        const std::vector<std::uint8_t>& grouping() const override { return m_grouping; }
        C thousands_sep() const override { return C('_'); }

    private:
        std::vector<std::uint8_t> m_grouping = {3};
    };

    template <typename C>
    locale<C> grouped()
    {
        return locale<C>("C").involve(std::make_shared<grouped_conf<C>>());
    }

    // Formats one value into a fresh stream, so no format flag leaks between cases.
    template <typename C, typename TFn>
    std::basic_string<C> formatted(TFn&& apply)
    {
        ostream os{mem_device{std::basic_string<C>{}}, grouped<C>()};
        apply(os);
        auto [dev, err] = os.detach();
        return dev.str();
    }
}

TEST(IoBaseManipBasefield, ShowbaseWritesAZeroForOctalAndZeroXForHex)
{
    EXPECT_EQ(formatted<char>([](auto& os) { os << oct << showbase << 0123456l; }),
              "0123_456");
    EXPECT_EQ(formatted<char>([](auto& os) { os << hex << showbase << 0x12345678l; }),
              "0x12_345_678");

    // Without showbase there is no prefix, and the grouping is unchanged.
    EXPECT_EQ(formatted<char>([](auto& os) { os << oct << 0123456l; }), "123_456");
    EXPECT_EQ(formatted<char>([](auto& os) { os << hex << 0x12345678l; }), "12_345_678");
}

// Grouping counts the digits, not the prefix: 123456789 is nine octal digits, so
// they group evenly and the "0" simply sits in front of the first group.
TEST(IoBaseManipBasefield, TheBasePrefixTakesNoPartInGrouping)
{
    EXPECT_EQ(formatted<char>([](auto& os) { os << oct << showbase << 123456789l; }),
              "0726_746_425");
    EXPECT_EQ(formatted<char>([](auto& os) { os << oct << showbase << 1234567l; }),
              "04_553_207");
}

TEST(IoBaseManipBasefield, AdjustfieldDecidesWhereTheFieldPaddingGoes)
{
    // The fill is '*' rather than '.': a fill equal to the decimal point is
    // refused for right and internal, which is its own rule and not this one.
    auto in_field = [](ios_defs::fmtflags adjust) {
        return formatted<char>([adjust](auto& os) {
            os << oct << showbase << setw(12) << setfill('*');
            os.setf(adjust, ios_defs::adjustfield);
            os << 01234567l;
        });
    };

    EXPECT_EQ(in_field(ios_defs::right), "**01_234_567");
    EXPECT_EQ(in_field(ios_defs::left), "01_234_567**");

    // Octal's leading "0" is a digit, not a separate base indication, so there is
    // nothing for internal to pin and it lands where right does.
    EXPECT_EQ(in_field(ios_defs::internal), "**01_234_567");

    // "0x" is a base indication, and that is what internal holds back.
    EXPECT_EQ(formatted<char>([](auto& os) {
                  os << hex << showbase << setw(14) << internal << setfill('*')
                     << 0x12345678l;
              }),
              "0x**12_345_678");
}

// '0' only pads a hex value where it lands between the "0x" and the digits, i.e.
// under `internal`. Padding `left` would append "0000" to the value and padding
// `right` would put it in front of the "0x", and both are rejected outright.
TEST(IoBaseManipBasefield, AZeroFillIsOnlyLegalUnderInternal)
{
    auto attempt = [](ios_defs::fmtflags adjust, std::string& text) {
        ostream os{mem_device{""}, grouped<char>()};
        os << hex << showbase << setw(16) << setfill('0');
        os.setf(adjust, ios_defs::adjustfield);
        os << 0x12345678l;
        const bool ok = os.good();
        auto [dev, err] = os.detach();
        text = dev.str();
        return ok;
    };

    std::string text;
    EXPECT_TRUE(attempt(ios_defs::internal, text));
    EXPECT_EQ(text, "0x000012_345_678");

    EXPECT_FALSE(attempt(ios_defs::left, text));
    EXPECT_FALSE(attempt(ios_defs::right, text));

    // An ordinary fill works for all three.
    auto spaced = [](ios_defs::fmtflags adjust) {
        return formatted<char>([adjust](auto& os) {
            os << hex << showbase << setw(16) << setfill(' ');
            os.setf(adjust, ios_defs::adjustfield);
            os << 0x12345678l;
        });
    };
    EXPECT_EQ(spaced(ios_defs::left), "0x12_345_678    ");
    EXPECT_EQ(spaced(ios_defs::right), "    0x12_345_678");
}

// Every rule above is the facet's, so it has to hold identically on a wide
// stream; the separator is the one character that could get lost in widening.
TEST(IoBaseManipBasefield, TheSameRulesHoldOnAWideStream)
{
    EXPECT_EQ(formatted<wchar_t>([](auto& os) { os << oct << showbase << 123456789l; }),
              L"0726_746_425");
    EXPECT_EQ(formatted<wchar_t>([](auto& os) { os << hex << showbase << 0x12345678l; }),
              L"0x12_345_678");

    EXPECT_EQ(formatted<wchar_t>([](auto& os) {
                  os << hex << showbase << setw(16) << internal << setfill(L'0')
                     << 0x12345678l;
              }),
              L"0x000012_345_678");

    EXPECT_EQ(formatted<wchar_t>([](auto& os) {
                  os << oct << showbase << setw(12) << left << setfill(L'*') << 01234567l;
              }),
              L"01_234_567**");
}
