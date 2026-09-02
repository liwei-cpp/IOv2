// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * boolalpha and the adjustfield manipulators.
 *
 * A bool has two spellings -- a digit, or a word from the numeric facet -- and
 * boolalpha chooses between them. The words are the facet's, not the library's,
 * so a facet that answers differently must change what is written and nothing
 * else about it.
 *
 * The padding half is about what internal means for a value with no sign and no
 * base indication. There is nothing to hold back, so internal has to fall
 * through to right; that is true of a bool in either spelling and of a string,
 * and it is the case an implementation gets wrong by treating internal as a
 * separate branch that forgets to pad at all.
 *
 * The fill is '*' throughout: '.' is the decimal point, and a fill equal to it
 * is refused on the numeric path, which would make the digit cases below fail
 * for a reason that has nothing to do with adjustment.
 */
#include <device/mem_device.h>
#include <facet/numeric_details.h>
#include <io/io_base.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace IOv2;

namespace
{
    // A facet whose words are nothing like "true"/"false", so a hard-coded pair
    // anywhere in the formatting path shows up.
    struct switch_words : numeric_conf<char>
    {
        switch_words() : numeric_conf<char>("C") {}
        const std::string& truename() const override { return m_on; }
        const std::string& falsename() const override { return m_off; }

    private:
        const std::string m_on  = "on";
        const std::string m_off = "off";
    };

    struct wide_switch_words : numeric_conf<wchar_t>
    {
        wide_switch_words() : numeric_conf<wchar_t>("C") {}
        const std::wstring& truename() const override { return m_on; }
        const std::wstring& falsename() const override { return m_off; }

    private:
        const std::wstring m_on  = L"on";
        const std::wstring m_off = L"off";
    };

    template <typename C, typename TFn>
    std::basic_string<C> formatted(const locale<C>& loc, TFn&& apply)
    {
        ostream os{mem_device{std::basic_string<C>{}}, loc};
        apply(os);
        auto [dev, err] = os.detach();
        return dev.str();
    }
}

TEST(IoBaseManipAdjustfield, BoolalphaChoosesBetweenTheDigitAndTheWord)
{
    const locale<char> c("C");

    EXPECT_EQ(formatted<char>(c, [](auto& os) { os << true << ' ' << false; }), "1 0");
    EXPECT_EQ(formatted<char>(c, [](auto& os) { os << boolalpha << true << ' ' << false; }),
              "true false");

    // The flag is not a one-shot: it can be turned back off and on again.
    EXPECT_EQ(formatted<char>(c, [](auto& os) {
                  os << boolalpha << true << ' ' << noboolalpha << true << ' '
                     << boolalpha << false;
              }),
              "true 1 false");
}

TEST(IoBaseManipAdjustfield, TheWordsComeFromTheFacet)
{
    const locale<char> mine = locale<char>("C").involve(std::make_shared<switch_words>());

    EXPECT_EQ(formatted<char>(mine, [](auto& os) { os << boolalpha << true << ' ' << false; }),
              "on off");

    // Without boolalpha the facet's words are not consulted at all.
    EXPECT_EQ(formatted<char>(mine, [](auto& os) { os << true << ' ' << false; }), "1 0");
}

// Nothing to pin, so internal lands where right does -- for a word, for a digit,
// and for a string, all three of which reach different formatting paths.
TEST(IoBaseManipAdjustfield, InternalFallsThroughToRightWhenThereIsNothingToPin)
{
    const locale<char> c("C");

    auto in_field = [&c](ios_defs::fmtflags adjust, bool words, bool v) {
        return formatted<char>(c, [&](auto& os) {
            if (words)
                os << boolalpha;
            os << setw(6) << setfill('*');
            os.setf(adjust, ios_defs::adjustfield);
            os << v;
        });
    };

    EXPECT_EQ(in_field(ios_defs::right, true, true), "**true");
    EXPECT_EQ(in_field(ios_defs::internal, true, true), "**true");
    EXPECT_EQ(in_field(ios_defs::left, true, true), "true**");
    EXPECT_EQ(in_field(ios_defs::right, true, false), "*false");

    EXPECT_EQ(in_field(ios_defs::right, false, true), "*****1");
    EXPECT_EQ(in_field(ios_defs::internal, false, true), "*****1");
    EXPECT_EQ(in_field(ios_defs::left, false, true), "1*****");

    auto string_in_field = [&c](ios_defs::fmtflags adjust) {
        return formatted<char>(c, [adjust](auto& os) {
            os << setw(6) << setfill('*');
            os.setf(adjust, ios_defs::adjustfield);
            os << "north";
        });
    };

    EXPECT_EQ(string_in_field(ios_defs::right), "*north");
    EXPECT_EQ(string_in_field(ios_defs::internal), "*north");
    EXPECT_EQ(string_in_field(ios_defs::left), "north*");
}

TEST(IoBaseManipAdjustfield, TheSameRulesHoldOnAWideStream)
{
    const locale<wchar_t> c("C");
    const locale<wchar_t> mine =
        locale<wchar_t>("C").involve(std::make_shared<wide_switch_words>());

    EXPECT_EQ(formatted<wchar_t>(c, [](auto& os) { os << true << L' ' << false; }), L"1 0");
    EXPECT_EQ(formatted<wchar_t>(c, [](auto& os) { os << boolalpha << true << L' ' << false; }),
              L"true false");
    EXPECT_EQ(formatted<wchar_t>(mine,
                                 [](auto& os) { os << boolalpha << true << L' ' << false; }),
              L"on off");

    auto in_field = [&c](ios_defs::fmtflags adjust) {
        return formatted<wchar_t>(c, [adjust](auto& os) {
            os << boolalpha << setw(6) << setfill(L'*');
            os.setf(adjust, ios_defs::adjustfield);
            os << true;
        });
    };

    EXPECT_EQ(in_field(ios_defs::right), L"**true");
    EXPECT_EQ(in_field(ios_defs::internal), L"**true");
    EXPECT_EQ(in_field(ios_defs::left), L"true**");
}
