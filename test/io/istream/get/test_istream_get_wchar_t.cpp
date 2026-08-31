/**
 * The same get() contract as test_istream_get_char.cpp for wchar_t.
 *
 * The buffered overload is spelled with two policies rather than with the
 * standard's two separate functions. DelimPolicy says what happens to the
 * delimiter -- keep_sep leaves it in the stream, cons_sep takes it out -- and
 * CStrPolicy says whether the result is a C string, which costs one of the
 * capacity for the terminator. The return value is where writing stopped, so
 * the caller measures what it got by subtraction instead of asking gcount().
 *
 * Three ways to stop: the capacity ran out, the delimiter arrived, or the input
 * did. They are distinguishable, and the cases below separate them. A get that
 * extracted nothing at all is a failure whichever of the three it was, which is
 * the one rule that catches a caller who never checks.
 *
 * The fixture is "0123456789abcdef", whose character at index n is n in base
 * 16, so how far a read got and what it wrote check each other.
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

#include <cstddef>
#include <limits>
#include <string>

using namespace IOv2;

namespace
{
    const std::wstring kDigits = L"0123456789abcdef";
}

TEST(IstreamGetWchar, ASingleGetYieldsTheNextCharacterAndAdvances)
{
    auto expect_one_at_a_time = []<template <typename, typename> class T>()
    {
        T is(mem_device{kDigits});

        EXPECT_EQ(is.get(), L'0');
        EXPECT_EQ(is.get(), L'1');

        wchar_t c = L'#';
        is.get(c);
        EXPECT_EQ(c, L'2');
        EXPECT_EQ(is.peek(), L'3');
    };

    expect_one_at_a_time.operator()<istream>();
    expect_one_at_a_time.operator()<iostream>();
}

TEST(IstreamGetWchar, AGetIntoABufferStopsAtTheCapacity)
{
    auto expect_capped = []<template <typename, typename> class T>()
    {
        T    is(mem_device{kDigits});
        wchar_t buf[8] = {};

        wchar_t* end = is.template get<keep_sep, no_zt>(buf, 3);
        EXPECT_EQ(end - buf, 3);
        EXPECT_EQ(std::wstring(buf, end), L"012");

        // The stream is left exactly where writing stopped.
        EXPECT_EQ(is.peek(), L'3');
    };

    expect_capped.operator()<istream>();
    expect_capped.operator()<iostream>();
}

// app_zt reserves one of the capacity for the terminator, so the same n yields
// one character fewer than no_zt does.
TEST(IstreamGetWchar, AppendingATerminatorCostsOneOfTheCapacity)
{
    auto expect_reserved = []<template <typename, typename> class T>()
    {
        T    is(mem_device{kDigits});
        wchar_t buf[8];
        for (wchar_t& c : buf) c = L'#';

        wchar_t* end = is.template get<keep_sep, app_zt>(buf, 3);
        EXPECT_EQ(std::wstring(buf), L"01");   // reads as a C string
        EXPECT_EQ(buf[2], L'\0');
        EXPECT_EQ(buf[3], L'#');              // and no further

        // The returned pointer is past everything written, the terminator
        // included, so it is one more than the character count.
        EXPECT_EQ(end - buf, 3);
        EXPECT_EQ(is.peek(), L'2');
    };

    expect_reserved.operator()<istream>();
    expect_reserved.operator()<iostream>();
}

TEST(IstreamGetWchar, KeepingTheDelimiterLeavesItInTheStream)
{
    auto expect_kept = []<template <typename, typename> class T>()
    {
        T    is(mem_device{std::wstring(L"012\n345")});
        wchar_t buf[8] = {};

        wchar_t* end = is.template get<keep_sep, no_zt>(buf, 7, L'\n');
        EXPECT_EQ(std::wstring(buf, end), L"012");
        EXPECT_EQ(is.peek(), L'\n');          // still there for the next reader
    };

    expect_kept.operator()<istream>();
    expect_kept.operator()<iostream>();
}

TEST(IstreamGetWchar, ConsumingTheDelimiterTakesItOutOfTheStream)
{
    auto expect_consumed = []<template <typename, typename> class T>()
    {
        T    is(mem_device{std::wstring(L"012\n345")});
        wchar_t buf[8] = {};

        wchar_t* end = is.template get<cons_sep, no_zt>(buf, 7, L'\n');
        EXPECT_EQ(std::wstring(buf, end), L"012");   // the delimiter is not written
        EXPECT_EQ(is.peek(), L'3');                 // but it is gone
    };

    expect_consumed.operator()<istream>();
    expect_consumed.operator()<iostream>();
}

// cons_sep promises to consume a delimiter, so a read that filled the buffer
// without reaching one has not done what was asked and says so.
TEST(IstreamGetWchar, ConsumingRequiresTheDelimiterWithinTheCapacity)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        T    is(mem_device{kDigits});
        wchar_t buf[8] = {};

        is.template get<cons_sep, no_zt>(buf, 3, L'\n');
        EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

// Nothing extracted is a failure however it came about: no input at all, or a
// delimiter sitting where the first character would have been.
TEST(IstreamGetWchar, ExtractingNothingIsAFailure)
{
    auto expect_failed = []<template <typename, typename> class T>()
    {
        {
            T    empty(mem_device{std::wstring(L"")});
            wchar_t buf[8] = {};
            wchar_t* end = empty.template get<keep_sep, no_zt>(buf, 4);
            EXPECT_EQ(end, buf);
            EXPECT_TRUE(empty.rdstate() & ios_defs::strfailbit);
        }
        {
            T    at_delim(mem_device{std::wstring(L"\n012")});
            wchar_t buf[8] = {};
            wchar_t* end = at_delim.template get<keep_sep, no_zt>(buf, 4, L'\n');
            EXPECT_EQ(end, buf);
            EXPECT_TRUE(at_delim.rdstate() & ios_defs::strfailbit);
        }
    };

    expect_failed.operator()<istream>();
    expect_failed.operator()<iostream>();
}

// Stopping because the input ran out, with room still left, is the end of file --
// as distinct from stopping because the capacity was reached, which is not.
TEST(IstreamGetWchar, RunningOutOfInputSetsEndOfFileAndFillingTheBufferDoesNot)
{
    auto expect_distinguished = []<template <typename, typename> class T>()
    {
        {
            T    is(mem_device{std::wstring(L"012")});
            wchar_t buf[8] = {};
            wchar_t* end = is.template get<keep_sep, no_zt>(buf, 7);
            EXPECT_EQ(std::wstring(buf, end), L"012");
            EXPECT_TRUE(is.eof());
        }
        {
            T    is(mem_device{kDigits});
            wchar_t buf[8] = {};
            is.template get<keep_sep, no_zt>(buf, 4);
            EXPECT_FALSE(is.eof());
        }
    };

    expect_distinguished.operator()<istream>();
    expect_distinguished.operator()<iostream>();
}

// The capacity is a signed ptrdiff_t so that a negative value is rejected here
// rather than arriving as SIZE_MAX and filling the caller's buffer until the
// delimiter or the end of the input.
TEST(IstreamGetWchar, ACapacityThatIsNotPositiveIsRejected)
{
    auto expect_rejected = []<template <typename, typename> class T>()
    {
        for (const std::ptrdiff_t n : {std::ptrdiff_t{-1},
                                       std::numeric_limits<std::ptrdiff_t>::min(),
                                       std::ptrdiff_t{0}})
        {
            SCOPED_TRACE(n);
            T    is{mem_device{std::wstring(4096, L'x')}, locale<wchar_t>("C")};
            wchar_t buf[8];
            for (wchar_t& c : buf) c = L'#';

            wchar_t* ret = nullptr;
            EXPECT_NO_THROW((ret = is.template get<keep_sep, app_zt>(buf, n, L'\n')));
            EXPECT_EQ(ret, buf);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);

            // app_zt must not read a non-positive capacity as room for the
            // terminator either.
            for (const wchar_t c : buf)
                EXPECT_EQ(c, L'#');

            // And nothing was taken from the stream.
            is.clear();
            EXPECT_EQ(is.peek(), L'x');
        }

        // The two-argument overload widens '\n' and forwards, so it has to reject
        // the same capacities on the way through.
        {
            T    is{mem_device{std::wstring(4096, L'x')}, locale<wchar_t>("C")};
            wchar_t buf[8];
            for (wchar_t& c : buf) c = L'#';

            wchar_t* ret = nullptr;
            EXPECT_NO_THROW((ret = is.template get<cons_sep, app_zt>(buf, -1)));
            EXPECT_EQ(ret, buf);
            EXPECT_TRUE(is.rdstate() & ios_defs::strfailbit);
            for (const wchar_t c : buf)
                EXPECT_EQ(c, L'#');
        }
    };

    expect_rejected.operator()<istream>();
    expect_rejected.operator()<iostream>();
}

TEST(IstreamGetWchar, TheEndOfInputThrowsWhenEndOfFileIsMasked)
{
    auto expect_thrown = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
            is.exceptions(ios_defs::eofbit);
            EXPECT_THROW((void)is.get(), eof_error);
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
            is.exceptions(ios_defs::eofbit);
            wchar_t c = L'Z';
            EXPECT_THROW(is.get(c), eof_error);
            EXPECT_TRUE(is.eof());
        }
        // Unmasked, the same end is reported rather than thrown, and the
        // caller's variable is left as it was.
        {
            T is{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
            EXPECT_FALSE(is.get().has_value());
            EXPECT_TRUE(is.eof());
        }
        {
            T is{mem_device{std::wstring(L"")}, locale<wchar_t>("C")};
            wchar_t c = L'Z';
            is.get(c);
            EXPECT_EQ(c, L'Z');
            EXPECT_TRUE(is.eof());
        }
    };

    expect_thrown.operator()<istream>();
    expect_thrown.operator()<iostream>();
}

// The delimiter-terminated form over a real file, repeated until the input runs
// out: each line comes back whole and in order, which is what a caller reading a
// file a line at a time depends on.
TEST(IstreamGetWchar, RepeatedGetsWalkAFileLineByLine)
{
    constexpr int kLines = 200;

    std::string data;
    for (int i = 0; i < kLines; ++i)
        data += "line-" + std::to_string(i) + "\n";

    const std::string path = "test_istream_get_lines.txt";
    file_guard        guard(path, data);

    auto expect_walked = [&]<template <typename, typename> class T, typename TDevice>()
    {
        // file_guard writes bytes, so a wide stream over the file is a narrow
        // device with a converter on top.
        T is(TDevice{path}, code_cvt_creator<char, wchar_t>("C"));
        ASSERT_TRUE(static_cast<bool>(is));

        int read = 0;
        while (is)
        {
            wchar_t line[64] = {};
            wchar_t* end = is.template get<cons_sep, app_zt>(line, sizeof line, L'\n');
            if (!is) break;
            const std::string  want_narrow = "line-" + std::to_string(read);
            const std::wstring want(want_narrow.begin(), want_narrow.end());
            EXPECT_EQ(std::wstring(line), want);
            EXPECT_GT(end, line);
            ++read;
        }
        EXPECT_EQ(read, kLines);
    };

    expect_walked.operator()<istream, ifile_device<char>>();
    expect_walked.operator()<iostream, file_device<char>>();
}
