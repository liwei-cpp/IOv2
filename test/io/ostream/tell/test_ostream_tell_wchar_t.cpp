#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <common/defs.h>
#include <device/mem_device.h>
#include <device/file_device.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>
#include <io/io_manip.h>
#include <io/ostream.h>
#include <io/iostream.h>
#include <support/dump_info.h>
#include <support/file_guard.h>
#include <support/verify.h>

namespace
{
// wchar_t counterpart of the throw_tell_device in the char tell test: a mem_device wrapper
// whose dtell() throws once a shared flag is flipped (after construction).
template <class CharT>
class throw_tell_device
{
public:
    using char_type = CharT;
    explicit throw_tell_device(std::basic_string<CharT> info = {})
        : m_dev(std::move(info)), m_fail(std::make_shared<bool>(false)) {}
    throw_tell_device(const throw_tell_device&) = default;
    throw_tell_device(throw_tell_device&&) noexcept = default;
    throw_tell_device& operator=(const throw_tell_device&) = default;
    throw_tell_device& operator=(throw_tell_device&&) noexcept = default;

    std::shared_ptr<bool> fail_flag() const { return m_fail; }

    bool deof() const { return m_dev.deof(); }
    size_t dget(char_type* s, size_t n) { return m_dev.dget(s, n); }
    template <bool Saturate = false>
    auto get_buf(size_t to_max) { return m_dev.template get_buf<Saturate>(to_max); }
    void get_rollback(size_t len) { m_dev.get_rollback(len); }

    size_t dtell() const
    {
        if (*m_fail) throw IOv2::device_error("throw_tell_device::dtell forced failure");
        return m_dev.dtell();
    }
    size_t dsize() const { return m_dev.dsize(); }
    void dseek(size_t v) { m_dev.dseek(v); }
    void drseek(size_t offset) { m_dev.drseek(offset); }
    void dput(const char_type* ch, size_t n) { m_dev.dput(ch, n); }
    CharT* put_buf(size_t len) { return m_dev.put_buf(len); }
    void put_rollback(size_t len) { m_dev.put_rollback(len); }
    void dflush() {}

private:
    IOv2::mem_device<CharT> m_dev;
    std::shared_ptr<bool> m_fail;
};
}

void test_ostream_tell_wchar_t_1()
{
    dump_info("Test ostream<wchar_t>::tell case 1...");

    auto helper = []<template <typename, typename> class T>()
    {
        file_guard g1("istream_seeks-3.txt");
        T ost1{IOv2::mem_device{L""}};
        T ofs1{IOv2::ofile_device<char>{"istream_seeks-3.txt"},
               IOv2::code_cvt_creator<char, wchar_t>("C")};
        
        auto p1 = ost1.tell();
        auto p2 = ofs1.tell();
        VERIFY( p1 == 0 );
        VERIFY( p2 == 0 );
        
        T ost2{IOv2::mem_device{L"bob_marley:kaya"}};
        VERIFY( ost2.tell() == 0 );
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}

void test_ostream_tell_wchar_t_2()
{
    dump_info("Test ostream<wchar_t>::tell case 2...");

    auto helper = []<template <typename, typename> class T>()
    {
        T ost{IOv2::mem_device{L""}};
        auto pos1 = ost.tell();
        VERIFY(pos1 == 0);

        ost << L"RZA ";
        pos1 = ost.tell();
        VERIFY( pos1 == 4 );

        ost << L"ghost dog: way of the samurai";
        pos1 = ost.tell();
        VERIFY( pos1 == 33 );
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}

// wchar_t counterpart of test_ostream_tell_char_3: a throwing device dtell() drives tell()
// into its catch -> devfailbit, returning an empty optional without throwing (no exception mask).
void test_ostream_tell_wchar_t_3()
{
    dump_info("Test ostream<wchar_t>::tell case 3 (device tell failure)...");

    auto helper = []<template <typename, typename> class T>()
    {
        throw_tell_device<wchar_t> dev{std::wstring(L"abc")};
        auto flag = dev.fail_flag();
        T ost{dev};

        VERIFY( ost.tell() == 0 );
        *flag = true;

        std::optional<size_t> pos = 0;
        bool threw = false;
        try { pos = ost.tell(); }
        catch (...) { threw = true; }
        VERIFY( !threw );
        VERIFY( !pos.has_value() );
        VERIFY( ost.rdstate() & IOv2::ios_defs::devfailbit );

        *flag = false;
    };

    helper.template operator()<IOv2::ostream>();
    helper.template operator()<IOv2::iostream>();

    dump_info("Done\n");
}