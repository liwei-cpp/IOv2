/**
 * Positioning an ostream<wchar_t>: seek, rseek and tell.
 *
 * The narrow file states the contract; what is specific here is that positions
 * are counted in characters while the device counts its own units, and the two
 * only agree when the conversion is one unit per character. tell() answers in
 * characters either way, but a seek has to be turned into a device position,
 * and a multi-byte encoding cannot do that without rescanning from the start --
 * so it refuses, through cvtfailbit, rather than landing somewhere wrong.
 */
#include <cvt/code_cvt.h>
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/char_and_str.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <string>

using namespace IOv2;

TEST(OstreamSeekWchar, SeekPutsTheNextWriteWhereItWasTold)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{L""});
        os << L"abcdef";

        os.seek(2);
        os << L"XY";

        EXPECT_EQ(os.device().str(), L"abXYef");
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// rseek counts from the end, so rseek(2) on six characters lands on the fifth.
TEST(OstreamSeekWchar, RseekIsMeasuredFromTheEndNotFromHere)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{L""});
        os << L"abcdef";

        os.rseek(2);
        os << L"XY";

        EXPECT_EQ(os.device().str(), L"abcdXY");
        EXPECT_TRUE(os.good());
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

// Positions count characters, so multi-byte content must not shift them.
TEST(OstreamSeekWchar, TellCountsCharactersNotBytes)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{L""});

        os << L"中é漢";
        ASSERT_TRUE(os.tell().has_value());
        EXPECT_EQ(os.tell().value(), 3u);

        os.seek(1);
        EXPECT_EQ(os.tell().value(), 1u);

        os << L"z";
        EXPECT_EQ(os.tell().value(), 2u);
        EXPECT_EQ(os.device().str(), L"中z漢");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamSeekWchar, SeekPastTheEndIsRefusedAndTakesTheWriteWithIt)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os(mem_device{L""});
        os << L"abcdef";

        // The seek itself is refused by the device.
        os.seek(10);
        EXPECT_EQ(os.rdstate(), ios_defs::devfailbit);

        // The write that follows is then dropped, and says so in its own bit.
        os << L"Z";
        EXPECT_TRUE(os.str_fail());
        EXPECT_EQ(os.device().str(), L"abcdef");
    };

    helper.operator()<ostream>();
    helper.operator()<iostream>();
}

TEST(OstreamSeekWchar, SeekingClearsEofbit)
{
    iostream ios(mem_device{std::wstring(L"ab")});

    wchar_t c = 0;
    while (ios.get(c))
        ;
    ASSERT_TRUE(ios.eof());

    // Reading past the end left strfailbit set as well; seek clears eofbit and
    // nothing else, so the stream is still failed and still refuses writes.
    ios.seek(0);
    EXPECT_FALSE(ios.eof());
    EXPECT_TRUE(ios.rdstate() & ios_defs::strfailbit);

    ios << L"XY";
    EXPECT_EQ(ios.device().str(), L"ab");

    ios.clear();
    ios << L"XY";
    EXPECT_EQ(ios.device().str(), L"XY");
}

// One device unit per character: the converter can turn a character position
// into a device position by multiplying, so seeking works as it does without
// one.
TEST(OstreamSeekWchar, AConvertingStreamSeeksWhenTheEncodingIsOneUnitPerCharacter)
{
    const std::string path = "test_ostream_seek_single_byte.txt";
    file_guard        guard(path);

    {
        ostream os(ofile_device<char>{path, file_open_flag::trunc},
                   code_cvt_creator<char, wchar_t>("C"));
        ASSERT_TRUE(static_cast<bool>(os));

        os << L"abcdef";
        ASSERT_TRUE(os.tell().has_value());
        EXPECT_EQ(os.tell().value(), 6u);

        os.seek(2);
        EXPECT_TRUE(os.good());
        os << L"XY";
        EXPECT_TRUE(os.good());

        auto [dev, err] = os.detach();
        dev.close();
    }

    istream is{ifile_device<char>{path}};
    ASSERT_TRUE(static_cast<bool>(is));

    std::string got(16, '\0');
    char*       end = is.read(got.data(), static_cast<std::ptrdiff_t>(got.size()));
    got.resize(static_cast<std::size_t>(end - got.data()));
    EXPECT_EQ(got, "abXYef");
}

// A multi-byte encoding cannot: it refuses with cvtfailbit and stays where it
// was, rather than seeking to a byte that may be inside a character. tell()
// still answers, because counting what has been written costs nothing.
TEST(OstreamSeekWchar, AMultiByteEncodingRefusesToSeek)
{
    ostream os(mem_device{""}, code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
    ASSERT_TRUE(static_cast<bool>(os));

    os << L"中é";
    os.flush();
    ASSERT_TRUE(os.tell().has_value());
    EXPECT_EQ(os.tell().value(), 2u);
    EXPECT_EQ(os.device().str().size(), 5u);        // two characters, five bytes

    os.seek(0);
    EXPECT_TRUE(os.rdstate() & ios_defs::cvtfailbit);
    EXPECT_FALSE(os.tell().has_value());

    // The refusal left the device untouched, and the write that follows is
    // dropped along with it because the stream is now failed.
    os << L"zz";
    os.flush();
    EXPECT_EQ(os.device().str().size(), 5u);
}
