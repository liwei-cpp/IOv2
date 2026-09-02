// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ostreambuf_iterator: an output iterator that forwards to streambuf::sputc().
 *
 * Only assignment does anything. operator*, operator++ and operator++(int) all
 * return the iterator unchanged, which is what lets the usual `*it++ = c`
 * spelling work without writing three times -- and is the property an
 * implementation breaks by making one of them advance or emit.
 *
 * Because it holds only a reference to the buffer, other traffic on that buffer
 * in between is not the iterator's problem: it always writes wherever the
 * buffer's put position happens to be.
 */
#include <cvt/root_cvt.h>
#include <device/mem_device.h>
#include <io/streambuf_iterator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <type_traits>

using namespace IOv2;

TEST(OstreambufIterator, ItSatisfiesOutputIteratorForItsCharacterType)
{
    {
        using It = ostreambuf_iterator<streambuf<mem_device<char>, char>>;
        static_assert(std::output_iterator<It, char>);
        static_assert(std::is_same_v<It::value_type, char>);
    }
    {
        using It = ostreambuf_iterator<ostreambuf<mem_device<char>, char>>;
        static_assert(std::output_iterator<It, char>);
        static_assert(std::is_same_v<It::value_type, char>);
    }
    {
        using It = ostreambuf_iterator<streambuf<mem_device<char>, char32_t>>;
        static_assert(std::output_iterator<It, char32_t>);
        static_assert(std::is_same_v<It::value_type, char32_t>);
    }
    SUCCEED() << "the conformance checks in this case are static_asserts";
}

// Assignment is the only operation that emits anything.
TEST(OstreambufIterator, OnlyAssignmentWrites)
{
    auto helper = []<typename T>(const T& fresh)
    {
        T  buf = fresh;
        auto it = ostreambuf_iterator(buf);

        // Dereferencing and incrementing, in every spelling, before anything is
        // written: none of them may put a character.
        (void)*it;
        ++it;
        it++;
        (void)*it++;

        auto [dev, err] = buf.detach();
        EXPECT_TRUE(dev.str().empty());
    };

    streambuf sb{mem_device{""}};
    helper(sb);
    ostreambuf osb{mem_device{""}};
    helper(osb);
}

// The `*it++ = c` spelling writes exactly one character per assignment, which is
// what makes the iterator usable with the standard algorithms.
TEST(OstreambufIterator, TheUsualSpellingWritesOneCharacterPerAssignment)
{
    const std::string text = "one two three";

    auto helper = [&text]<typename T>(const T& fresh)
    {
        {
            T    buf = fresh;
            auto it  = ostreambuf_iterator(buf);
            for (char c : text)
                *it++ = c;

            auto [dev, err] = buf.detach();
            EXPECT_EQ(dev.str(), text);
        }
        {
            // The same thing through an algorithm that only knows the concept.
            T    buf = fresh;
            std::copy(text.begin(), text.end(), ostreambuf_iterator(buf));

            auto [dev, err] = buf.detach();
            EXPECT_EQ(dev.str(), text);
        }
    };

    streambuf sb{mem_device{""}};
    helper(sb);
    ostreambuf osb{mem_device{""}};
    helper(osb);
}

// Two iterators over the same buffer share its put position, because neither of
// them holds one.
TEST(OstreambufIterator, TwoIteratorsOverOneBufferWriteInSequence)
{
    streambuf buf{mem_device{""}};

    auto first  = ostreambuf_iterator(buf);
    auto second = ostreambuf_iterator(buf);

    *first++  = 'a';
    *second++ = 'b';
    *first++  = 'c';

    auto [dev, err] = buf.detach();
    EXPECT_EQ(dev.str(), "abc");
}
