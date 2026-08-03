void test_io_base_char_fill_1();
void test_io_base_wchar_t_fill_1();

void test_io_base_width_1();
void test_io_base_width_2();
void test_io_base_width_3();
void test_io_base_width_wchar_t_1();

void test_io_base_boolalpha_1();
void test_io_base_state_handle_exception_idempotent_1();

void test_io_base_storage_1();
void test_io_base_storage_2();
void test_io_base_storage_3();
void test_io_base_storage_4();

void test_io_base_manipulators();

void test_io_base()
{
    test_io_base_char_fill_1();
    test_io_base_wchar_t_fill_1();

    test_io_base_width_1();
    test_io_base_width_2();
    test_io_base_width_3();
    test_io_base_width_wchar_t_1();

    test_io_base_boolalpha_1();
    test_io_base_state_handle_exception_idempotent_1();

    test_io_base_storage_1();
    test_io_base_storage_2();
    test_io_base_storage_3();
    test_io_base_storage_4();

    test_io_base_manipulators();
}