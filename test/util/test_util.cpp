#include <exception>
#include <string>
#include <common/dump_info.h>

void test_prefix_tree();
void test_lru_cache();
void test_clocale_wrapper();

int main()
{
    try
    {
        test_prefix_tree();
        test_lru_cache();
        test_clocale_wrapper();
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
