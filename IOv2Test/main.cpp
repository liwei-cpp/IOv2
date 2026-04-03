void test_device();
void test_cvt();
void test_facet();
void test_locale();
void test_io();

int main()
{
    test_device();
    test_cvt();
    test_facet();
    test_locale();
    test_io();
}


//#include <filesystem>
//#include <facet/messages.h>
//#include <io/objects/objects.h>
//#define _(STRING) STRING
//
//int main()
//{
//    using namespace IOv2;
//    base_ft<messages>::bind_text_domain("my_component", std::filesystem::current_path().string());
//    const messages<char> facet(std::make_shared<messages_conf<char>>("my_component", "zh_CN", "zh_CN.UTF-8", false));
//
//    cout << facet.translate(_("Hello")) << endl;
//    cout << facet.head_entry() << endl;
//}
//