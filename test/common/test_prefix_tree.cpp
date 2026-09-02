// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <common/prefix_tree.h>
#include <common/stamp_input_iterator.h>
#include <device/mem_device.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace IOv2;

using ::testing::HasSubstr;

TEST(PrefixTree, Basic)
{
    prefix_tree<char, int> tree;
    tree.add("hello", 1);
    tree.add("world", 2);
    tree.add("he", 3);

    decltype(tree)::match_out_type out{};

    std::string s1 = "hello";
    auto it1 = tree.max_match(s1.begin(), s1.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 1);
    EXPECT_EQ(it1, s1.end());

    std::string s2 = "he";
    it1 = tree.max_match(s2.begin(), s2.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 3);
    EXPECT_EQ(it1, s2.end());

    std::string s3 = "hell";
    it1 = tree.max_match(s3.begin(), s3.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 3) << "\"he\" is the longest stored key that prefixes \"hell\"";
    EXPECT_EQ(it1, s3.begin() + 2);

    std::string s4 = "world";
    it1 = tree.max_match(s4.begin(), s4.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 2);
    EXPECT_EQ(it1, s4.end());

    // Runs last on purpose: out still holds 2 from the call above, so this also
    // pins that a failed match clears the previous result rather than leaving it.
    std::string s5 = "not_found";
    it1 = tree.max_match(s5.begin(), s5.end(), out);
    EXPECT_FALSE(out.has_value());
    EXPECT_EQ(it1, s5.begin());
}

TEST(PrefixTree, DuplicateAdd)
{
    prefix_tree<char, int> tree;
    tree.add("test", 10);

    // Re-adding a key is only an error when it would change the stored value.
    EXPECT_NO_THROW(tree.add("test", 10));
    EXPECT_THROW(tree.add("test", 20), std::runtime_error);

    std::string s = "iter";
    tree.add(s.begin(), s.end(), 100);
    EXPECT_NO_THROW(tree.add(s.begin(), s.end(), 100));
    EXPECT_THROW(tree.add(s.begin(), s.end(), 200), std::runtime_error);

    // Same rule for a value type stored out of line.
    prefix_tree<char, std::string> tree_large;
    tree_large.add("test", "val1");
    EXPECT_THROW(tree_large.add("test", "val2"), std::runtime_error);
}

TEST(PrefixTree, StringViewKey)
{
    prefix_tree<char, int> tree;
    std::string_view sv = "view";
    tree.add(sv, 5);

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(sv.begin(), sv.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 5);
    EXPECT_EQ(it, sv.end());
}

TEST(PrefixTree, VectorConstructor)
{
    std::vector<const char*> strs = {"apple", "banana", "cherry"};
    prefix_tree<char, int> tree(strs);

    // The value of each key is its index in the vector.
    decltype(tree)::match_out_type out{};
    std::string s = "banana";
    (void)tree.max_match(s.begin(), s.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 1);

    // The alias is not cosmetic: a comma inside template arguments would be read
    // as a second macro argument.
    using int_tree = prefix_tree<char, int>;
    std::vector<const char*> empty_strs;
    EXPECT_NO_THROW((void)int_tree(empty_strs));
}

TEST(PrefixTree, RootValue)
{
    prefix_tree<char, int> tree;
    tree.add("", 100);
    tree.add("a", 1);

    // "b" matches no child, so the walk stops at the root and takes its value.
    decltype(tree)::match_out_type out{};
    std::string s = "b";
    auto it = tree.max_match(s.begin(), s.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 100);
    EXPECT_EQ(it, s.begin());
}

TEST(PrefixTree, GreedyMatch)
{
    prefix_tree<char, int> tree;
    tree.add("abc", 1);
    tree.add("abcd", 2);

    // Both keys prefix the input; the longer one wins.
    decltype(tree)::match_out_type out{};
    std::string s = "abcde";
    auto it = tree.max_match(s.begin(), s.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 2);
    EXPECT_EQ(it, s.begin() + 4);
}

namespace
{
    // The three-key tree the two istreambuf cases below share: a root value, plus
    // "ab" nested inside "abc" so a walk can overshoot and have to come back.
    prefix_tree<char, int> istreambuf_tree()
    {
        prefix_tree<char, int> tree;
        tree.add("", 100);
        tree.add("abc", 1);
        tree.add("ab", 2);
        return tree;
    }
}

TEST(PrefixTree, StreambufPartialMatch)
{
    auto tree = istreambuf_tree();

    mem_device dev("abxe");
    istreambuf sb(dev);
    istreambuf_iterator beg(sb);
    decltype(beg) end;

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(beg, end, out);

    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 2) << "\"abc\" was entered but only \"ab\" is present";
    EXPECT_EQ(*it, 'x');
}

TEST(PrefixTree, StreambufBacktrackToRoot)
{
    auto tree = istreambuf_tree();

    mem_device dev("axe");
    istreambuf sb(dev);
    istreambuf_iterator beg(sb);
    decltype(beg) end;

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(beg, end, out);

    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 100);
    EXPECT_EQ(*it, 'a');
}

// A stamp_input_iterator wrapping an istreambuf_iterator is what timeio's era path hands
// to max_match. It is single-pass yet steps back through sputbackc, so it satisfies
// steppable_back but not std::bidirectional_iterator -- backing up must go through
// operator--, never std::advance, which would take its input_iterator_tag branch and
// walk forward with a negative count.

TEST(PrefixTree, StampIteratorBacktracksTwoLevels)
{
    prefix_tree<char, int> tree;
    tree.add("abc", 1);

    // The input is a proper prefix of a stored key, so the walk descends two levels,
    // finds no value on the way and has to back up both of them.
    mem_device dev("abq");
    istreambuf sb(dev);
    istreambuf_iterator raw(sb);
    stamp_input_iterator beg(raw);
    decltype(beg) end;

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(beg, end, out);

    EXPECT_FALSE(out.has_value());
    EXPECT_EQ(*it, 'a') << "back at the position it started from";
    ++it;
    EXPECT_EQ(*it, 'b') << "the underlying buffer was repositioned, not just the wrapper";
}

TEST(PrefixTree, StampIteratorBacktracksToShallowerValue)
{
    prefix_tree<char, int> tree;
    tree.add("a", 5);
    tree.add("abc", 1);

    mem_device dev("abq");
    istreambuf sb(dev);
    istreambuf_iterator raw(sb);
    stamp_input_iterator beg(raw);
    decltype(beg) end;

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(beg, end, out);

    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 5);
    EXPECT_EQ(*it, 'b') << "one step back, just past the matched \"a\"";
}

TEST(PrefixTree, StampIteratorFullMatchDoesNotBacktrack)
{
    prefix_tree<char, int> tree;
    tree.add("abc", 1);

    mem_device dev("abcz");
    istreambuf sb(dev);
    istreambuf_iterator raw(sb);
    stamp_input_iterator beg(raw);
    decltype(beg) end;

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(beg, end, out);

    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 1);
    EXPECT_EQ(*it, 'z');
}

TEST(PrefixTree, ValueTypeOverflow)
{
    // The vector constructor numbers its keys, so 129 of them overflow int8_t.
    std::vector<const char*> strs(129, "a");

    try
    {
        prefix_tree<char, std::int8_t> tree(strs);
        FAIL() << "expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_THAT(e.what(), HasSubstr("too many strings"));
    }
}

TEST(PrefixTree, EmptyTreeLeavesOutUnset)
{
    prefix_tree<char, int> tree;

    decltype(tree)::match_out_type out{};
    std::string s = "abc";
    auto it = tree.max_match(s.begin(), s.end(), out);

    EXPECT_FALSE(out.has_value());
    EXPECT_EQ(it, s.begin());
}

TEST(PrefixTree, NullPointerInVector)
{
    std::vector<const char*> strs = {"hello", nullptr, "world"};

    try
    {
        prefix_tree<char, int> tree(strs);
        FAIL() << "expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_THAT(e.what(), HasSubstr("null pointer"));
    }
}

TEST(PrefixTree, LargeValueType)
{
    // std::string is stored out of line, so match_out_type is a pointer here,
    // not an optional.
    prefix_tree<char, std::string> tree;
    tree.add("hello", "value1");
    tree.add("world", "value2");
    tree.add("", "root_value");

    decltype(tree)::match_out_type out{};

    std::string s1 = "hello_suffix";
    auto it1 = tree.max_match(s1.begin(), s1.end(), out);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, "value1");
    EXPECT_EQ(it1, s1.begin() + 5);

    std::string s2 = "no_match";
    auto it_s2 = tree.max_match(s2.begin(), s2.end(), out);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, "root_value");
    EXPECT_EQ(it_s2, s2.begin());

    const char* c_str = "world";
    auto it_c = tree.max_match(c_str, c_str + 5, out);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, "value2");
    EXPECT_EQ(it_c, c_str + 5);
}

TEST(PrefixTree, Int8Value)
{
    std::vector<const char*> strs = {"a", "b", "c"};
    prefix_tree<char, std::int8_t> tree(strs);

    decltype(tree)::match_out_type out{};
    std::string s = "b";
    (void)tree.max_match(s.begin(), s.end(), out);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, 1);
}

TEST(PrefixTree, BacktrackSmallValueBidirectional)
{
    prefix_tree<char, int> tree;
    tree.add("ab", 1);

    std::string s = "ax";
    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(s.begin(), s.end(), out);

    EXPECT_FALSE(out.has_value());
    EXPECT_EQ(it, s.begin());
}

TEST(PrefixTree, BacktrackLargeValueBidirectional)
{
    prefix_tree<char, std::string> tree;
    tree.add("ab", "val");

    std::string s = "ax";
    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(s.begin(), s.end(), out);

    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(it, s.begin());
}

TEST(PrefixTree, BacktrackLargeValueStreambuf)
{
    // Neither "a" nor "ab" carries a value, so the walk backs up two steps.
    prefix_tree<char, std::string> tree;
    tree.add("abc", "val");

    mem_device dev("abx");
    istreambuf sb(dev);
    istreambuf_iterator beg(sb);
    decltype(beg) end;

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(beg, end, out);

    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(*it, 'a');
}

TEST(PrefixTree, RootValueLargeValueStreambuf)
{
    prefix_tree<char, std::string> tree;
    tree.add("", "root");

    mem_device dev("x");
    istreambuf sb(dev);
    istreambuf_iterator beg(sb);
    decltype(beg) end;

    decltype(tree)::match_out_type out{};
    auto it = tree.max_match(beg, end, out);

    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, "root");
    EXPECT_EQ(*it, 'x');
}
