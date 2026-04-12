#include <common/prefix_tree.h>
#include <common/verify.h>
#include <common/dump_info.h>
#include <string>
#include <vector>

using namespace IOv2;

void test_prefix_tree_basic()
{
    dump_info("Test prefix_tree basic...");
    prefix_tree<char, int> tree;
    tree.add("hello", 1);
    tree.add("world", 2);
    tree.add("he", 3);

    int out = -1;
    std::string s1 = "hello";
    auto it1 = tree.max_match(s1.begin(), s1.end(), out);
    VERIFY(out == 1);
    VERIFY(it1 == s1.end());

    std::string s2 = "he";
    auto it2 = tree.max_match(s2.begin(), s2.end(), out);
    VERIFY(out == 3);
    VERIFY(it2 == s2.end());

    std::string s3 = "hell";
    auto it3 = tree.max_match(s3.begin(), s3.end(), out);
    VERIFY(out == 3); // "he" is the max match
    VERIFY(it3 == s3.begin() + 2);

    std::string s4 = "world";
    auto it4 = tree.max_match(s4.begin(), s4.end(), out);
    VERIFY(out == 2);
    VERIFY(it4 == s4.end());

    std::string s5 = "not_found";
    auto it5 = tree.max_match(s5.begin(), s5.end(), out);
    VERIFY(out == -1); // root default value
    VERIFY(it5 == s5.begin());
    dump_info("Done\n");
}

void test_prefix_tree_duplicate_handling()
{
    dump_info("Test prefix_tree duplicate handling...");
    prefix_tree<char, int> tree;
    tree.add("test", 10);
    
    // Adding same key with same value should NOT throw
    try {
        tree.add("test", 10);
    } catch (...) {
        VERIFY(false); // Should not throw
    }

    // Adding same key with different value SHOULD throw
    try {
        tree.add("test", 20);
        VERIFY(false); // Should have thrown
    } catch (const std::runtime_error& e) {
        // Expected
    }

    // Test with iterator version
    std::string s = "iter";
    tree.add(s.begin(), s.end(), 100);
    try {
        tree.add(s.begin(), s.end(), 100);
    } catch (...) {
        VERIFY(false); // Should not throw
    }

    try {
        tree.add(s.begin(), s.end(), 200);
        VERIFY(false); // Should have thrown
    } catch (const std::runtime_error& e) {
        // Expected
    }
    dump_info("Done\n");
}

void test_prefix_tree_sentinel_value()
{
    dump_info("Test prefix_tree sentinel value...");
    prefix_tree<char, int> tree(999); // Use 999 as sentinel

    // Should NOT be able to add the sentinel value
    try {
        tree.add("bad", 999);
        VERIFY(false); // Should have thrown
    } catch (const std::runtime_error& e) {
        // Expected
    }

    // Should be able to add other values
    tree.add("good", 1);
    int out = -1;
    std::string s = "good";
    tree.max_match(s.begin(), s.end(), out);
    VERIFY(out == 1);
    dump_info("Done\n");
}

void test_prefix_tree_string_view()
{
    dump_info("Test prefix_tree string_view...");
    prefix_tree<char, int> tree;
    std::string_view sv = "view";
    tree.add(sv, 5);

    int out = -1;
    auto it = tree.max_match(sv.begin(), sv.end(), out);
    VERIFY(out == 5);
    VERIFY(it == sv.end());
    dump_info("Done\n");
}

void test_prefix_tree_vector_constructor()
{
    dump_info("Test prefix_tree vector constructor...");
    std::vector<const char*> strs = {"apple", "banana", "cherry"};
    prefix_tree<char, int> tree(strs);

    int out = -1;
    std::string s = "banana";
    tree.max_match(s.begin(), s.end(), out);
    VERIFY(out == 1); // Index 1 in the vector
    dump_info("Done\n");
}

void test_prefix_tree()
{
    test_prefix_tree_basic();
    test_prefix_tree_duplicate_handling();
    test_prefix_tree_sentinel_value();
    test_prefix_tree_string_view();
    test_prefix_tree_vector_constructor();
}
