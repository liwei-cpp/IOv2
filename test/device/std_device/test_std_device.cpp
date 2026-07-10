#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <chrono>

#include <device/std_device.h>

#include <common/dump_info.h>
#include <common/stdio_guard.h>
#include <common/verify.h>

void test_std_device_gen_1()
{
    using namespace IOv2;

    dump_info("Test std_device general 1...");
    static_assert(IOv2::io_device<std_device<STDIN_FILENO>>);
    static_assert(IOv2::io_device<std_device<STDOUT_FILENO>>);
    static_assert(IOv2::io_device<std_device<STDERR_FILENO>>);

    static_assert(std::is_same_v<std_device<STDIN_FILENO>::char_type, char>);
    static_assert(std::is_same_v<std_device<STDOUT_FILENO>::char_type, char>);
    static_assert(std::is_same_v<std_device<STDERR_FILENO>::char_type, char>);

    {
        using CheckType = std_device<STDOUT_FILENO>;
        static_assert(!IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(!IOv2::dev_cpt::support_get<CheckType>);
    }

    {
        using CheckType = std_device<STDERR_FILENO>;
        static_assert(!IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(IOv2::dev_cpt::support_put<CheckType>);
        static_assert(!IOv2::dev_cpt::support_get<CheckType>);
    }
    
    {
        using CheckType = std_device<STDIN_FILENO>;
        static_assert(!IOv2::dev_cpt::support_positioning<CheckType>);
        static_assert(!IOv2::dev_cpt::support_put<CheckType>);
        static_assert(IOv2::dev_cpt::support_get<CheckType>);
    }
    dump_info("Done\n");
}

void test_std_device_input_1()
{
    using namespace IOv2;

    dump_info("Test std_device input case 1...");
    
    const char* c_lit = "black pearl jasmine tea";
    
    std_device<STDIN_FILENO> obj;
    
    char buf[5] = {0};
    iguard g(c_lit);
    VERIFY(obj.dget(buf, 1) == 1 && buf[0] == c_lit[0]);
    VERIFY(obj.dget(buf, 1) == 1 && buf[0] == c_lit[1]);

    memset(buf, 'x', 5);
    VERIFY(obj.dget(buf, 5) == 5);
    VERIFY(memcmp(buf, c_lit + 2, 5) == 0);
    VERIFY(obj.dget(buf, 1) == 1 && buf[0] == c_lit[7]);

    dump_info("Done\n");
}

void test_std_device_output_1()
{
    using namespace IOv2;

    dump_info("Test std_device output case 1...");

    {
        oguard<true> g;
        std_device<STDOUT_FILENO> obj;
        VERIFY(g.contents().empty());
        
        obj.dput("a", 1);
        obj.dput("bcdef", 5);
        obj.dflush();
        VERIFY(g.contents() == "abcdef");
    }
    
    {
        oguard<false> g;
        std_device<STDERR_FILENO> obj;
        VERIFY(g.contents().empty());
        
        obj.dput("a", 1);
        obj.dput("bcdef", 5);
        obj.dflush();
        VERIFY(g.contents() == "abcdef");
    }

    dump_info("Done\n");
}

void test_std_device_output_2()
{
    using namespace IOv2;

    dump_info("Test std_device output case 2...");

    const std::string test_file = "out_test.txt";
    {
        oguard<true> g;
        std_device<STDOUT_FILENO> obj;
        VERIFY(g.contents().empty());
        
        obj.dput("a", 1);
        obj.dflush();
        obj.dput("bcdef", 5);
        VERIFY(g.contents()[0] == 'a');
        obj.dflush();
        VERIFY(g.contents() == "abcdef");
    }
    
    {
        oguard<false> g;
        std_device<STDERR_FILENO> obj;
        VERIFY(g.contents().empty());
        
        obj.dput("a", 1);
        obj.dflush();
        obj.dput("bcdef", 5);
        VERIFY(g.contents() == "abcdef");
    }

    dump_info("Done\n");
}
void test_std_device_edge_cases()
{
    using namespace IOv2;

    dump_info("Test std_device edge cases...");

    {
        std_output_device obj;
        // Test dput(nullptr, 0)
        obj.dput(nullptr, 0);
    }

    {
        std_input_device obj;
        // Test dget(nullptr, 0)
        char ch;
        iguard g("abc");
        VERIFY(obj.dget(nullptr, 0) == 0);
        
        // Test dget(nullptr, 1) should throw
        try {
            obj.dget(nullptr, 1);
            throw std::runtime_error("std_device::dget(nullptr, 1) should throw");
        } catch (const device_error&) {}

        VERIFY(obj.dget(&ch, 1) == 1 && ch == 'a');
    }

    dump_info("Done\n");
}

void test_std_device_move() {
    using namespace IOv2;
    dump_info("Test std_device move semantics...");
    
    // Move for stdin
    {
        std_input_device d1;
        // Mock EOF hit
        {
            iguard g(""); // empty input triggers EOF
            char buf;
            d1.dget(&buf, 1);
        }
        VERIFY(d1.deof());
        
        std_input_device d2(std::move(d1));
        VERIFY(d2.deof());
        
        std_input_device d3;
        d3 = std::move(d2);
        VERIFY(d3.deof());
    }

    // Move (construct and assign) must carry the deof()-cached look-ahead byte,
    // otherwise the peeked byte would be silently dropped.
    {
        iguard g("xy");
        std_input_device d1;
        VERIFY(!d1.deof());                  // probes and caches 'x'

        std_input_device d2(std::move(d1));  // move-construct carries the cache
        char buf;
        VERIFY(d2.dget(&buf, 1) == 1 && buf == 'x');

        VERIFY(!d2.deof());                  // probes and caches 'y'
        std_input_device d3;
        d3 = std::move(d2);                  // move-assign carries the cache
        VERIFY(d3.dget(&buf, 1) == 1 && buf == 'y');
        VERIFY(d3.deof());
    }

    // Move for stdout (no state but should work)
    {
        std_output_device d1;
        std_output_device d2(std::move(d1));
        std_output_device d3;
        d3 = std::move(d2);
    }
    
    dump_info("Done\n");
}

void test_std_device_eof() {
    using namespace IOv2;
    dump_info("Test std_device EOF...");

    // deof() must be constructed and probed only against a redirected stdin;
    // probing the real stdin would block or latch a spurious EOF.
    {
        iguard g("a");
        std_input_device d1;
        char buf[2];

        // Data available: deof() probes, reports not-EOF, and caches the byte.
        VERIFY(!d1.deof());
        // The probe-cached byte is delivered in order by the next dget().
        VERIFY(d1.dget(buf, 1) == 1 && buf[0] == 'a');
        // Stream exhausted: deof() probes read()==0 and latches sticky EOF.
        VERIFY(d1.deof());
        VERIFY(d1.dget(buf, 1) == 0);
        // Subsequent calls keep reporting EOF / 0.
        VERIFY(d1.deof());
        VERIFY(d1.dget(buf, 1) == 0);
        VERIFY(d1.deof());
    }

    dump_info("Done\n");
}

void test_std_device_errors() {
    using namespace IOv2;
    dump_info("Test std_device error paths...");
    
    // dput error (stdout)
    {
        oguard<true> g;
        std_output_device d;
        int saved_stdout = dup(STDOUT_FILENO);
        close(STDOUT_FILENO);
        
        try {
            d.dput("test", 4);
        } catch (const device_error&) {
            // Expected
        }
        
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    // dput error (stderr)
    {
        oguard<false> g;
        std_error_device d;
        int saved_stderr = dup(STDERR_FILENO);
        close(STDERR_FILENO);
        
        try {
            d.dput("test", 4);
        } catch (const device_error&) {
            // Expected
        }
        
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }

    // dflush error (stdout)
    {
        oguard<true> g;
        std_output_device d;
        int saved_stdout = dup(STDOUT_FILENO);
        close(STDOUT_FILENO);
        
        try {
            d.dflush();
        } catch (const device_error&) {
            // Expected
        }
        
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    // dflush error (stderr)
    {
        oguard<false> g;
        std_error_device d;
        int saved_stderr = dup(STDERR_FILENO);
        close(STDERR_FILENO);
        
        try {
            d.dflush();
        } catch (const device_error&) {
            // Expected
        }
        
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }

    // dget error
    {
        iguard g("abc");
        std_input_device d;
        int saved_stdin = dup(STDIN_FILENO);
        close(STDIN_FILENO);
        
        try {
            char buf;
            d.dget(&buf, 1);
        } catch (const device_error&) {
            // Expected
        }
        
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    
    dump_info("Done\n");
}

void test_std_device_nonblocking() {
    using namespace IOv2;
    dump_info("Test std_device non-blocking read...");
    
    int pipefds[2];
    if (pipe(pipefds) == -1) return;
    
    int saved_stdin = dup(STDIN_FILENO);
    dup2(pipefds[0], STDIN_FILENO);
    
    // Set non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    std_input_device d;
    char buf[5];
    
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (write(pipefds[1], "hello", 5) != 5) {
            // Ignore error
        }
    });
    
    VERIFY(d.dget(buf, 5) == 5);
    VERIFY(std::memcmp(buf, "hello", 5) == 0);
    
    t.join();
    
    // Test POLLHUP by closing write end
    close(pipefds[1]);
    VERIFY(d.dget(buf, 1) == 0);
    VERIFY(d.deof());
    
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
    close(pipefds[0]);
    
    dump_info("Done\n");
}
