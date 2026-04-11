#include <exception>
#include <string>
#include <common/dump_info.h>

void test_locale_char_1();
void test_locale_char_2();
void test_locale_char_3();
void test_locale_char_4();
void test_locale_char_5();
void test_locale_char_6();
void test_locale_char_7();
void test_locale_char_8();
void test_locale_char_9();

void test_locale_wchar_t_1();
void test_locale_wchar_t_2();
void test_locale_wchar_t_3();
void test_locale_wchar_t_4();
void test_locale_wchar_t_5();
void test_locale_wchar_t_6();
void test_locale_wchar_t_7();
void test_locale_wchar_t_8();
void test_locale_wchar_t_9();
void test_locale_wchar_t_10();

void test_locale_char32_t_1();
void test_locale_char32_t_2();
void test_locale_char32_t_3();
void test_locale_char32_t_4();
void test_locale_char32_t_5();
void test_locale_char32_t_6();
void test_locale_char32_t_7();
void test_locale_char32_t_8();
void test_locale_char32_t_9();
void test_locale_char32_t_10();

void test_locale_char8_t_1();
void test_locale_char8_t_2();
void test_locale_char8_t_3();
void test_locale_char8_t_4();
void test_locale_char8_t_5();
void test_locale_char8_t_6();
void test_locale_char8_t_7();
void test_locale_char8_t_8();
void test_locale_char8_t_9();
void test_locale_char8_t_10();

int main()
{
    try
    {
        test_locale_char_1();
        test_locale_char_2();
        test_locale_char_3();
        test_locale_char_4();
        test_locale_char_5();
        test_locale_char_6();
        test_locale_char_7();
        test_locale_char_8();
        test_locale_char_9();

        test_locale_wchar_t_1();
        test_locale_wchar_t_2();
        test_locale_wchar_t_3();
        test_locale_wchar_t_4();
        test_locale_wchar_t_5();
        test_locale_wchar_t_6();
        test_locale_wchar_t_7();
        test_locale_wchar_t_8();
        test_locale_wchar_t_9();
        test_locale_wchar_t_10();
        
        test_locale_char32_t_1();
        test_locale_char32_t_2();
        test_locale_char32_t_3();
        test_locale_char32_t_4();
        test_locale_char32_t_5();
        test_locale_char32_t_6();
        test_locale_char32_t_7();
        test_locale_char32_t_8();
        test_locale_char32_t_9();
        test_locale_char32_t_10();

        test_locale_char8_t_1();
        test_locale_char8_t_2();
        test_locale_char8_t_3();
        test_locale_char8_t_4();
        test_locale_char8_t_5();
        test_locale_char8_t_6();
        test_locale_char8_t_7();
        test_locale_char8_t_8();
        test_locale_char8_t_9();
        test_locale_char8_t_10();

        return 0;
    }
    catch (const std::exception& e)
    {
        dump_info((std::string("\n[!] Locale test failed with exception: ") + e.what() + "\n").c_str());
        return 1;
    }
    catch (...)
    {
        dump_info("\n[!] Locale test failed with an unknown exception.\n");
        return 1;
    }
}