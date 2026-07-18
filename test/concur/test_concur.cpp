#include <exception>
#include <string>
#include <support/dump_info.h>

void test_concur_output_1();
void test_concur_flush_1();
void test_concur_sentryless_1();
void test_concur_ignore_ws_1();
void test_concur_state_1();
void test_concur_tie_1();

void test_istream_sync_char_1();
void test_istream_sync_wchar_t_1();
void test_ostream_sync_char_1();
void test_ostream_sync_wchar_t_1();

int main()
{
    try
    {
        test_concur_output_1();
        test_concur_flush_1();
        test_concur_sentryless_1();
        test_concur_ignore_ws_1();
        test_concur_state_1();
        test_concur_tie_1();

        test_istream_sync_char_1();
        test_istream_sync_wchar_t_1();
        test_ostream_sync_char_1();
        test_ostream_sync_wchar_t_1();
        return 0;
    }
    catch (const std::exception& e)
    {
        dump_info((std::string("\n[!] Concurrency test failed with exception: ") + e.what() + "\n").c_str());
        return 1;
    }
    catch (...)
    {
        dump_info("\n[!] Concurrency test failed with an unknown exception.\n");
        return 1;
    }
}
