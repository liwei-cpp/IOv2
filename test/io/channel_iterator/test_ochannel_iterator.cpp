// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ochannel_iterator: an output iterator that forwards to iochannel::putc().
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
#include <IOv2/device/mem_device.h>
#include <IOv2/io/iochannel.h>
#include <IOv2/io/iochannel_iterator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <type_traits>

using namespace IOv2;

TEST(OchannelIterator, ItSatisfiesOutputIteratorForItsCharacterType)
{
    {
        using It = ochannel_iterator<iochannel<mem_device<char>, char>>;
        static_assert(std::output_iterator<It, char>);
        static_assert(std::is_same_v<It::value_type, char>);
    }
    {
        using It = ochannel_iterator<ochannel<mem_device<char>, char>>;
        static_assert(std::output_iterator<It, char>);
        static_assert(std::is_same_v<It::value_type, char>);
    }
    {
        using It = ochannel_iterator<iochannel<mem_device<char>, char32_t>>;
        static_assert(std::output_iterator<It, char32_t>);
        static_assert(std::is_same_v<It::value_type, char32_t>);
    }
    SUCCEED() << "the conformance checks in this case are static_asserts";
}

// Assignment is the only operation that emits anything.
TEST(OchannelIterator, OnlyAssignmentWrites)
{
    auto helper = []<typename T>(const T& fresh)
    {
        T  buf = fresh;
        auto it = ochannel_iterator(buf);

        // Dereferencing and incrementing, in every spelling, before anything is
        // written: none of them may put a character.
        (void)*it;
        ++it;
        it++;
        (void)*it++;

        auto [dev, err] = buf.detach();
        EXPECT_TRUE(dev.str().empty());
    };

    iochannel chan{mem_device{""}};
    helper(chan);
    ochannel ochan{mem_device{""}};
    helper(ochan);
}

// The `*it++ = c` spelling writes exactly one character per assignment, which is
// what makes the iterator usable with the standard algorithms.
TEST(OchannelIterator, TheUsualSpellingWritesOneCharacterPerAssignment)
{
    const std::string text = "one two three";

    auto helper = [&text]<typename T>(const T& fresh)
    {
        {
            T    buf = fresh;
            auto it  = ochannel_iterator(buf);
            for (char c : text)
                *it++ = c;

            auto [dev, err] = buf.detach();
            EXPECT_EQ(dev.str(), text);
        }
        {
            // The same thing through an algorithm that only knows the concept.
            T    buf = fresh;
            std::copy(text.begin(), text.end(), ochannel_iterator(buf));

            auto [dev, err] = buf.detach();
            EXPECT_EQ(dev.str(), text);
        }
    };

    iochannel chan{mem_device{""}};
    helper(chan);
    ochannel ochan{mem_device{""}};
    helper(ochan);
}

// Two iterators over the same buffer share its put position, because neither of
// them holds one.
TEST(OchannelIterator, TwoIteratorsOverOneBufferWriteInSequence)
{
    iochannel buf{mem_device{""}};

    auto first  = ochannel_iterator(buf);
    auto second = ochannel_iterator(buf);

    *first++  = 'a';
    *second++ = 'b';
    *first++  = 'c';

    auto [dev, err] = buf.detach();
    EXPECT_EQ(dev.str(), "abc");
}
