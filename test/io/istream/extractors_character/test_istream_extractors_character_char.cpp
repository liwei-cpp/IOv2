/**
 * Formatted extraction of characters and character arrays from an
 * istream<char>: operator>>(CharT&) and operator>>(CharT (&)[N]).
 *
 * Both skip leading whitespace and both are formatted, so both fail when they
 * find nothing. What separates them is the bound. The single-character form
 * takes exactly one. The array form takes a whole token, which means it has to
 * stop somewhere before the caller's buffer ends -- and there are two limits
 * that can do it: the array bound N, which the operator deduces from the
 * parameter type and therefore always knows, and the field width, which the
 * caller sets and which is spent by the extraction that honours it. The
 * smaller of the two wins, and one place is always kept for the terminator.
 *
 * A char stream also accepts signed char and unsigned char, and reads them as
 * characters rather than as small numbers -- '6' comes back as '6' and not
 * as 54, which is the one place the character extractors could plausibly be
 * confused with the arithmetic ones.
 */
#include <device/file_device.h>
#include <device/mem_device.h>
#include <io/io_manip.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/traits/nullptr.h>

#include <gtest/gtest.h>

#include <support/file_guard.h>
#include <support/injectable_device.h>
#include <support/io_traits_probe.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

using namespace IOv2;

TEST(IstreamExtractCharacterChar, ASingleCharacterExtractionSkipsWhitespaceAndTakesOne)
{
    auto expect_one = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::string("  ab c")});

        char c = '#';
        is >> c;
        EXPECT_EQ(c, 'a');
        EXPECT_EQ(is.peek(), 'b');     // and no more than one

        is >> c;
        EXPECT_EQ(c, 'b');

        is >> c;                       // the space before 'c' is skipped
        EXPECT_EQ(c, 'c');
        EXPECT_TRUE(is.good());
    };

    expect_one.operator()<istream>();
    expect_one.operator()<iostream>();
}

// Finding nothing is a failure, and a failed extraction does not write to the
// caller's variable -- so a caller who forgot to check still sees whatever they
// had rather than something invented.
TEST(IstreamExtractCharacterChar, ACharacterExtractionThatFindsNothingLeavesTheVariableAlone)
{
    auto expect_untouched = []<template <typename, typename> class T>()
    {
        {
            T is{mem_device{std::string("")}};
            char c = '#';
            is >> c;
            EXPECT_EQ(c, '#');
            EXPECT_TRUE(is.str_fail());
        }
        {
            // Whitespace only: the skip runs off the end, so there is still
            // nothing to extract.
            T is{mem_device{std::string("   ")}};
            char c = '#';
            is >> c;
            EXPECT_EQ(c, '#');
            EXPECT_TRUE(is.str_fail());
            EXPECT_TRUE(is.eof());
        }
    };

    expect_untouched.operator()<istream>();
    expect_untouched.operator()<iostream>();
}

TEST(IstreamExtractCharacterChar, AnArrayExtractionTakesAWholeTokenAndTerminatesIt)
{
    auto expect_token = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::string("  alpha beta ")});

        char buf[16];
        for (char& c : buf) c = '#';

        is >> buf;
        EXPECT_STREQ(buf, "alpha");
        EXPECT_EQ(buf[6], '#');             // nothing written past the terminator
        EXPECT_EQ(is.peek(), ' ');          // the whitespace that stopped it is left
        EXPECT_TRUE(is.good());

        is >> buf;
        EXPECT_STREQ(buf, "beta");
    };

    expect_token.operator()<istream>();
    expect_token.operator()<iostream>();
}

// The array bound is part of the parameter type, so it is always known and
// always applies -- there is no way to ask for more than the destination holds.
// One place goes to the terminator, so an array of N takes N-1 characters.
TEST(IstreamExtractCharacterChar, TheArrayBoundLimitsTheExtractionOnItsOwn)
{
    auto expect_bounded = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::string("abcdefghi jk l")});

        char three[3];
        is >> three;                        // two characters and the terminator
        EXPECT_STREQ(three, "ab");
        EXPECT_TRUE(is.good());

        // The rest of the token is still there, so a bounded read truncates
        // rather than discards.
        char four[4];
        is >> four;
        EXPECT_STREQ(four, "cde");

        // A token shorter than the bound simply ends early.
        char big[16];
        is >> big;
        EXPECT_STREQ(big, "fghi");

        three[2] = '#';
        is >> three;
        EXPECT_STREQ(three, "jk");
        EXPECT_TRUE(is.good());
    };

    expect_bounded.operator()<istream>();
    expect_bounded.operator()<iostream>();
}

// The field width is the caller's own limit. It applies to both destinations,
// and the extraction that honours it spends it -- otherwise it would silently
// truncate the next extraction too.
TEST(IstreamExtractCharacterChar, TheFieldWidthLimitsTheExtractionAndIsSpentByIt)
{
    auto expect_width = []<template <typename, typename> class T>()
    {
        {
            T is(mem_device{std::string("abcdefghij")});

            char buf[16];
            is >> setw(4) >> buf;           // three characters and the terminator
            EXPECT_STREQ(buf, "abc");
            EXPECT_EQ(is.width(), 0);

            // Spent, so the next one is bounded only by the array.
            is >> buf;
            EXPECT_STREQ(buf, "defghij");
        }
        {
            // Whichever of the two limits is smaller decides.
            T is(mem_device{std::string(1000, 'a')});

            char buf[8];
            is >> setw(64) >> buf;          // the array is smaller
            EXPECT_EQ(std::strlen(buf), 7u);

            is.clear();
            char big[64];
            is >> setw(8) >> big;           // the width is smaller
            EXPECT_EQ(std::strlen(big), 7u);
        }
        {
            // A string destination needs no terminator, so the width buys a
            // character more there than it does into an array of the same size.
            T is(mem_device{std::string("abcdefghij")});

            std::string s;
            is >> setw(4) >> s;
            EXPECT_EQ(s, "abcd");
            EXPECT_EQ(is.width(), 0);
        }
    };

    expect_width.operator()<istream>();
    expect_width.operator()<iostream>();
}

// A limit of one leaves no room for a character, only for the terminator, so
// the extraction has nothing it is allowed to take and says so.
TEST(IstreamExtractCharacterChar, ALimitOfOneLeavesRoomOnlyForTheTerminator)
{
    auto expect_empty = []<template <typename, typename> class T>()
    {
        T is(mem_device{std::string("abcdef")});

        char buf[16];
        buf[0] = '#';
        is >> setw(1) >> buf;
        EXPECT_EQ(buf[0], '\0');
        EXPECT_TRUE(is.str_fail());

        // Nothing was consumed on the way to the failure.
        is.clear();
        EXPECT_EQ(is.peek(), 'a');
    };

    expect_empty.operator()<istream>();
    expect_empty.operator()<iostream>();
}

// The last token of an input that does not end in whitespace stops because the
// input ran out. That sets eofbit but is not a failure: the token is complete.
TEST(IstreamExtractCharacterChar, ATokenEndedByTheInputIsStillAWholeToken)
{
    auto expect_last = []<template <typename, typename> class T>()
    {
        {
            T is(mem_device{std::string("   measure")});
            char buf[16];
            is >> buf;
            EXPECT_STREQ(buf, "measure");
            EXPECT_EQ(is.rdstate(), ios_defs::eofbit);
            EXPECT_FALSE(is.str_fail());
        }
        {
            // Stopping on a limit instead leaves the stream at the next
            // character, so the end has not been seen.
            T is(mem_device{std::string("   measure")});
            char buf[16];
            is >> setw(8) >> buf;
            EXPECT_STREQ(buf, "measure");
            EXPECT_EQ(is.rdstate(), ios_defs::goodbit);
        }
    };

    expect_last.operator()<istream>();
    expect_last.operator()<iostream>();
}

// Availability is probed through `extractable` (support/io_traits_probe.h) rather than
// `requires { in >> v; }`, so a failure points at io_traits itself and not at the
// value-category and parse-context handling operator>> layers on top of it.
TEST(IstreamExtractCharacterChar, SignedAndUnsignedCharAreReadAsCharactersNotNumbers)
{
    static_assert( extractable<char, char> );
    static_assert( extractable<char, signed char> );
    static_assert( extractable<char, unsigned char> );
    static_assert( !extractable<char, wchar_t> );
    static_assert( !extractable<char, char8_t> );
    static_assert( !extractable<char, char16_t> );
    static_assert( !extractable<char, char32_t> );
    static_assert( !extractable<char, std::nullptr_t> );

    auto expect_characters = []<template <typename, typename> class T>()
    {
        // A char stream reads signed char / unsigned char as characters, not as numbers.
        T is(mem_device{std::string("65")});

        signed char   sc = 0;
        unsigned char uc = 0;
        is >> sc >> uc;
        EXPECT_TRUE(is.good());
        EXPECT_EQ(sc, static_cast<signed char>('6'));
        EXPECT_EQ(uc, static_cast<unsigned char>('5'));

        // The array forms of both are character arrays for the same reason.
        T arr(mem_device{std::string("cde fgh")});
        signed char   sbuf[4];
        unsigned char ubuf[4];
        arr >> sbuf >> ubuf;
        EXPECT_EQ(sbuf[0], 'c');
        EXPECT_EQ(sbuf[3], '\0');
        EXPECT_EQ(ubuf[0], 'f');
        EXPECT_EQ(ubuf[3], '\0');
    };

    expect_characters.operator()<istream>();
    expect_characters.operator()<iostream>();
}

// width() bounds the read, so it must be able to express any length a
// destination can hold -- not just the small values a field width takes.
TEST(IstreamExtractCharacterChar, TheWidthIsNotLimitedToWhatAFieldWidthCanExpress)
{
    auto expect_large = []<template <typename, typename> class T>()
    {
        const std::string src(1000, 'a');

        {
            T is(mem_device{src});
            std::string s;
            is >> setw(600) >> s;
            EXPECT_TRUE(is.good());
            EXPECT_EQ(s.size(), 600u);
            EXPECT_EQ(is.width(), 0);
        }
        {
            // The array bound N still wins: min(width, N) - 1 characters are read.
            T is(mem_device{src});
            char buf[300];
            is >> setw(600) >> buf;
            EXPECT_TRUE(is.good());
            EXPECT_EQ(std::strlen(buf), 299u);
        }
        {
            T is(mem_device{src});
            char buf[300];
            is >> setw(280) >> buf;
            EXPECT_TRUE(is.good());
            EXPECT_EQ(std::strlen(buf), 279u);
        }
    };

    expect_large.operator()<istream>();
    expect_large.operator()<iostream>();
}

// width() is one-shot state visible across operations, so a string extraction that
// leaves by an exception must still have consumed it -- otherwise the stale width
// silently truncates the *next* extraction, which reports good(). The failure has to
// land inside the read loop (a device error on a buffer refill), because a throw
// after the loop was never the leaking path.
TEST(IstreamExtractCharacterChar, TheWidthIsSpentEvenWhenTheExtractionThrows)
{
    constexpr std::size_t payload = 20000;   // well past the 2048-char buffer
    constexpr std::size_t leak_w  = 3000;

    {
        injectable_device<char> dev{std::string(payload, 'A')};
        auto                    st = dev.shared_state();
        istream                 in(std::move(dev));

        // Prime the buffer and leave most of it unread.
        std::string s1;
        in >> setw(10) >> s1;
        ASSERT_TRUE(in.good());
        EXPECT_EQ(s1.size(), 10u);

        // The next refill fails, so this extraction throws part-way through the loop.
        st->fail_dget = true;
        std::string s2;
        in >> setw(leak_w) >> s2;
        EXPECT_FALSE(in.good());
        EXPECT_EQ(in.width(), 0);

        // With the width consumed, an extraction that asks for no width is not capped.
        st->fail_dget = false;
        in.clear();
        std::string s3;
        in >> s3;
        EXPECT_GT(s3.size(), leak_w);
        EXPECT_EQ(s3.find_first_not_of('A'), std::string::npos);
    }

    // The character-array path consumes the width up front and always has; keeping the
    // two side by side is what stops them drifting apart again.
    {
        injectable_device<char> dev{std::string(payload, 'A')};
        auto                    st = dev.shared_state();
        istream                 in(std::move(dev));

        char buf[16];
        in >> setw(sizeof(buf)) >> buf;
        ASSERT_TRUE(in.good());

        st->fail_dget = true;
        std::string s2;
        in >> setw(leak_w) >> s2;
        EXPECT_FALSE(in.good());
        EXPECT_EQ(in.width(), 0);
    }
}

// Tokens far longer than the stream's internal buffer have to be reassembled
// across several device reads, and the boundary between two of those reads must
// not look like the end of a token. The lengths straddle the buffer size in
// both directions so that a boundary lands inside a token, between two, and on
// the whitespace itself.
TEST(IstreamExtractCharacterChar, TokensSpanningSeveralDeviceReadsComeBackWhole)
{
    std::vector<std::string> tokens;
    for (const std::size_t n : {1u, 2047u, 2048u, 2049u, 5000u, 3u, 20000u})
        tokens.push_back(std::string(n, static_cast<char>('a' + tokens.size())));

    std::string data;
    for (const std::string& t : tokens)
        data += t + " ";

    const std::string path = "test_istream_extract_long_tokens.txt";
    file_guard        guard(path, data);

    auto expect_whole = [&]<template <typename, typename> class T, typename TDevice>()
    {
        T is(TDevice{path});
        ASSERT_TRUE(static_cast<bool>(is));

        std::size_t n = 0;
        std::string tok;
        while (is >> tok)
        {
            ASSERT_LT(n, tokens.size());
            EXPECT_EQ(tok, tokens[n]);
            ++n;
        }

        EXPECT_EQ(n, tokens.size());
        EXPECT_TRUE(is.eof());
    };

    expect_whole.operator()<istream, ifile_device<char>>();
    expect_whole.operator()<iostream, file_device<char>>();
}
