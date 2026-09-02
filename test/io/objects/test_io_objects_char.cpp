// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The narrow standard stream objects: cout, cerr, clog and cin.
 *
 * [iostream.objects] fixes what they are attached to and how they are tied,
 * and every property below follows from that. cerr is unit-buffered so a
 * diagnostic is not lost when the program dies; cerr and cin are both tied to
 * cout, which is what makes a prompt appear before the read that follows it.
 * That tie is the one users notice when it is missing, so it is checked by
 * observing the prompt rather than by comparing pointers alone.
 *
 * The objects also have to survive sync_with_stdio: it changes how they reach
 * the C streams, not which objects they are, so their addresses must not move.
 */
#include <io/objects/objects.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <string>

TEST(IoObjectsChar, EachStreamWritesToItsOwnDestination)
{
    {
        oguard<true> out;
        IOv2::cout << "to stdout" << IOv2::endl;
        EXPECT_EQ(out.contents(), "to stdout\n");
    }
    {
        oguard<false> err;
        IOv2::cerr << "to stderr" << IOv2::endl;
        EXPECT_EQ(err.contents(), "to stderr\n");
    }
    {
        oguard<false> err;
        IOv2::clog << "also stderr" << IOv2::endl;
        EXPECT_EQ(err.contents(), "also stderr\n");
    }
}

// The two destinations do not bleed into one another, in either order.
TEST(IoObjectsChar, StdoutAndStderrStaySeparate)
{
    oguard<true>  out;
    oguard<false> err;

    IOv2::cout << "first ";
    IOv2::cout.flush();
    IOv2::cerr << "second";
    IOv2::cerr.flush();
    IOv2::cout << "third" << IOv2::endl;
    IOv2::cout.flush();

    EXPECT_EQ(out.contents(), "first third\n");
    EXPECT_EQ(err.contents(), "second");
}

TEST(IoObjectsChar, CerrIsUnitBufferedAndBothInputsAreTiedToCout)
{
    EXPECT_TRUE(IOv2::cerr.flags() & IOv2::ios_defs::unitbuf);
    EXPECT_EQ(IOv2::cerr.tie(), &IOv2::cout);
    EXPECT_EQ(IOv2::cin.tie(), &IOv2::cout);

    // The flags a fresh stream starts with.
    EXPECT_TRUE(IOv2::cerr.flags() & IOv2::ios_defs::dec);
    EXPECT_TRUE(IOv2::cerr.flags() & IOv2::ios_defs::skipws);
}

// What the tie is for: an unterminated prompt is still sitting in cout's buffer
// when the read starts, and the read is what pushes it out.
TEST(IoObjectsChar, ReadingFromCinFlushesThePromptOnCout)
{
    IOv2::cout.reset();
    IOv2::cin.reset();
    IOv2::cout.sync_with_stdio(false);

    oguard<true> out;
    iguard       in("Ada");

    IOv2::cout << "ready" << IOv2::endl;
    IOv2::cout << "name? ";                 // no newline, no flush
    EXPECT_EQ(out.contents(), "ready\n");   // so it has not gone out yet

    std::string answer;
    IOv2::cin >> answer;
    EXPECT_EQ(answer, "Ada");
    EXPECT_EQ(out.contents(), "ready\nname? ");
}

// Each block re-points stdin, so cin is reset with it: these are global objects
// and a previous test's end-of-input would otherwise still be on them.
TEST(IoObjectsChar, CinReadsAndPutsBack)
{
    {
        iguard g("alpha beta");
        IOv2::cin.reset();
        char   first = 0;
        char   again = 1;

        IOv2::cin.get(first);
        IOv2::cin.putback(first);
        IOv2::cin.get(again);

        EXPECT_TRUE(IOv2::cin.good());
        EXPECT_EQ(first, again);
    }
    {
        iguard g("alpha beta");
        IOv2::cin.reset();
        char   buf[2];
        // Hoisted: the template argument list would look like a second macro
        // argument to the preprocessor.
        char* end = IOv2::cin.get<IOv2::keep_sep, IOv2::no_zt>(buf, 2);
        EXPECT_EQ(end - buf, 2);

        IOv2::cin.putback(buf[1]);
        EXPECT_EQ(IOv2::cin.get(), buf[1]);
    }
    {
        iguard g("\n");
        IOv2::cin.reset();
        EXPECT_TRUE(static_cast<bool>(IOv2::cin.ignore(1)));
    }
}

// sync_with_stdio changes how the objects reach the C streams, not which
// objects they are.
TEST(IoObjectsChar, SyncWithStdioDoesNotReplaceTheObjects)
{
    const void* before[] = {&IOv2::cout, &IOv2::cin, &IOv2::cerr, &IOv2::clog};

    IOv2::sync_with_stdio(false);

    const void* after[] = {&IOv2::cout, &IOv2::cin, &IOv2::cerr, &IOv2::clog};

    for (std::size_t i = 0; i < 4; ++i)
        EXPECT_EQ(before[i], after[i]) << "object " << i;
}
