#include <cstring>
#include <stdexcept>
#include <system_error>
#include <string>
#include <io/io_base.h>
#include <common/dump_info.h>
namespace
{
struct E : IOv2::ios_defs::failure
{
    E(const char* s)
        : IOv2::ios_defs::failure(s, std::make_error_code(std::io_errc::stream)) { }
};
}

void test_io_base_failure_1()
{
    dump_info("Test ios_base::failure case 1...");

    {
        IOv2::ios_defs::failure e("io error");
        if (std::string(e.what()).find("io error") == std::string::npos)
            throw std::runtime_error("ios_base::failure check fail");
    }
    {
        E e("io error");
        if (std::string(e.what()).find("io error") == std::string::npos)
            throw std::runtime_error("ios_base::failure check fail");
    }

    dump_info("Done\n");
}

void test_io_base_failure_2()
{
    dump_info("Test ios_base::failure case 2...");

    std::error_code ec, def_ec;
    ec = std::make_error_code(std::errc::executable_format_error);
    def_ec = std::io_errc::stream;
    IOv2::ios_defs::failure e1("string literal");
    if (e1.code() != def_ec) throw std::runtime_error("ios_base::failure check fail");
    IOv2::ios_defs::failure e2(std::string("std::string"));
    if (e2.code() != def_ec) throw std::runtime_error("ios_base::failure check fail");
    IOv2::ios_defs::failure e3("string literal", ec);
    if (e3.code() != ec) throw std::runtime_error("ios_base::failure check fail");
    IOv2::ios_defs::failure e4(std::string("std::string"), ec);
    if (e4.code() != ec) throw std::runtime_error("ios_base::failure check fail");

    dump_info("Done\n");
}

void test_io_base_failure_what_1()
{
    dump_info("Test ios_base::failure::what case 1...");

    std::string s("lack of sunlight, no water error");

    // 1
    IOv2::ios_defs::failure obj1 = IOv2::ios_defs::failure(s);

    // 2
    IOv2::ios_defs::failure obj2(s);

    if (obj1.what() != s) throw std::runtime_error("ios_base::failure check fail");
    if (obj2.what() != s) throw std::runtime_error("ios_base::failure check fail");

    dump_info("Done\n");
}

namespace
{
class fuzzy_logic : public IOv2::ios_defs::failure
{
public:
  fuzzy_logic() : IOv2::ios_defs::failure("whoa") { }
};
}

void test_io_base_failure_what_2()
{
    dump_info("Test ios_base::failure::what case 2...");

    try
    {
        throw fuzzy_logic();
    }
    catch(const fuzzy_logic& obj)
    {
        if (obj.what() != std::string("whoa"))
            throw std::runtime_error("ios_base::failure check fail");
    }

    dump_info("Done\n");
}

namespace
{
    void allocate_on_stack(void) 
    {
        const size_t num = 512;
        char array[num];
        for (size_t i = 0; i < num; i++) 
            array[i]=0;
        for (size_t i = 0; i < num; i++) 
            array[i]=array[i]; // Suppress unused warning.
    }
}
void test_io_base_failure_what_3()
{
    dump_info("Test ios_base::failure::what case 3...");
    const std::string s("CA ISO emergency once again:immediate power down");
    const char* strlit1 = "wish I lived in Palo Alto";
    const char* strlit2 = "...or Santa Barbara";
    IOv2::ios_defs::failure obj1(s);
    
    {
        const std::string s2(strlit1);
        IOv2::ios_defs::failure obj2(s2);
        obj1 = obj2;
    }
    allocate_on_stack();
    if (std::strcmp(strlit1, obj1.what()) != 0) throw std::runtime_error("ios_base::failure check fail");
    
    {
        const std::string s3(strlit2);
        IOv2::ios_defs::failure obj3 = IOv2::ios_defs::failure(s3);
        obj1 = obj3;
    }
    if (std::strcmp(strlit2, obj1.what()) != 0) throw std::runtime_error("ios_base::failure check fail");

    dump_info("Done\n");
}

void test_io_base_failure_what_4()
{
    dump_info("Test ios_base::failure::what case 4...");

    typedef IOv2::ios_defs::failure test_type;

    const std::string xxx(10000, 'x');
    test_type t(xxx);
    if (std::strcmp(t.what(), xxx.c_str()) != 0) throw std::runtime_error("ios_base::failure check fail");

    dump_info("Done\n");
}