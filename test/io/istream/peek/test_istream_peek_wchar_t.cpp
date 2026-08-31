/**
 * The same peek contract as test_istream_peek_char.cpp for wchar_t: peek does
 * not consume, does not move the read position, and does not change the state
 * except at the end of the input, where an empty optional and eofbit are the
 * report.
 *
 * The file case is the one that differs. file_guard writes bytes, so a wide
 * stream over a file is a narrow device with a converter on top -- which also
 * makes it the case where "the position did not move" has to hold across a
 * conversion rather than over raw bytes.
 */
#include <cvt/code_cvt.h>
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/char_and_str.h>
#include <locale/locale.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>

#include <string>

using namespace IOv2;

namespace
{
    const std::wstring kDigits = L"0123456789abcdef";
}

TEST(IstreamPeekWchar, PeekReportsTheNextCharacterWithoutConsumingIt)
{
    auto expect_non_destructive = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        EXPECT_EQ(is.peek(), L'0');
        EXPECT_EQ(is.peek(), L'0');
        EXPECT_EQ(is.peek(), L'0');

        EXPECT_EQ(is.get(), L'0');
        EXPECT_EQ(is.peek(), L'1');
    };

    expect_non_destructive.operator()<istream>();
    expect_non_destructive.operator()<iostream>();
}

TEST(IstreamPeekWchar, PeekLeavesTheStateAlone)
{
    auto expect_state_kept = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        const ios_defs::iostate before = is.rdstate();
        EXPECT_EQ(is.peek(), L'0');
        EXPECT_EQ(is.rdstate(), before);
    };

    expect_state_kept.operator()<istream>();
    expect_state_kept.operator()<iostream>();
}

TEST(IstreamPeekWchar, PeekReportsWhateverTheCursorIsOn)
{
    auto expect_follows_cursor = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        wchar_t buf[8] = {};
        is.read(buf, 4);
        EXPECT_EQ(is.peek(), L'4');

        is.ignore();
        EXPECT_EQ(is.peek(), L'5');

        is.ignore(0);
        EXPECT_EQ(is.peek(), L'5');

        is.ignore(6, L'8');
        EXPECT_EQ(is.peek(), L'9');
    };

    expect_follows_cursor.operator()<istream>();
    expect_follows_cursor.operator()<iostream>();
}

TEST(IstreamPeekWchar, PeekAtTheEndReportsNothingAndSetsEndOfFile)
{
    auto expect_end = []<template <typename, typename> class T>()
    {
        T empty{mem_device{std::wstring(L"")}};
        EXPECT_EQ(empty.rdstate(), ios_defs::goodbit);
        EXPECT_FALSE(empty.peek().has_value());
        EXPECT_EQ(empty.rdstate(), ios_defs::eofbit);

        T drained(mem_device{kDigits});
        drained.ignore(kDigits.size());
        EXPECT_FALSE(drained.peek().has_value());
        EXPECT_TRUE(drained.eof());
    };

    expect_end.operator()<istream>();
    expect_end.operator()<iostream>();
}

TEST(IstreamPeekWchar, PeekDoesNotMoveTheReadPosition)
{
    const std::string path = "test_istream_peek_position_wide.txt";
    file_guard        guard(path, std::string("0123456789abcdef"));

    auto expect_still = [&path]<template <typename, typename> class T, typename TDevice>()
    {
        T is(TDevice{path}, code_cvt_creator<char, wchar_t>("C"));
        is.seek(0);

        const auto before = is.tell();
        EXPECT_EQ(is.peek(), L'0');
        EXPECT_EQ(is.tell(), before);

        is.seek(10);
        const auto middle = is.tell();
        EXPECT_EQ(is.peek(), L'a');
        EXPECT_EQ(is.tell(), middle);
    };

    expect_still.operator()<istream, ifile_device<char>>();
    expect_still.operator()<iostream, file_device<char>>();
}

TEST(IstreamPeekWchar, PeekAtTheEndThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        T masked{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
        masked.exceptions(ios_defs::eofbit);
        EXPECT_THROW((void)masked.peek(), eof_error);
        EXPECT_TRUE(masked.eof());

        T unmasked{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
        EXPECT_FALSE(unmasked.peek().has_value());
        EXPECT_TRUE(unmasked.eof());
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}
