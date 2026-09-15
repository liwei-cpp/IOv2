// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
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
#include <IOv2/io/io_base.h>
#include <IOv2/io/objects/in_impl.h>
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/objects/out_impl.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include <fcntl.h>
#include <unistd.h>

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

// reset() offers the bytes it is about to drop to the old device once. When
// that write cannot land, the loss is reported as devfailbit rather than
// thrown, and the stream is still fully usable after clear() -- the device has
// been replaced and the converter is initialized, not left half-way.
TEST(IoObjectsChar, ResetReportsAFailedFlushOfTheDroppedBytesAndStaysUsable)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<true> out;
    const bool   sync = IOv2::cout.sync_with_stdio(false);

    IOv2::cout << "DROPPED";                      // still in this stream's buffer

    const int saved = ::dup(STDOUT_FILENO);
    ::dup2(full, STDOUT_FILENO);                  // the pending bytes cannot land
    EXPECT_NO_THROW(IOv2::cout.reset());
    ::dup2(saved, STDOUT_FILENO);
    ::close(saved);
    ::close(full);

    EXPECT_EQ(IOv2::cout.rdstate(), IOv2::ios_defs::devfailbit);

    IOv2::cout.clear();
    IOv2::cout << "AFTER" << IOv2::flush;

    EXPECT_TRUE(IOv2::cout.good());
    EXPECT_EQ(out.contents(), "AFTER");

    IOv2::cout.sync_with_stdio(sync);
}

// In synchronized mode an insertion can leave this stream's bytes in stdout's
// FILE buffer: fwrite succeeds, and only a later fflush sees the broken fd.
// reset() must report that later failure too, before discarding the old device.
TEST(IoObjectsChar, ResetReportsAFlushFailureFromAFullBufferedStdout)
{
    const int full = ::open("/dev/full", O_WRONLY);
    if (full == -1) GTEST_SKIP() << "no /dev/full here";

    oguard<true> out;
    IOv2::cout.reset();
    const bool sync = IOv2::cout.sync_with_stdio(true);

    {
        stdout_full_buffer buffered;                // restores unbuffered stdout on any exit

        IOv2::cout << "DROPPED";                    // moved into stdout's FILE buffer
        EXPECT_TRUE(out.contents().empty());

        const int saved = ::dup(STDOUT_FILENO);
        ASSERT_NE(saved, -1);
        EXPECT_EQ(::dup2(full, STDOUT_FILENO), STDOUT_FILENO);
        EXPECT_NO_THROW(IOv2::cout.reset());
        EXPECT_EQ(::dup2(saved, STDOUT_FILENO), STDOUT_FILENO);
        ::close(saved);
        ::close(full);
    }

    EXPECT_EQ(IOv2::cout.rdstate(), IOv2::ios_defs::devfailbit);

    IOv2::cout.clear();
    IOv2::cout << "AFTER" << IOv2::flush;

    EXPECT_TRUE(IOv2::cout.good());
    EXPECT_NE(out.contents().find("AFTER"), std::string::npos);

    IOv2::cout.sync_with_stdio(sync);
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
