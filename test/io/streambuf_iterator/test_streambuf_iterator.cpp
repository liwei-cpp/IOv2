void test_istreambuf_iterator_gen_1();
void test_istreambuf_iterator_gen_2();
void test_istreambuf_iterator_get_1();
void test_istreambuf_iterator_sentinel_1();
void test_istreambuf_iterator_sentinel_2();
void test_istreambuf_iterator_putback_1();

void test_ostreambuf_iterator_gen_1();
void test_ostreambuf_iterator_put_1();

void test_streambuf_iterator()
{
    test_istreambuf_iterator_gen_1();
    test_istreambuf_iterator_gen_2();
    test_istreambuf_iterator_get_1();
    test_istreambuf_iterator_sentinel_1();
    test_istreambuf_iterator_sentinel_2();
    test_istreambuf_iterator_putback_1();
    
    test_ostreambuf_iterator_gen_1();
    test_ostreambuf_iterator_put_1();
}