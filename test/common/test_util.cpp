#include <exception>
#include <string>
#include <support/dump_info.h>

// test_prefix_tree is gone: test_prefix_tree.cpp is now GoogleTest, and its
// cases register themselves. This file is still what derive_suites.py reads to
// order the entries below, so it shrinks as the directory is converted rather
// than being deleted up front.
void test_lru_cache();
void test_clocale_wrapper();
void test_clocale_wrapper_exception_paths();
void test_stamp_input_iterator();
void test_copyable_mutex();
void test_copyable_atomic();

int main()
{
    try
    {
        test_lru_cache();
        test_clocale_wrapper();
        test_clocale_wrapper_exception_paths();
        test_stamp_input_iterator();
        test_copyable_mutex();
        test_copyable_atomic();
        return 0;
    }
    catch (const std::exception& e)
    {
        dump_info((std::string("\n[!] Util test failed with exception: ") + e.what() + "\n").c_str());
        return 1;
    }
    catch (...)
    {
        dump_info("\n[!] Util test failed with an unknown exception.\n");
        return 1;
    }
}
