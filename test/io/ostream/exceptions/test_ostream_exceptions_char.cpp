#include <stdexcept>
#include <string>
#include <device/mem_device.h>
#include <io/fp_defs/char_and_str.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <common/dump_info.h>
#include <common/verify.h>

namespace
{
    struct foobar: std::exception { };
    struct dummy_type {};
}

namespace IOv2
{
    template <typename TChar>
    struct writer<TChar, dummy_type>
    {
        template <typename TIter>
            requires (std::is_same_v<TChar, typename TIter::value_type>)
        static TIter swrite(TIter, ios_base<TChar>&, const locale<TChar>&, dummy_type)
        {
            throw foobar();
        }
    };
}

void test_ostream_exceptions_char_1()
{
    dump_info("Test ostream<char> exceptions case 1...");

    auto helper = []<template<typename, typename> class T>()
    {
        T strm(IOv2::mem_device{""});
        strm.exceptions(IOv2::ios_defs::otherfailbit);
        try
        {
            strm << dummy_type{};
            dump_info("unreachable code");
            std::abort();
        }
        catch (const foobar&)
        {
            // the fail of strm will cause the stream_file to be set
            VERIFY(!strm);
        }
        catch (...)
        {
            VERIFY(false);
        }
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();
    
    dump_info("Done\n");
}

void test_ostream_exceptions_char_2()
{
    dump_info("Test ostream<char> exceptions case 2...");

    auto helper = []<template<typename, typename> class T>()
    {
        T out(IOv2::mem_device{""});
        out.setf(IOv2::ios_defs::unitbuf);
        out.exceptions(IOv2::ios_defs::cvtfailbit);
        out << dummy_type{};
        VERIFY(!out);

        out.clear();
        VERIFY((bool)out);
        out.exceptions(IOv2::ios_defs::cvtfailbit);
        out << dummy_type{};
        VERIFY(!out);
    };

    helper.operator()<IOv2::ostream>();
    helper.operator()<IOv2::iostream>();

    dump_info("Done\n");
}