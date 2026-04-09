#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/ostream.h>
#include <common/dump_info.h>

namespace
{
    template <typename C>
    struct tabby_mctype : IOv2::ctype_conf<C>
    {
        tabby_mctype()
            : IOv2::ctype_conf<C>("C") {}
        virtual C widen(char c) const override
        {
            return (c == ' ') ? '\t' : c;
        }
    };
}

void test_io_base_char_fill_1()
{
    dump_info("Test ios_base<char> fill case 1...");
    {
        IOv2::ostream out{IOv2::mem_device{""}};
        IOv2::locale<char> loc = IOv2::locale<char>().involve(std::make_shared<tabby_mctype<char>>());
        out.locale(loc);
        
        // Imbuing a new locale doesn't affect fill().
        if (out.fill() != ' ') throw std::runtime_error("ios_base fill check fail");
        out.fill('*');
        out.locale(IOv2::locale<char>{});
        if (out.fill() != '*') throw std::runtime_error("ios_base fill check fail");
    }

    dump_info("Done\n");
}

void test_io_base_wchar_t_fill_1()
{
    dump_info("Test ios_base<wchar_t> fill case 1...");
    {
        IOv2::ostream out{IOv2::mem_device{L""}};
        IOv2::locale<wchar_t> loc = IOv2::locale<wchar_t>().involve(std::make_shared<tabby_mctype<wchar_t>>());
        out.locale(loc);
        
        // Imbuing a new locale doesn't affect fill().
        if (out.fill() != L' ') throw std::runtime_error("ios_base fill check fail");
        out.fill(L'*');
        out.locale(IOv2::locale<wchar_t>{});
        if (out.fill() != L'*') throw std::runtime_error("ios_base fill check fail");
    }

    dump_info("Done\n");
}