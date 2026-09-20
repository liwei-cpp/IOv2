// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ichannel_iterator: an input iterator that reads through an iochannel.
 *
 * A default-constructed one is the end, and comparing against it is the only
 * way to ask "is there more". That question is the interesting one: on a stream
 * that has data but has not ended, the answer does not exist yet, so the
 * comparison has to wait for the writer rather than guess -- which the last
 * test checks over a real pipe.
 *
 * The iterator caches at most one look-ahead character, and that cache is what
 * the increment and putback cases are about: a postfix ++ hands back a copy
 * holding the character it read, so putting a character back through one has to
 * push the cached one first.
 */
#include <IOv2/common/defs.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/device/std_device.h>
#include <IOv2/io/iochannel.h>
#include <IOv2/io/iochannel_iterator.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <iterator>
#include <string>
#include <thread>
#include <type_traits>
#include <unistd.h>

using namespace IOv2;

TEST(IchannelIterator, ItSatisfiesInputIteratorForItsCharacterType)
{
    {
        using It = ichannel_iterator<iochannel<mem_device<char>, char>>;
        static_assert(std::input_iterator<It>);
        static_assert(std::is_same_v<It::value_type, char>);
    }
    {
        using It = ichannel_iterator<ichannel<mem_device<char>, char>>;
        static_assert(std::input_iterator<It>);
        static_assert(std::is_same_v<It::value_type, char>);
    }
    {
        using It = ichannel_iterator<iochannel<mem_device<char>, char32_t>>;
        static_assert(std::input_iterator<It>);
        static_assert(std::is_same_v<It::value_type, char32_t>);
    }
    SUCCEED() << "the conformance checks in this case are static_asserts";
}

namespace
{
    // Long enough that the buffer is refilled at least once, and with no repeated
    // prefix, so a position mistake cannot go unnoticed.
    const std::string kText = "a short line, then a longer one after it";
}

// A default-constructed iterator is the end. Reaching it is what ends a range,
// and two ends are equal to each other whatever they came from.
TEST(IchannelIterator, ADefaultConstructedIteratorIsTheEnd)
{
    auto helper = []<typename T>(const T& fresh)
    {
        T    chan(fresh);
        auto first = ichannel_iterator(chan);
        decltype(first) last;

        EXPECT_NE(first, last);

        const std::string all(first, last);
        EXPECT_EQ(all, kText);
        EXPECT_EQ(first, last);

        decltype(first) one_end;
        decltype(first) another_end;
        EXPECT_EQ(one_end, another_end);

        T    other(fresh);
        auto a = ichannel_iterator(other);
        auto b = ichannel_iterator(other);
        EXPECT_EQ(a, b);                // same buffer, so the same position
        EXPECT_NE(one_end, b);
    };

    iochannel chan(mem_device{kText});
    helper(chan);
    ichannel ichan(mem_device{kText});
    helper(ichan);
}

TEST(IchannelIterator, DereferencingAnEmptyBoundIteratorReportsEndOfInput)
{
    iochannel in(mem_device{""});
    bool      saw_eof = false;
    auto      iter = ichannel_iterator(in, &saw_eof);

    EXPECT_THROW((void)*iter, eof_error);
    EXPECT_TRUE(saw_eof);
    EXPECT_EQ(iter, std::default_sentinel);
}

// Post-increment yields the character it was on and then advances;
// pre-increment advances and yields the next one. The buffer's own position
// follows along, which is what lets bumpc pick up where the iterator stopped.
TEST(IchannelIterator, PostAndPreIncrementDifferInWhichCharacterTheyYield)
{
    auto helper = []<typename T>(const T& fresh)
    {
        {
            T    chan(fresh);
            auto it = ichannel_iterator(chan);
            for (std::size_t i = 0; i + 2 < kText.size(); ++i)
                EXPECT_EQ(*it++, kText[i]);

            EXPECT_EQ(chan.bumpc(), kText[kText.size() - 2]);
            EXPECT_EQ(chan.bumpc(), kText[kText.size() - 1]);
        }
        {
            T    chan(fresh);
            auto it = ichannel_iterator(chan);
            for (std::size_t i = 0; i + 2 < kText.size();)
                EXPECT_EQ(*++it, kText[++i]);

            EXPECT_EQ(chan.bumpc(), kText[kText.size() - 2]);
            EXPECT_EQ(chan.bumpc(), kText[kText.size() - 1]);
        }
    };

    iochannel chan(mem_device{kText});
    helper(chan);
    ichannel ichan(mem_device{kText});
    helper(ichan);
}

// Prefix and postfix increments may be interleaved in one pass.  Varying the
// choice as the input advances also crosses a refill without constructing three
// identical traversals whose agreement could hide a shared mistake.
TEST(IchannelIterator, MixedIncrementFormsPreserveOneContinuousSequence)
{
    iochannel chan(mem_device{kText});
    auto      it = ichannel_iterator(chan);
    decltype(it) end;
    std::string observed;

    for (std::size_t position = 0; it != end; ++position)
    {
        observed.push_back(*it);
        if (position % 3 == 1)
            it++;
        else
            ++it;
    }

    EXPECT_EQ(observed.size(), kText.size());
    EXPECT_EQ(observed, kText);
    EXPECT_EQ(it, end);
}

TEST(IchannelIterator, TheDefaultSentinelIsUsableAsTheEnd)
{
    using namespace IOv2;
    
    static_assert(std::sentinel_for<std::default_sentinel_t,
                                    ichannel_iterator<iochannel<mem_device<char>, char>>>);

    ichannel_iterator<iochannel<mem_device<char>, char>> i = std::default_sentinel;
    EXPECT_EQ(i, std::default_sentinel);
    EXPECT_EQ(std::default_sentinel, i);
}

TEST(IchannelIterator, ComparingAgainstTheSentinelReportsWhetherMoreIsComing)
{
    using namespace IOv2;

    {
        iochannel in(mem_device{"abc"});
        ichannel_iterator iter(in);
        EXPECT_NE(iter, std::default_sentinel);
        EXPECT_NE(std::default_sentinel, iter);

        (void)std::next(iter, 3);
        EXPECT_EQ(iter, std::default_sentinel);
        EXPECT_EQ(std::default_sentinel, iter);
    }

    {
        ichannel in(mem_device{"abc"});
        ichannel_iterator iter(in);
        EXPECT_NE(iter, std::default_sentinel);
        EXPECT_NE(std::default_sentinel, iter);

        (void)std::next(iter, 3);
        EXPECT_EQ(iter, std::default_sentinel);
        EXPECT_EQ(std::default_sentinel, iter);
    }
}

TEST(IchannelIterator, AChainedIncrementOnACachedCopyDoesNotLoseACharacter)
{
    using namespace IOv2;

    // Regression test: once operator++(int) returns a copy that caches an
    // already-consumed character (m_c), incrementing that copy again must
    // not pull yet another character from the shared iochannel. Before the
    // fix, operator++ / operator++(int) called bumpc() unconditionally,
    // silently discarding the cached character and consuming/skipping one
    // extra character from the stream.
    auto helper = []<typename TChannel>(TChannel& chan)
    {
        // prefix increment on a cached copy
        {
            ichannel_iterator it(chan);
            auto old1 = it++;
            EXPECT_EQ(*old1, 'a');

            ++old1;
            EXPECT_EQ(*it, 'b');
            EXPECT_EQ(*old1, 'b');
        }
    };

    auto helper_postfix = []<typename TChannel>(TChannel& chan)
    {
        // postfix increment on a cached copy
        {
            ichannel_iterator it(chan);
            auto old1 = it++;
            EXPECT_EQ(*old1, 'a');

            auto old2 = old1++;
            EXPECT_EQ(*old2, 'a');
            EXPECT_EQ(*old1, 'b');
            EXPECT_EQ(*it, 'b');
        }
    };

    {
        iochannel chan(mem_device{"abc"});
        helper(chan);
    }
    {
        ichannel chan(mem_device{"abc"});
        helper(chan);
    }
    {
        iochannel chan(mem_device{"abc"});
        helper_postfix(chan);
    }
    {
        ichannel chan(mem_device{"abc"});
        helper_postfix(chan);
    }
}

TEST(IchannelIterator, PutbackReplacesTheCharacterTheIteratorWillYieldNext)
{
    using namespace IOv2;

    {
        iochannel in(mem_device{"abc"});
        ichannel_iterator iter(in);
        ++iter;
        EXPECT_EQ(*iter, 'b');
        iter.putbackc('x');
        EXPECT_EQ(*iter++, 'x');
        EXPECT_EQ(*iter++, 'b');
    }

    {
        ichannel in(mem_device{"abc"});
        ichannel_iterator iter(in);
        ++iter;
        EXPECT_EQ(*iter, 'b');
        iter.putbackc('x');
        EXPECT_EQ(*iter++, 'x');
        EXPECT_EQ(*iter++, 'b');
    }
}

TEST(IchannelIterator, PutbackPushesTheCachedLookAheadBackFirst)
{
    using namespace IOv2;

    // putbackc on an iterator that still holds a cached look-ahead character
    // (m_c has a value): the cached character must be pushed back first, then
    // the supplied one. A postfix ++ leaves the returned iterator with m_c set.
    auto helper = []<typename TChannel>(TChannel& in)
    {
        ichannel_iterator it(in);
        auto old = it++;            // 'old' caches 'a'; device has consumed 'a'
        old.putbackc('x');          // push back cached 'a', then 'x'

        std::string got;
        decltype(old) eos;
        for (; old != eos; ++old) got.push_back(*old);
        EXPECT_EQ(got, "xabc");
    };

    {
        iochannel in(mem_device{"abc"});
        helper(in);
    }
    {
        ichannel in(mem_device{"abc"});
        helper(in);
    }

    // putbackc on an end/singular iterator (no bound iochannel) must throw.
    {
        ichannel_iterator<iochannel<mem_device<char>, char>> eos;
        bool threw = false;
        try
        {
            eos.putbackc('z');
        }
        catch (const cvt_error&)
        {
            threw = true;
        }
        EXPECT_TRUE(threw);
    }
}

TEST(IchannelIterator, EndDetectionWaitsForTheWriterRatherThanGuessing)
{
    using namespace IOv2;

    // A pipe carrying "ab" whose write end closes only after a delay. Comparing against the
    // end iterator must answer whether the stream has ended, and on a stream that has data
    // but has not ended, that answer only exists once the writer acts -- so the comparison
    // has to wait, exactly as std::istreambuf_iterator's does via sgetc().
    //
    // Guards against "answer end-detection without waiting": a comparison that reported
    // "not at end" merely because nothing had arrived yet would send the loop into
    // operator*, which throws eof_error when the end does arrive. The loop must instead
    // finish cleanly, having consumed exactly "ab".
    pipe_iguard g("ab");
    std::thread closer([&g]{ std::this_thread::sleep_for(std::chrono::seconds(1)); g.close_write(); });

    ichannel in{std_device<STDIN_FILENO>{}};
    std::string got;
    bool threw = false;
    try
    {
        for (ichannel_iterator it(in); it != decltype(it){}; ++it)
            got.push_back(*it);
    }
    catch (...) { threw = true; }
    closer.join();

    EXPECT_FALSE(threw);
    EXPECT_EQ(got, "ab");
}
