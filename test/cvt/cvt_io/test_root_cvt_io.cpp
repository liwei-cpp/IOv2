#include <typeinfo>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <device/mem_device.h>

#include <common/dump_info.h>
#include <common/snatchy_device.h>
#include <common/verify.h>

void test_root_cvt_mem_input_1()
{
    using namespace IOv2;
    dump_info("Test cvt_reader with root_cvt<mem_device> case 1...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();

        cvt_io<T> io;
        auto reader = io.reader(obj, 1024);

        auto [ptr, len] = reader.get_buf(3);
        VERIFY(len == 3);
        VERIFY(std::string(ptr, ptr + len) == "123");

        std::tie(ptr, len) = reader.get_buf(2);
        VERIFY(len == 2);
        VERIFY(std::string(ptr, ptr + len) == "45");

        reader.rollback(2);
        std::tie(ptr, len) = reader.get_buf(5);
        VERIFY(len == 3);
        VERIFY(std::string(ptr, ptr + len) == "456");
    };

    auto obj1 = make_root_cvt<true>(mem_device{"123456"});
    helper(obj1);

    auto obj2 = make_root_cvt<false>(mem_device{"123456"});
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_input_2()
{
    using namespace IOv2;
    dump_info("Test cvt_reader with root_cvt<mem_device> case 2...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();

        cvt_io<T> io;
        auto reader = io.reader(obj, 5);

        auto [ptr, len] = reader.get_buf(5);
        VERIFY(len == 5);
        VERIFY(std::string(ptr, ptr + len) == "01234");

        reader.rollback(1);
        std::tie(ptr, len) = reader.get_buf(2);
        VERIFY(len == 2);
        VERIFY(std::string(ptr, ptr + len) == "45");

        reader.rollback(2);
        std::tie(ptr, len) = reader.get_buf(5);
        VERIFY(len == 5);
        VERIFY(std::string(ptr, ptr + len) == "45678");
    };

    auto obj1 = make_root_cvt<true>(mem_device{"0123456789"});
    helper(obj1);

    auto obj2 = make_root_cvt<false>(mem_device{"0123456789"});
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_input_3()
{
    using namespace IOv2;
    dump_info("Test cvt_reader with root_cvt<mem_device> case 3...");

    std::string ref; ref.reserve(1024);
    for (size_t i = 0; i < 1024; ++i)
        ref += 'a' + (i % 26);

    auto helper = [&ref]<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();

        std::string res;
        {
            cvt_io<T> io;
            auto reader = io.reader(obj, 7);

            size_t cur = 0;
            size_t get_len = 1;
            while (cur < 1024)
            {
                auto [ptr, len] = reader.get_buf(std::min(1024 - cur, get_len));
                std::copy(ref.data() + cur, ref.data() + cur + len, std::back_inserter(res));
                cur += len;
                get_len = (get_len % 7) + 1;
            }
            VERIFY(cur == 1024);
        }

        VERIFY(res == ref);
    };

    auto obj1 = make_root_cvt<true>(mem_device{ref});
    helper(obj1);

    auto obj2 = make_root_cvt<false>(mem_device{ref});
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_output_1()
{
    using namespace IOv2;
    dump_info("Test cvt_writer with root_cvt<mem_device> case 1...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();

        {
            cvt_io<T> io;
            auto writer = io.writer(obj, 1024);

            auto ptr = writer.put_buf(3);
            std::string str = "123";
            std::copy(str.begin(), str.end(), ptr);

            ptr = writer.put_buf(2);
            str = "45";
            std::copy(str.begin(), str.end(), ptr);

            ptr = writer.put_buf(2);
            str = "67";
            std::copy(str.begin(), str.end(), ptr);
            writer.commit();
        }

        VERIFY(obj.detach().str() == "1234567");
    };

    auto obj1 = make_root_cvt<true>(mem_device{""});
    helper(obj1);

    auto obj2 = make_root_cvt<false>(mem_device{""});
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_output_2()
{
    using namespace IOv2;
    dump_info("Test cvt_writer with root_cvt<mem_device> case 2...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();

        {
            cvt_io<T> io;
            auto writer = io.writer(obj, 1024);

            auto ptr = writer.put_buf(5);
            std::string str = "123";
            std::copy(str.begin(), str.end(), ptr);
            writer.rollback(2);

            ptr = writer.put_buf(3);
            str = "45";
            std::copy(str.begin(), str.end(), ptr);
            writer.rollback(1);

            ptr = writer.put_buf(3);
            str = "67";
            std::copy(str.begin(), str.end(), ptr);
            writer.rollback(1);
            writer.commit();
        }

        VERIFY(obj.detach().str() == "1234567");
    };

    auto obj1 = make_root_cvt<true>(mem_device{""});
    helper(obj1);

    auto obj2 = make_root_cvt<false>(mem_device{""});
    helper(obj2);


    dump_info("Done\n");
}

void test_root_cvt_mem_output_3()
{
    using namespace IOv2;
    dump_info("Test cvt_writer with root_cvt<mem_device> case 3...");

    auto helper = []<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();

        std::string ref; ref.reserve(1024);
        for (size_t i = 0; i < 1024; ++i)
            ref += 'a' + (i % 26);

        {
            cvt_io<T> io;
            auto writer = io.writer(obj, 10);

            size_t cur = 0;
            size_t get_len = 1;
            while (cur < 1024)
            {
                auto aim_len = std::min(1024 - cur, get_len);
                auto ptr = writer.put_buf(aim_len);
                std::copy(ref.data() + cur, ref.data() + cur + aim_len, ptr);
                cur += aim_len;
                get_len = (get_len % 10) + 1;
            }
            VERIFY(cur == 1024);
            writer.commit();
        }

        VERIFY(obj.detach().str() == ref);
    };

    auto obj1 = make_root_cvt<true>(mem_device{""});
    helper(obj1);

    auto obj2 = make_root_cvt<false>(mem_device{""});
    helper(obj2);

    dump_info("Done\n");
}

void test_root_cvt_mem_saturate_input_1()
{
    using namespace IOv2;
    dump_info("Test cvt_reader with root_cvt (saturate get) case 1...");

    std::string ref; ref.reserve(1024);
    for (size_t i = 0; i < 1024; ++i)
        ref += 'a' + (i % 26);

    auto helper = [&ref]<typename T>(T& obj)
    {
        obj.bos(); obj.main_cont_beg();

        std::string res;
        {
            cvt_io<T> io;
            auto reader = io.reader(obj, 7);

            size_t cur = 0;
            size_t get_len = 1;
            while (cur < 1024)
            {
                const size_t aim_len = std::min(1024 - cur, get_len);
                auto ptr = reader.template get_buf<true>(aim_len);
                std::copy(ptr, ptr + aim_len, std::back_inserter(res));
                cur += aim_len;
                get_len = (get_len % 7) + 1;
            }
            VERIFY(cur == 1024);
        }

        VERIFY(res == ref);
    };

    auto obj1 = make_root_cvt<true>(snatchy_device<char, 3>{ref});
    helper(obj1);

    auto obj2 = make_root_cvt<false>(snatchy_device<char, 3>{ref});
    helper(obj2);

    dump_info("Done\n");
}