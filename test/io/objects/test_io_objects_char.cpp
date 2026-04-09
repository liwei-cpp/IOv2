#include <io/objects/objects.h>
#include <common/dump_info.h>
#include <common/stdio_guard.h>
#include <common/verify.h>

void test_io_objects_char_1()
{
    dump_info("Test io objects (cout / cerr / clog / cin) case 1...");

    {
        oguard<true> g;
        IOv2::cout << "testing cout" << IOv2::endl;
        VERIFY(g.contents() == "testing cout\n");
    }

    {
        oguard<false> g;
        IOv2::cerr << "testing cerr" << IOv2::endl;
        VERIFY(g.contents() == "testing cerr\n");
        VERIFY( IOv2::cerr.flags() & IOv2::ios_defs::unitbuf );
    }

    {
        oguard<false> g;
        IOv2::clog << "testing clog" << IOv2::endl;
        VERIFY(g.contents() == "testing clog\n");
    }

    {
        iguard g("hello");
        char array1[20] = "testing istream";
        IOv2::cin >> array1;
        VERIFY(std::string("hello") == array1);
        VERIFY( IOv2::cin.tie() == &IOv2::cout );
    }

    dump_info("Done\n");
}

void test_io_objects_char_2()
{
    dump_info("Test io objects (cout / cerr / clog / cin) case 2...");

    using namespace IOv2;

    {
        oguard<true> g1;
        oguard<false> g2;
        cout << "hello ";
        cout.flush();
        cerr << "fine ";
        cerr.flush();
        cout << "world" << endl;
        cout.flush();

        auto buf1 = g1.contents();
        auto buf2 = g2.contents();
        VERIFY(buf1 == "hello world\n");
        VERIFY(buf2 == "fine ");
    }

    dump_info("Done\n");
}

void test_io_objects_char_3()
{
    dump_info("Test io objects (cout / cerr / clog / cin) case 3...");

    {
        IOv2::cout.reset();
        IOv2::cin.reset();

        IOv2::cout.sync_with_stdio(false);
        oguard<true> g1;
        iguard g2("Wei Li");

        IOv2::cout << "hello" << ' ' << "world" << IOv2::endl;
        IOv2::cout << "Enter your name: ";
        VERIFY(g1.contents() == "hello world\n");
        std::string s;
        IOv2::cin >> s;
        VERIFY(g1.contents() == "hello world\nEnter your name: ");
        IOv2::cout << "hello " << s << IOv2::endl;
        VERIFY(g1.contents() == "hello world\nEnter your name: hello Wei\n");
    }

    dump_info("Done\n");
}

void test_io_objects_char_4()
{
    dump_info("Test io objects (cout / cerr / clog / cin) case 4...");

    {
        oguard<true> g1;
        iguard g2("\n");

        IOv2::cout << "Press ENTER once\n";
        VERIFY((bool)(IOv2::cin.ignore(1)));
    }

    dump_info("Done\n");
}

void test_io_objects_char_5()
{
    dump_info("Test io objects (cout / cerr / clog / cin) case 5...");

    void* p1 = &IOv2::cout;
    void* p2 = &IOv2::cin;
    void* p3 = &IOv2::cerr;
    void* p4 = &IOv2::clog;
    IOv2::sync_with_stdio(false); 
    void* p1s = &IOv2::cout;
    void* p2s = &IOv2::cin;
    void* p3s = &IOv2::cerr;
    void* p4s = &IOv2::clog;
    VERIFY( p1 == p1s );
    VERIFY( p2 == p2s );
    VERIFY( p3 == p3s );
    VERIFY( p4 == p4s );

    dump_info("Done\n");
}

void test_io_objects_char_6()
{
    dump_info("Test io objects (cout / cerr / clog / cin) case 6...");

    {
        iguard g("hello world");
        char c1 = 0;
        char c2 = 1;
        IOv2::cin.get(c1);
        IOv2::cin.putback(c1);
        IOv2::cin.get(c2);
        VERIFY( IOv2::cin.good() );
        VERIFY( c1 == c2 );
    }

    {
        iguard g("hello world");
        char buf[2];
        VERIFY( IOv2::cin.get<IOv2::keep_sep, IOv2::no_zt>(buf, 2) - buf == 2 );
        IOv2::cin.putback(buf[1]);
        auto c = IOv2::cin.get();
        VERIFY( c == buf[1] );
    }

    dump_info("Done\n");
}

void test_io_objects_char_7()
{
    dump_info("Test io objects (cout / cerr / clog / cin) case 7...");

    VERIFY( IOv2::cerr.flags() & IOv2::ios_defs::dec );
    VERIFY( IOv2::cerr.flags() & IOv2::ios_defs::skipws );
    VERIFY( IOv2::cerr.flags() & IOv2::ios_defs::unitbuf );
    VERIFY( IOv2::cerr.tie() == &IOv2::cout );

    dump_info("Done\n");
}