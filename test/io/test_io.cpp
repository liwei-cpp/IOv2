#include <exception>
#include <string>
#include <support/dump_info.h>

void test_io_base();
void test_ios_state();
void test_io_traits();
void test_streambuf();
void test_streambuf_iterator();

void test_iostream();

void test_io_objects();

int main()
{
    try
    {
        test_io_base();
        test_ios_state();
        test_io_traits();
        test_streambuf();
        test_streambuf_iterator();
        test_iostream();
        test_io_objects();
        return 0;
    }
    catch (const std::exception& e)
    {
        dump_info((std::string("\n[!] IO test failed with exception: ") + e.what() + "\n").c_str());
        return 1;
    }
    catch (...)
    {
        dump_info("\n[!] IO test failed with an unknown exception.\n");
        return 1;
    }
}