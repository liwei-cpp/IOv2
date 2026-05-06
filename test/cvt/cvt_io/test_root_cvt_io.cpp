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

        std::vector<typename T::internal_type> buf;
        cvt_reader<T> reader(obj, buf);
        reader.reset(1024);

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

    auto obj1 = rb_root_cvt{mem_device{"123456"}};
    helper(obj1);

    auto obj2 = no_rb_root_cvt{mem_device{"123456"}};
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

        std::vector<typename T::internal_type> buf;
        cvt_reader<T> reader(obj, buf);
        reader.reset(5);

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

    auto obj1 = rb_root_cvt{mem_device{"0123456789"}};
    helper(obj1);

    auto obj2 = no_rb_root_cvt{mem_device{"0123456789"}};
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
            std::vector<typename T::internal_type> buf;
            cvt_reader<T> reader(obj, buf);
            reader.reset(7);

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

    auto obj1 = rb_root_cvt{mem_device{ref}};
    helper(obj1);

    auto obj2 = no_rb_root_cvt{mem_device{ref}};
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
            std::vector<typename T::internal_type> buf;
            cvt_writer<T> writer(obj, buf);
            writer.reset(1024);

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

    auto obj1 = rb_root_cvt{mem_device{""}};
    helper(obj1);

    auto obj2 = no_rb_root_cvt{mem_device{""}};
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
            std::vector<typename T::internal_type> buf;
            cvt_writer<T> writer(obj, buf);
            writer.reset(1024);

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

    auto obj1 = rb_root_cvt{mem_device{""}};
    helper(obj1);

    auto obj2 = no_rb_root_cvt{mem_device{""}};
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
            std::vector<typename T::internal_type> buf;
            cvt_writer<T> writer(obj, buf);
            writer.reset(10);

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

    auto obj1 = rb_root_cvt{mem_device{""}};
    helper(obj1);

    auto obj2 = no_rb_root_cvt{mem_device{""}};
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
            std::vector<typename T::internal_type> buf;
            cvt_reader<T> reader(obj, buf);
            reader.reset(7);

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

    auto obj1 = rb_root_cvt{snatchy_device<char, 3>{ref}};
    helper(obj1);

    auto obj2 = no_rb_root_cvt{snatchy_device<char, 3>{ref}};
    helper(obj2);

    dump_info("Done\n");
}

// Minimal write-only device for cvt_writer non-mem_device specialization tests.
namespace {
struct str_write_device {
    using char_type = char;
    std::string result;
    void dput(const char_type* s, size_t n) { result.append(s, n); }
    void dflush() {}
};
} // namespace

// cvt_reader<rb_root_cvt<non-mem-device>> error paths:
//   get_buf(0), get_buf(too_large), get_buf<Saturate=true> EOS throw,
//   rollback() success, rollback(0) throw, rollback(too_large) throw.
void test_root_cvt_file_reader_errors_1()
{
    using namespace IOv2;
    dump_info("Test cvt_reader<non-mem-device> error paths...");

    using ObjT = rb_root_cvt<snatchy_device<char, 3>>;

    // get_buf(0) → throw (line 1706)
    {
        ObjT obj{snatchy_device<char, 3>{"hello"}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_reader<ObjT> reader(obj, buf);
        reader.reset(10);
        bool threw = false;
        try { reader.get_buf(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // get_buf(too_large) → throw (line 1708)
    {
        ObjT obj{snatchy_device<char, 3>{"hello"}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_reader<ObjT> reader(obj, buf);
        reader.reset(10);
        bool threw = false;
        try { reader.get_buf(ObjT::s_buffer_length + 1); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // get_buf<Saturate=true> hits EOS before filling → throw (line 1738)
    {
        ObjT obj{snatchy_device<char, 3>{"ab"}}; // only 2 bytes available
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_reader<ObjT> reader(obj, buf);
        reader.reset(10);
        bool threw = false;
        try { reader.get_buf<true>(5); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback() success path (line 1784)
    {
        ObjT obj{snatchy_device<char, 3>{"hello"}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_reader<ObjT> reader(obj, buf);
        reader.reset(5);
        auto [ptr, len] = reader.get_buf(3); // reads "hel" (snatchy max=3)
        VERIFY(len == 3);
        reader.rollback(2); // rolls back 2 → "el" re-exposed
        auto [ptr2, len2] = reader.get_buf(3);
        VERIFY(len2 == 3);
        VERIFY(std::string(ptr2, len2) == "ell");
    }

    // rollback(0) → throw (line 1780-1781)
    {
        ObjT obj{snatchy_device<char, 3>{"hello"}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_reader<ObjT> reader(obj, buf);
        reader.reset(5);
        reader.get_buf(3);
        bool threw = false;
        try { reader.rollback(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback(too_large) → throw (line 1782-1783)
    {
        ObjT obj{snatchy_device<char, 3>{"hello"}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_reader<ObjT> reader(obj, buf);
        reader.reset(5);
        reader.get_buf(3);
        bool threw = false;
        try { reader.rollback(100); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

// cvt_writer<rb_root_cvt<non-mem-device>> error paths:
//   reset(too_large), put_buf(0), put_buf(too_large),
//   rollback(0) throw, rollback(too_large) throw.
void test_root_cvt_file_writer_errors_1()
{
    using namespace IOv2;
    dump_info("Test cvt_writer<non-mem-device> error paths...");

    using ObjT = rb_root_cvt<str_write_device>;

    // reset(too_large) → throw (line 1863)
    {
        ObjT obj{str_write_device{}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_writer<ObjT> writer(obj, buf);
        bool threw = false;
        try { writer.reset(ObjT::s_buffer_length + 1); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // put_buf(0) → throw (line 1895)
    {
        ObjT obj{str_write_device{}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_writer<ObjT> writer(obj, buf);
        writer.reset(10);
        bool threw = false;
        try { writer.put_buf(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // put_buf(too_large) → throw (line 1897)
    {
        ObjT obj{str_write_device{}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_writer<ObjT> writer(obj, buf);
        writer.reset(5);
        bool threw = false;
        try { writer.put_buf(6); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback(0) → throw (line 1944)
    {
        ObjT obj{str_write_device{}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_writer<ObjT> writer(obj, buf);
        writer.reset(10);
        auto ptr = writer.put_buf(5);
        std::copy_n("hello", 5, ptr);
        bool threw = false;
        try { writer.rollback(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback(too_large) → throw (line 1946)
    {
        ObjT obj{str_write_device{}};
        obj.bos(); obj.main_cont_beg();
        std::vector<char> buf;
        cvt_writer<ObjT> writer(obj, buf);
        writer.reset(10);
        auto ptr = writer.put_buf(5);
        std::copy_n("hello", 5, ptr);
        bool threw = false;
        try { writer.rollback(100); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}
