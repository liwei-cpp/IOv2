#include <io/objects/objects.h>
#include <common/dump_info.h>
#include <common/stdio_guard.h>
#include <common/verify.h>

void test_io_objects_wchar_t_1()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 1...");

    {
        oguard<true> g;
        IOv2::wcout << L"testing cout" << IOv2::endl;
        VERIFY(g.contents() == "testing cout\n");
    }

    {
        oguard<false> g;
        IOv2::wcerr << L"testing cerr" << IOv2::endl;
        VERIFY(g.contents() == "testing cerr\n");
        VERIFY( IOv2::cerr.flags() & IOv2::ios_defs::unitbuf );
    }

    {
        oguard<false> g;
        IOv2::wclog << L"testing clog" << IOv2::endl;
        VERIFY(g.contents() == "testing clog\n");
    }

    {
        iguard g("hello");
        wchar_t array1[20] = L"testing istream";
        IOv2::wcin >> array1;
        VERIFY(std::wstring(L"hello") == array1);
        VERIFY( IOv2::wcin.tie() == &IOv2::wcout );
    }

    dump_info("Done\n");
}

void test_io_objects_wchar_t_2()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 2...");

    using namespace IOv2;

    {
        oguard<true> g1;
        oguard<false> g2;
        wcout << L"hello ";
        wcout.flush();
        wcerr << L"fine ";
        wcerr.flush();
        wcout << L"world" << endl;
        wcout.flush();

        auto buf1 = g1.contents();
        auto buf2 = g2.contents();
        VERIFY(buf1 == "hello world\n");
        VERIFY(buf2 == "fine ");
    }

    dump_info("Done\n");
}

void test_io_objects_wchar_t_3()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 3...");

    {
        oguard<true> g1;
        iguard g2("Wei Li");

        IOv2::wcout.reset();
        IOv2::wcin.reset();

        IOv2::wcout << L"hello" << L' ' << L"world" << IOv2::endl;
        IOv2::wcout << L"Enter your name: ";
        VERIFY(g1.contents() == "hello world\n");
        std::wstring s;
        IOv2::wcin >> s;
        VERIFY(g1.contents() == "hello world\nEnter your name: ");
        IOv2::wcout << L"hello " << s << IOv2::endl;
        VERIFY(g1.contents() == "hello world\nEnter your name: hello Wei\n");
    }

    dump_info("Done\n");
}

void test_io_objects_wchar_t_4()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 4...");

    {
        oguard<true> g1;
        iguard g2("\n");

        IOv2::wcout << L"Press ENTER once\n";
        VERIFY((bool)(IOv2::wcin.ignore(1)));
    }

    dump_info("Done\n");
}

void test_io_objects_wchar_t_5()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 5...");

    void* p1 = &IOv2::wcout;
    void* p2 = &IOv2::wcin;
    void* p3 = &IOv2::wcerr;
    void* p4 = &IOv2::wclog;
    IOv2::sync_with_stdio(false); 
    void* p1s = &IOv2::wcout;
    void* p2s = &IOv2::wcin;
    void* p3s = &IOv2::wcerr;
    void* p4s = &IOv2::wclog;
    VERIFY( p1 == p1s );
    VERIFY( p2 == p2s );
    VERIFY( p3 == p3s );
    VERIFY( p4 == p4s );

    dump_info("Done\n");
}

void test_io_objects_wchar_t_6()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 6...");

    {
        iguard g("hello world");
        wchar_t c1 = 0;
        wchar_t c2 = 1;
        IOv2::wcin.get(c1);
        IOv2::wcin.putback(c1);
        IOv2::wcin.get(c2);
        VERIFY( IOv2::wcin.good() );
        VERIFY( c1 == c2 );
    }

    {
        iguard g("hello world");
        wchar_t buf[2];
        VERIFY( IOv2::wcin.get<IOv2::keep_sep, IOv2::no_zt>(buf, 2) - buf == 2 );
        IOv2::wcin.putback(buf[1]);
        auto c = IOv2::wcin.get();
        VERIFY( c == buf[1] );
    }

    dump_info("Done\n");
}

void test_io_objects_wchar_t_7()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 7...");

    VERIFY( IOv2::wcerr.flags() & IOv2::ios_defs::dec );
    VERIFY( IOv2::wcerr.flags() & IOv2::ios_defs::skipws );
    VERIFY( IOv2::wcerr.flags() & IOv2::ios_defs::unitbuf );
    VERIFY( IOv2::wcerr.tie() == &IOv2::wcout );

    dump_info("Done\n");
}

void test_io_objects_wchar_t_8()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 8...");

    {
        iguard g("\xe8\xaf\xb7 \xd0\xbb\xd0\xbb");

        // use attach to refresh the whole buffer.
        IOv2::wcin.reset();

        std::wstring s1;
        std::wstring s2;

        IOv2::wcin.switch_code("zh_CN.UTF-8");
        IOv2::wcin >> s1;
        VERIFY(IOv2::wcin.code() == "zh_CN.UTF-8");

        IOv2::wcin.switch_code("zh_CN.GBK");
        IOv2::wcin >> s2;
        VERIFY(IOv2::wcin.code() == "zh_CN.GBK");

        VERIFY(s1 == L"请");
        VERIFY(s2 == L"谢谢");
    }

    dump_info("Done\n");
}

void test_io_objects_wchar_t_9()
{
    dump_info("Test io objects (wcout / wcerr / wclog / wcin) case 9...");

    {
        oguard<true> g1;
        IOv2::wcout.switch_code("zh_CN.UTF-8");
        IOv2::wcout << L"请 ";
        VERIFY(IOv2::wcout.code() == "zh_CN.UTF-8");
        IOv2::wcout.switch_code("zh_CN.GBK");
        IOv2::wcout << L"谢谢" << IOv2::flush;
        VERIFY(IOv2::wcout.code() == "zh_CN.GBK");

        VERIFY(g1.contents() == "\xe8\xaf\xb7 \xd0\xbb\xd0\xbb");
    }

    dump_info("Done\n");
}