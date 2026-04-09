void test_io_base_failure_1();
void test_io_base_failure_2();
void test_io_base_failure_what_1();
void test_io_base_failure_what_2();
void test_io_base_failure_what_3();
void test_io_base_failure_what_4();

void test_io_base_char_fill_1();
void test_io_base_wchar_t_fill_1();

void test_io_base_boolalpha_1();

void test_io_base_storage_1();
void test_io_base_storage_2();
void test_io_base_storage_3();

void test_io_base_manipulators();

void test_io_base()
{
    test_io_base_failure_1();
    test_io_base_failure_2();
    test_io_base_failure_what_1();
    test_io_base_failure_what_2();
    test_io_base_failure_what_3();
    test_io_base_failure_what_4();

    test_io_base_char_fill_1();
    test_io_base_wchar_t_fill_1();

    test_io_base_boolalpha_1();

    test_io_base_storage_1();
    test_io_base_storage_2();
    test_io_base_storage_3();

    test_io_base_manipulators();
}