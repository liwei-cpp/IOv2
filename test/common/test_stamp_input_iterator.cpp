// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/stamp_input_iterator.h>
#include <device/mem_device.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>

#include <gtest/gtest.h>

#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace IOv2;

namespace
{
    struct TestPoint
    {
        int x;
        int y;
    };
}

TEST(StampInputIterator, Basic)
{
    std::vector<TestPoint> points = {{1, 2}, {3, 4}};
    IOv2::stamp_input_iterator it(points.begin());

    EXPECT_EQ(it->x, 1);
    EXPECT_EQ(it->y, 2);

    ++it;
    EXPECT_EQ(it->x, 3);
    EXPECT_EQ(it->y, 4);

    it.rollback();
    EXPECT_EQ(it->x, 1);
}

TEST(StampInputIterator, IstreambufArrowOperator)
{
    IOv2::mem_device dev("abc");
    IOv2::istreambuf buf(dev);
    IOv2::istreambuf_iterator is_it(buf);
    IOv2::stamp_input_iterator s_it(is_it);

    char c = *s_it;
    EXPECT_EQ(c, 'a');

    // Testing operator-> if possible (istreambuf_iterator usually points to char)
    // Here we just ensure it compiles and behaves correctly
    EXPECT_EQ(*(s_it.operator->()), 'a');
}

TEST(StampInputIterator, RawPointer)
{
    TestPoint points[] = {{10, 20}, {30, 40}};
    IOv2::stamp_input_iterator it(&points[0]);

    EXPECT_EQ(it->x, 10);
    EXPECT_EQ(it->y, 20);

    ++it;
    EXPECT_EQ(it->x, 30);
    EXPECT_EQ(it->y, 40);
}

TEST(StampInputIterator, MoveConstruction)
{
    std::vector<int> vec = {1, 2, 3, 4};
    IOv2::stamp_input_iterator it1(vec.begin());
    ++it1;
    ++it1; // m_pos = 2

    auto it2 = std::move(it1);
    // it1.m_pos should be 0 now
    it1.rollback(); // Should be no-op

    it2.rollback();
    EXPECT_EQ(*it2, 1);
}

TEST(StampInputIterator, MoveAssignment)
{
    std::vector<int> vec = {1, 2, 3, 4};
    IOv2::stamp_input_iterator it1(vec.begin());
    ++it1;
    ++it1; // m_pos = 2

    auto it3 = IOv2::stamp_input_iterator(vec.begin());
    it3 = std::move(it1);

    it1.rollback(); // Should be no-op
    it3.rollback();
    EXPECT_EQ(*it3, 1);
}

TEST(StampInputIterator, SelfMoveAssignment)
{
    std::vector<int> vec = {1, 2, 3, 4};
    IOv2::stamp_input_iterator it1(vec.begin());
    ++it1; // m_pos = 1

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    it1 = std::move(it1);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    EXPECT_EQ(*it1, 2);
    it1.rollback();
    EXPECT_EQ(*it1, 1);
}

TEST(StampInputIterator, IstreambufMoveConstruction)
{
    IOv2::mem_device dev("abc");
    IOv2::istreambuf buf(dev);
    IOv2::istreambuf_iterator is_it(buf);
    IOv2::stamp_input_iterator s_it1(is_it);

    ++s_it1;
    ++s_it1; // m_rec has 2 elements

    auto s_it2 = std::move(s_it1);
    s_it1.rollback(); // Should be no-op as m_rec is empty

    s_it2.rollback();
    EXPECT_EQ(*s_it2, 'a');
}

TEST(StampInputIterator, ArithmeticAndComparisons)
{
    std::vector<int> vec = {1, 2, 3, 4, 5};
    IOv2::stamp_input_iterator it1(vec.begin());
    IOv2::stamp_input_iterator it2(vec.begin());

    EXPECT_EQ(it1, it2);
    EXPECT_FALSE(it1 != it2);

    (void)it1++;
    EXPECT_EQ(*it1, 2);
    EXPECT_NE(it1, it2);
    EXPECT_GT(it1, it2);
    EXPECT_GE(it1, it2);
    EXPECT_LT(it2, it1);
    EXPECT_LE(it2, it1);

    it1 += 2;
    EXPECT_EQ(*it1, 4);

    it1 -= 1;
    EXPECT_EQ(*it1, 3);

    auto it3 = it1 + 1;
    EXPECT_EQ(*it3, 4);

    auto it4 = 1 + it1;
    EXPECT_EQ(*it4, 4);

    auto it5 = it1 - 1;
    EXPECT_EQ(*it5, 2);

    EXPECT_EQ(it1 - it2, 2);

    EXPECT_EQ(it1[1], 4);

    (void)it1--;
    EXPECT_EQ(*it1, 2);

    it1.rollback();
    EXPECT_EQ(*it1, 1);

    EXPECT_EQ(it1.internal(), vec.begin());
}

TEST(StampInputIterator, IstreambufSteppingBack)
{
    IOv2::mem_device dev("abc");
    IOv2::istreambuf buf(dev);
    IOv2::istreambuf_iterator is_it(buf);
    IOv2::stamp_input_iterator s_it(is_it);

    (void)s_it++;
    (void)s_it++;
    EXPECT_EQ(*s_it, 'c');

    --s_it;
    EXPECT_EQ(*s_it, 'b');

    (void)s_it--;
    EXPECT_EQ(*s_it, 'a');

    // Already at the oldest recorded position, so there is nothing to step to.
    EXPECT_THROW(--s_it, std::runtime_error);

    (void)s_it++;
    s_it.rollback();
    EXPECT_EQ(*s_it, 'a');
}

TEST(StampInputIterator, Constructors)
{
    IOv2::stamp_input_iterator<std::vector<int>::iterator> it_default;
    // it_default is default initialized, internal iterator is value initialized.

    IOv2::stamp_input_iterator<std::vector<int>::iterator> it_sentinel{std::default_sentinel};
    // Similar to default

    std::vector<int> vec = {1};
    IOv2::stamp_input_iterator it(vec.begin());
    IOv2::stamp_input_iterator it_copy(it);
    EXPECT_EQ(it_copy, it);

    IOv2::stamp_input_iterator it_assign = it;
    EXPECT_EQ(it_assign, it);

    // Test traits
    EXPECT_TRUE(is_stamp_input_iterator_v<decltype(it)>);
    EXPECT_FALSE(is_stamp_input_iterator_v<int>);
}

TEST(StampInputIterator, IstreambufRollbackAndInternal)
{
    IOv2::mem_device dev("abc");
    IOv2::istreambuf buf(dev);
    IOv2::istreambuf_iterator is_it(buf);
    IOv2::stamp_input_iterator s_it(is_it);

    // rollback when empty
    s_it.rollback();
    EXPECT_EQ(*s_it, 'a');

    EXPECT_EQ(s_it.internal(), is_it);

    IOv2::stamp_input_iterator s_it2(is_it);
    s_it2 = std::move(s_it);
    EXPECT_EQ(*s_it2, 'a');
}

TEST(StampInputIterator, CategoryConsistency)
{
    IOv2::mem_device dev("abc");
    IOv2::istreambuf buf(dev);
    IOv2::istreambuf_iterator raw(buf);
    IOv2::stamp_input_iterator s_it(raw);

    using raw_t   = decltype(raw);
    using stamp_t = decltype(s_it);

    // Both tags are stated, and both report the wrapped iterator's category. Leaving them
    // unstated is not neutral: iterator_traits would synthesize input_iterator_tag while
    // ITER_CONCEPT, finding no member, falls back to random_access_iterator_tag -- the
    // C++20 concepts would then call this type bidirectional while the C++17 traits call
    // it input, and an algorithm dispatching on the latter takes the wrong branch.
    static_assert(std::is_same_v<std::iterator_traits<stamp_t>::iterator_category,
                                 std::input_iterator_tag>);
    static_assert(std::input_iterator<stamp_t>);
    static_assert(!std::forward_iterator<stamp_t>);
    static_assert(!std::bidirectional_iterator<stamp_t>);

    // It steps back all the same, which is what steppable_back names.
    static_assert(IOv2::steppable_back<stamp_t>);
    static_assert(!IOv2::steppable_back<raw_t>);              // istreambuf_iterator has no --
    static_assert(IOv2::steppable_back<std::string::iterator>); // bidirectional is subsumed

    ++s_it;
    ++s_it;
    EXPECT_EQ(*s_it, 'c');
    --s_it;
    EXPECT_EQ(*s_it, 'b');
    --s_it;
    EXPECT_EQ(*s_it, 'a');
}
