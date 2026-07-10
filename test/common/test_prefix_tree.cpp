#include <common/prefix_tree.h>
#include <support/verify.h>
#include <support/dump_info.h>
#include <string>
#include <vector>
#include <sstream>
#include <device/mem_device.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>

using namespace IOv2;

void test_prefix_tree_basic()
{
    dump_info("Test prefix_tree basic...");
    prefix_tree<char, int> tree;
    tree.add("hello", 1);
    tree.add("world", 2);
    tree.add("he", 3);

    decltype(tree)::match_out_type out;
    std::string s1 = "hello";
    auto it1 = tree.max_match(s1.begin(), s1.end(), out);
    VERIFY(out && *out == 1);
    VERIFY(it1 == s1.end());

    std::string s2 = "he";
    it1 = tree.max_match(s2.begin(), s2.end(), out);
    VERIFY(out && *out == 3);
    VERIFY(it1 == s2.end());

    std::string s3 = "hell";
    it1 = tree.max_match(s3.begin(), s3.end(), out);
    VERIFY(out && *out == 3); // "he" is the max match
    VERIFY(it1 == s3.begin() + 2);

    std::string s4 = "world";
    it1 = tree.max_match(s4.begin(), s4.end(), out);
    VERIFY(out && *out == 2);
    VERIFY(it1 == s4.end());

    std::string s5 = "not_found";
    it1 = tree.max_match(s5.begin(), s5.end(), out);
    VERIFY(!out); // remains empty
    VERIFY(it1 == s5.begin());
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

    // Duplicate test for large types
    prefix_tree<char, std::string> tree_large;
    tree_large.add("test", "val1");
    try {
        tree_large.add("test", "val2");
        VERIFY(false);
    } catch (const std::runtime_error& e) {
        // Expected
    }
    dump_info("Done\n");
}

void test_prefix_tree_string_view()
{
    dump_info("Test prefix_tree string_view...");
    prefix_tree<char, int> tree;
    std::string_view sv = "view";
    tree.add(sv, 5);

    decltype(tree)::match_out_type out;
    auto it = tree.max_match(sv.begin(), sv.end(), out);
    VERIFY(out && *out == 5);
    VERIFY(it == sv.end());
    dump_info("Done\n");
}

void test_prefix_tree_vector_constructor()
{
    dump_info("Test prefix_tree vector constructor...");
    std::vector<const char*> strs = {"apple", "banana", "cherry"};
    prefix_tree<char, int> tree(strs);

    decltype(tree)::match_out_type out;
    std::string s = "banana";
    (void)tree.max_match(s.begin(), s.end(), out);
    VERIFY(out && *out == 1); // Index 1 in the vector

    // Test empty vector
    std::vector<const char*> empty_strs;
    prefix_tree<char, int> empty_tree(empty_strs);
    dump_info("Done\n");
}

void test_prefix_tree_root_value()
{
    dump_info("Test prefix_tree root value...");
    prefix_tree<char, int> tree;
    tree.add("", 100); // Set value at root
    tree.add("a", 1);

    decltype(tree)::match_out_type out;
    std::string s = "b"; // Does not match any child
    auto it = tree.max_match(s.begin(), s.end(), out);
    VERIFY(out && *out == 100); // Should pick root value
    VERIFY(it == s.begin());
    dump_info("Done\n");
}

void test_prefix_tree_greedy_match()
{
    dump_info("Test prefix_tree greedy match...");
    prefix_tree<char, int> tree;
    tree.add("abc", 1);
    tree.add("abcd", 2);

    decltype(tree)::match_out_type out;
    std::string s = "abcde"; 
    auto it = tree.max_match(s.begin(), s.end(), out);
    VERIFY(out && *out == 2); // Should match longest "abcd"
    VERIFY(it == s.begin() + 4);
    dump_info("Done\n");
}

void test_prefix_tree_istreambuf()
{
    dump_info("Test prefix_tree istreambuf_iterator...");
    prefix_tree<char, int> tree;
    tree.add("", 100); // Root value
    tree.add("abc", 1);
    tree.add("ab", 2);

    // Test partial match and backtracking
    {
        mem_device dev("abxe");
        istreambuf sb(dev);
        istreambuf_iterator beg(sb);
        decltype(beg) end;
        
        decltype(tree)::match_out_type out;
        auto it = tree.max_match(beg, end, out);
        
        VERIFY(out && *out == 2); // Should match "ab"
        VERIFY(*it == 'x');
    }

    // Test backtracking to root
    {
        mem_device dev("axe");
        istreambuf sb(dev);
        istreambuf_iterator beg(sb);
        decltype(beg) end;
        
        decltype(tree)::match_out_type out;
        auto it = tree.max_match(beg, end, out);
        
        VERIFY(out && *out == 100); // Should match root
        VERIFY(*it == 'a');
    }
    dump_info("Done\n");
}

void test_prefix_tree_overflow()
{
    dump_info("Test prefix_tree TValue overflow check...");
    
    // int8_t max is 127. We provide 129 elements to trigger overflow.
    std::vector<const char*> strs;
    for (int i = 0; i < 129; ++i) {
        strs.push_back("a"); 
    }

    try {
        prefix_tree<char, int8_t> tree(strs);
        VERIFY(false); // Should have thrown
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        VERIFY(msg.find("too many strings") != std::string::npos);
    }
    dump_info("Done\n");
}

void test_prefix_tree_uninitialized_out()
{
    dump_info("Test prefix_tree uninitialized out (Issue 1)...");
    prefix_tree<char, int> tree;
    // Tree is empty, no root value, no children.
    
    decltype(tree)::match_out_type out;
    std::string s = "abc";
    auto it = tree.max_match(s.begin(), s.end(), out);
    
    VERIFY(!out); 
    VERIFY(it == s.begin());
    dump_info("Done\n");
}

void test_prefix_tree_nullptr_check()
{
    dump_info("Test prefix_tree nullptr check (Issue 5)...");
    std::vector<const char*> strs = {"hello", nullptr, "world"};
    try {
        prefix_tree<char, int> tree(strs);
        VERIFY(false); // Should have thrown
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        VERIFY(msg.find("null pointer") != std::string::npos);
    }
    dump_info("Done\n");
}

void test_prefix_tree_large_value()
{
    dump_info("Test prefix_tree large value type...");
    // std::string is a large type (> 16 bytes usually)
    prefix_tree<char, std::string> tree;
    tree.add("hello", "value1");
    tree.add("world", "value2");
    tree.add("", "root_value");

    decltype(tree)::match_out_type out;
    std::string s1 = "hello_suffix";
    auto it1 = tree.max_match(s1.begin(), s1.end(), out);
    VERIFY(out && *out == "value1");
    VERIFY(it1 == s1.begin() + 5);

    std::string s2 = "no_match";
    auto it_s2 = tree.max_match(s2.begin(), s2.end(), out);
    VERIFY(out && *out == "root_value");
    VERIFY(it_s2 == s2.begin());
    
    // Test with const char*
    const char* c_str = "world";
    auto it_c = tree.max_match(c_str, c_str + 5, out);
    VERIFY(out && *out == "value2");
    VERIFY(it_c == c_str + 5);

    dump_info("Done\n");
}

void test_prefix_tree_int8_normal()
{
    dump_info("Test prefix_tree int8_t normal usage...");
    std::vector<const char*> strs = {"a", "b", "c"};
    prefix_tree<char, int8_t> tree(strs);
    
    decltype(tree)::match_out_type out;
    std::string s = "b";
    (void)tree.max_match(s.begin(), s.end(), out);
    VERIFY(out && *out == 1);
    dump_info("Done\n");
}

void test_prefix_tree_backtracking_variants()
{
    dump_info("Test prefix_tree backtracking variants...");
    
    // Test small type backtracking with bidirectional iterator
    {
        prefix_tree<char, int> tree;
        tree.add("ab", 1);
        std::string s = "ax";
        decltype(tree)::match_out_type out;
        auto it = tree.max_match(s.begin(), s.end(), out);
        VERIFY(!out);
        VERIFY(it == s.begin());
    }

    // Test large type backtracking with bidirectional iterator
    {
        prefix_tree<char, std::string> tree;
        tree.add("ab", "val");
        std::string s = "ax";
        decltype(tree)::match_out_type out;
        auto it = tree.max_match(s.begin(), s.end(), out);
        VERIFY(!out);
        VERIFY(it == s.begin());
    }

    // Test large type with istreambuf_iterator backtracking
    {
        prefix_tree<char, std::string> tree;
        tree.add("abc", "val"); // 'a' and 'ab' have no values
        mem_device dev("abx");
        istreambuf sb(dev);
        istreambuf_iterator beg(sb);
        decltype(beg) end;
        decltype(tree)::match_out_type out;
        auto it = tree.max_match(beg, end, out);
        VERIFY(!out);
        VERIFY(*it == 'a'); // Should have backtracked 2 steps to 'a'
    }
    
    // Test matching root for large type in istreambuf
    {
        prefix_tree<char, std::string> tree;
        tree.add("", "root");
        mem_device dev("x");
        istreambuf sb(dev);
        istreambuf_iterator beg(sb);
        decltype(beg) end;
        decltype(tree)::match_out_type out;
        auto it = tree.max_match(beg, end, out);
        VERIFY(out && *out == "root");
        VERIFY(*it == 'x');
    }
    dump_info("Done\n");
}

void test_prefix_tree()
{
    test_prefix_tree_basic();
    test_prefix_tree_duplicate_handling();
    test_prefix_tree_string_view();
    test_prefix_tree_vector_constructor();
    test_prefix_tree_root_value();
    test_prefix_tree_greedy_match();
    test_prefix_tree_istreambuf();
    test_prefix_tree_overflow();
    test_prefix_tree_uninitialized_out();
    test_prefix_tree_nullptr_check();
    test_prefix_tree_large_value();
    test_prefix_tree_int8_normal();
    test_prefix_tree_backtracking_variants();
}
