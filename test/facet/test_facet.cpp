#include <exception>
#include <string>
#include <common/dump_info.h>

void test_ctype();
void test_collate();
void test_numeric();
void test_monetary();
void test_timeio();
void test_messages();

int main()
{
    try
    {
        test_ctype();
        test_collate();
        test_numeric();
        test_monetary();
        test_timeio();
        test_messages();
        return 0;
    }
    catch (const std::exception& e)
    {
        dump_info((std::string("\n[!] Facet test failed with exception: ") + e.what() + "\n").c_str());
        return 1;
    }
    catch (...)
    {
        dump_info("\n[!] Facet test failed with an unknown exception.\n");
        return 1;
    }
}