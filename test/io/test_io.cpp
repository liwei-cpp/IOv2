#include <exception>
#include <string>
#include <common/dump_info.h>

void test_io_base();
void test_io_state_and_exp();
void test_streambuf();
void test_streambuf_iterator();

void test_istream();
void test_ostream();
void test_iostream();

void test_io_objects();

int main()
{
    try
    {
        test_io_base();
        test_io_state_and_exp();
        test_streambuf();
        test_streambuf_iterator();
        test_istream();
        test_ostream();
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