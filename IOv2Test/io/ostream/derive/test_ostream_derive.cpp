#include <stdexcept>
#include <string>

#include <cvt/code_cvt.h>
#include <cvt/comp/zlib_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <cvt/crypt/hash_cvt.h>
#include <cvt/cvt_pipe_creator.h>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/fp_defs/char_and_str.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <locale/locale.h>

#include <common/dump_info.h>
#include <common/verify.h>

namespace
{
struct DevOstream : public IOv2::ostream<IOv2::mem_device<char>, char>
{
    int x = 20;
};

struct DevIOstream : public IOv2::ostream<IOv2::mem_device<char>, char>
{
    int x = 50;
};
}

void test_ostream_derive_1()
{
    dump_info("Test ostream derive case 1...");

    DevOstream obj1;
    // make sure the << operator should return DevOstream object
    VERIFY((obj1 << "hello").x == 20);

    DevIOstream obj2;
    // make sure the << operator should return DevIOstream object
    VERIFY((obj2 << "hello").x == 50);

    dump_info("Done\n");
}

namespace
{
struct Level {
    std::string val;
};

class MyLogger : public IOv2::ostream<IOv2::mem_device<char>, char>
{
public:
    using IOv2::ostream<IOv2::mem_device<char>, char>::ostream;

    MyLogger& operator<<(const Level& l) {
        *this << "[" << l.val << "] ";
        return *this;
    }
};
}

void test_ostream_derive_2()
{
    MyLogger logger;
    logger << Level{"DEBUG"} << "User login\n"
           << Level{"WARN"} << "something happened";
    auto str = logger.detach().str();
    VERIFY(str == "[DEBUG] User login\n[WARN] something happened");
}

void test_ostream_derive()
{
    test_ostream_derive_1();
    test_ostream_derive_2();
}