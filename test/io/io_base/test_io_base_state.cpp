#include <exception>
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

// Handling the same exception twice is observably the same as handling it once. Nested
// handling points make this routine: put() handles its own exception and then rethrows on
// account of the mask, so the same exception reaches the caller's catch as well. What a
// regression would look like is the message being swapped, not a crash -- clear() falls back
// to a generic stand-in ("stream failure bit has been set") whenever the category holds no
// stored exception_ptr, and the first pass consumes the one that was stored.
void test_io_base_state_handle_exception_idempotent_1()
{
    dump_info("Test ios_base::handle_exception idempotence case 1...");

    std::string original;
    std::exception_ptr ex;
    try
    {
        throw IOv2::stream_error("handle_exception idempotence probe");
    }
    catch (const IOv2::stream_error& e)
    {
        original = e.what();
        ex = std::current_exception();
    }

    IOv2::ostream oss(IOv2::mem_device{""}, IOv2::locale<char>("C"));
    oss.exceptions(IOv2::ios_defs::strfailbit);

    std::string first;
    try { oss.handle_exception(ex); }
    catch (const IOv2::stream_error& e) { first = e.what(); }
    VERIFY(first == original);
    VERIFY(oss.str_fail());

    std::string second;
    try { oss.handle_exception(ex); }
    catch (const IOv2::stream_error& e) { second = e.what(); }
    VERIFY(second == original);
    VERIFY(oss.str_fail());

    // With the bit out of the mask neither pass throws, and the bit stays set.
    oss.clear();
    oss.exceptions(IOv2::ios_defs::goodbit);
    VERIFY(oss.good());

    oss.handle_exception(ex);
    oss.handle_exception(ex);
    VERIFY(oss.str_fail());

    dump_info("Done\n");
}
