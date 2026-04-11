#include <exception>
#include <string>
#include <common/dump_info.h>

void test_root_cvt();
void test_code_cvt();
void test_crypt_cvt();
void test_comp_cvt();
void test_cvt_pipe_creator();
void test_runtime_cvt();

void test_cvt_io();

int main()
{
    try
    {
        test_root_cvt();
        test_code_cvt();
        test_comp_cvt();
        test_crypt_cvt();
        test_cvt_pipe_creator();
        test_runtime_cvt();
        test_cvt_io();
        return 0;
    }
    catch (const std::exception& e)
    {
        dump_info((std::string("\n[!] CVT test failed with exception: ") + e.what() + "\n").c_str());
        return 1;
    }
    catch (...)
    {
        dump_info("\n[!] CVT test failed with an unknown exception.\n");
        return 1;
    }
}
