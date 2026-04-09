#include <io/io_base.h>
#include <common/dump_info.h>

void test_io_state_and_exp_1()
{
    dump_info("Test io_state_and_exp case 1...");
    {
        IOv2::io_state_and_exp ios_01;
        if (ios_01.exceptions() != IOv2::ios_defs::goodbit)
            throw std::runtime_error("io_state_and_exp exceptions check fail");
    }
    {
        IOv2::io_state_and_exp ios_01;
        try
        {
            ios_01.exceptions(IOv2::ios_defs::cvtfailbit);
        }
        catch(...)
        {
            dump_info("Unreachable code\n");
            std::abort();
        }
        auto iostate02 = ios_01.exceptions();
        if (iostate02 != IOv2::ios_defs::cvtfailbit)
            throw std::runtime_error("io_state_and_exp exceptions check fail");
    }
    {
        IOv2::ios_defs::iostate iostate02 = IOv2::ios_defs::goodbit;
        IOv2::io_state_and_exp ios_01;
        ios_01.clear(IOv2::ios_defs::cvtfailbit);
        try
        {
            ios_01.exceptions(IOv2::ios_defs::cvtfailbit);
            dump_info("Unreachable code\n");
            std::abort();
        }
        catch (IOv2::ios_defs::failure&)
        {
            iostate02 = ios_01.exceptions();
        }
        catch(...)
        {
            dump_info("Unreachable code\n");
            std::abort();
        }
        if (iostate02 != IOv2::ios_defs::cvtfailbit)
            throw std::runtime_error("io_state_and_exp exceptions check fail");
    }
    dump_info("Done\n");
}

void test_io_state_and_exp_2()
{
    dump_info("Test io_state_and_exp case 2...");

    IOv2::io_state_and_exp stream;
    try
    {
        stream.setstate(IOv2::ios_defs::cvtfailbit);
        stream.exceptions(IOv2::ios_defs::cvtfailbit);
        dump_info("Unreachable code\n");
        std::abort();
    }
    catch (...)
    {
        // Don't clear.
    }
    
    try
    {
        // Calls clear(rdstate()), which throws in this case.
        stream.setstate(IOv2::ios_defs::goodbit);
        dump_info("Unreachable code\n");
        std::abort();
    }
    catch (...) {}

    dump_info("Done\n");
}

void test_io_state_and_exp()
{
    test_io_state_and_exp_1();
    test_io_state_and_exp_2();
}