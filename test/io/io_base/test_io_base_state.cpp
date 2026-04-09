#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/arithmetic.h>
#include <io/ostream.h>
#include <common/dump_info.h>

void test_io_base_boolalpha_1()
{
    dump_info("Test ios_base::boolalpha case 1...");

    const std::string strue("true");
    const std::string sfalse("false");
    
    IOv2::locale<char> loc_c("C");
    IOv2::ostream ostr01(IOv2::mem_device{""}, loc_c);
    ostr01.flags(IOv2::ios_defs::boolalpha);

    ostr01 << true;
    std::string str02 = ostr01.detach().str();
    if (str02 != strue) throw std::runtime_error("ios_base::boolalpha check fail");

    ostr01.attach(IOv2::mem_device{""});
    ostr01 << false;
    str02 = ostr01.detach().str();
    if (str02 != sfalse) throw std::runtime_error("ios_base::boolalpha check fail");

    dump_info("Done\n");
}