#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/ostream.h>
#include <support/dump_info.h>
#include <support/verify.h>

void test_io_base_boolalpha_1()
{
    dump_info("Test ios_base::boolalpha case 1...");

    const std::string strue("true");
    const std::string sfalse("false");
    
    IOv2::locale<char> loc_c("C");
    IOv2::ostream ostr01(IOv2::mem_device{""}, loc_c);
    ostr01.flags(IOv2::ios_defs::boolalpha);

    ostr01 << true;
    auto [dev1, err1] = ostr01.detach();
    std::string str02 = dev1.str();
    VERIFY(str02 == strue);

    ostr01.attach(IOv2::mem_device{""});
    ostr01 << false;
    auto [dev2, err2] = ostr01.detach();
    str02 = dev2.str();
    VERIFY(str02 == sfalse);

    dump_info("Done\n");
}