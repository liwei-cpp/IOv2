#include <typeinfo>
#include <cvt/root_cvt.h>
#include <cvt/runtime_cvt.h>
#include <cvt/crypt/vigenere_cvt.h>
#include <device/mem_device.h>

#include <support/dump_info.h>
#include <support/snatchy_device.h>
#include <support/verify.h>

namespace {

// Converter whose put_main always throws — used to exercise abs_cvt's
// catch block (m_is_tainted = true) and assert_not_tainted() throw.
template <IOv2::io_converter KernelType>
struct failing_put_cvt
    : public IOv2::abs_cvt<failing_put_cvt<KernelType>, KernelType,
                           typename KernelType::internal_type>
{
    using IT = typename KernelType::internal_type;
    using BT = IOv2::abs_cvt<failing_put_cvt<KernelType>, KernelType, IT>;
    friend BT;
public:
    using device_type   = typename KernelType::device_type;
    using internal_type = IT;
    using external_type = IT;
    explicit failing_put_cvt(KernelType k) : BT(std::move(k)) {}
    // get_main is only invoked after assert_not_tainted() passes; the stub
    // returning 0 is never actually reached in the tainted-state test.
    size_t get_main(IOv2::cvt_reader<KernelType>&, IT*, size_t) { return 0; }
    void put_main(IOv2::cvt_writer<KernelType>&, const IT*, size_t)
    {
        throw IOv2::cvt_error("failing_put_cvt: simulated failure");
    }
};

// Exposes force_neutral_status() so a test can set m_io_status back to
// neutral after main_cont_beg(), triggering the second guard in bos().
template <IOv2::io_converter KernelType>
struct bos_guard_hack_cvt
    : public IOv2::abs_cvt<bos_guard_hack_cvt<KernelType>, KernelType,
                           typename KernelType::internal_type>
{
    using IT = typename KernelType::internal_type;
    using BT = IOv2::abs_cvt<bos_guard_hack_cvt<KernelType>, KernelType, IT>;
    friend BT;
public:
    using device_type   = typename KernelType::device_type;
    using internal_type = IT;
    using external_type = IT;
    explicit bos_guard_hack_cvt(KernelType k) : BT(std::move(k)) {}
    void force_neutral_status() { this->m_io_status = IOv2::io_status::neutral; }
    size_t get_main(IOv2::cvt_reader<KernelType>&, IT*, size_t) { return 0; }
    void put_main(IOv2::cvt_writer<KernelType>&, const IT*, size_t) {}
};

// Minimal CRTP cvt: exposes `char` as internal_type while the kernel uses
// `wchar_t` units. Lets us exercise the BOS partial-unit code path in
// abs_cvt::get / abs_cvt::put (triggered when to_max * sizeof(char) is not
// a multiple of sizeof(wchar_t)).
template <IOv2::io_converter KernelType>
    requires std::is_same_v<typename KernelType::internal_type, wchar_t>
struct wext_char_cvt
    : public IOv2::abs_cvt<wext_char_cvt<KernelType>, KernelType, char,
                           /*position=*/false, /*io_switch=*/false>
{
    using BT = IOv2::abs_cvt<wext_char_cvt<KernelType>, KernelType, char,
                              false, false>;
    friend BT;
public:
    using device_type  = typename KernelType::device_type;
    using internal_type = char;
    using external_type = wchar_t;
    explicit wext_char_cvt(KernelType k) : BT(std::move(k)) {}

    size_t get_main(IOv2::cvt_reader<KernelType>&, char*, size_t) { return 0; }
    void   put_main(IOv2::cvt_writer<KernelType>&, const char*, size_t) {}
};

// Minimal CRTP cvt with io-direction switching disabled (default_io_switch=false).
// Used to exercise the "cannot switch to input/output mode" error paths in
// abs_cvt::get / abs_cvt::put.
template <IOv2::io_converter KernelType>
struct no_switch_cvt
    : public IOv2::abs_cvt<no_switch_cvt<KernelType>, KernelType,
                           typename KernelType::internal_type,
                           /*position=*/false, /*io_switch=*/false>
{
    using IT = typename KernelType::internal_type;
    using BT = IOv2::abs_cvt<no_switch_cvt<KernelType>, KernelType, IT,
                              false, false>;
    friend BT;
public:
    using device_type  = typename KernelType::device_type;
    using internal_type = IT;
    using external_type = IT;
    explicit no_switch_cvt(KernelType k) : BT(std::move(k)) {}

    size_t get_main(IOv2::cvt_reader<KernelType>& r, IT* to, size_t n)
    {
        auto [ptr, len] = r.get_buf(n);
        std::copy(ptr, ptr + len, to);
        return len;
    }
    void put_main(IOv2::cvt_writer<KernelType>& w, const IT* from, size_t n)
    {
        auto* ptr = w.put_buf(n);
        std::copy(from, from + n, ptr);
        w.commit();
    }
};

} // namespace

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

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == "1234567");
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

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == "1234567");
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

        auto [dev, err] = obj.detach();
        VERIFY(dev.str() == ref);
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

// Tests the base cvt_reader<> template (not the root_cvt specializations).
// Uses vigenere_cvt as kernel, which is an io_converter but not a root_cvt,
// so cvt_reader<vigenere_cvt<...>> instantiates the abs_cvt.h base template.
void test_abs_cvt_base_reader_errors_1()
{
    using namespace IOv2;
    using namespace IOv2::Crypt::Classic;
    dump_info("Test abs_cvt base cvt_reader error/uncovered paths...");

    using KernelT = vigenere_cvt<rb_root_cvt<mem_device<char>>>;

    auto make_input_kernel = [](const char* data) {
        KernelT k{rb_root_cvt<mem_device<char>>{mem_device<char>{data}}, "key"};
        k.bos(); k.main_cont_beg();
        return k;
    };

    // get_buf(0) → throw
    {
        auto k = make_input_kernel("hello world");
        std::vector<char> buf;
        cvt_reader<KernelT> reader(k, buf);
        reader.reset(10);
        bool threw = false;
        try { reader.get_buf(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // get_buf(too_large) → throw
    {
        auto k = make_input_kernel("hello world");
        std::vector<char> buf;
        cvt_reader<KernelT> reader(k, buf);
        reader.reset(5);
        bool threw = false;
        try { reader.get_buf(6); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback() success + early-return (Saturate=false): read, roll back, re-read
    {
        auto k = make_input_kernel("hello world");
        std::vector<char> buf;
        cvt_reader<KernelT> reader(k, buf);
        reader.reset(10);
        auto [ptr, len] = reader.get_buf(5);
        VERIFY(len == 5);
        reader.rollback(3);  // success path — covers rollback body
        auto [ptr2, len2] = reader.get_buf(2);  // 2 <= 3 buffered → early-return
        VERIFY(len2 == 2);
    }

    // early-return with Saturate=true
    {
        auto k = make_input_kernel("hello world");
        std::vector<char> buf;
        cvt_reader<KernelT> reader(k, buf);
        reader.reset(10);
        reader.get_buf(5);
        reader.rollback(3);
        const char* ptr = reader.get_buf<true>(2);  // 2 <= 3 buffered → early-return
        (void)ptr;
    }

    // rollback(0) → throw
    {
        auto k = make_input_kernel("hello world");
        std::vector<char> buf;
        cvt_reader<KernelT> reader(k, buf);
        reader.reset(10);
        reader.get_buf(5);
        bool threw = false;
        try { reader.rollback(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback(too_large) → throw
    {
        auto k = make_input_kernel("hello world");
        std::vector<char> buf;
        cvt_reader<KernelT> reader(k, buf);
        reader.reset(10);
        reader.get_buf(5);
        bool threw = false;
        try { reader.rollback(100); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // get_buf<true> EOS → throw  (only 2 bytes available, need 5)
    {
        auto k = make_input_kernel("ab");
        std::vector<char> buf;
        cvt_reader<KernelT> reader(k, buf);
        reader.reset(10);
        bool threw = false;
        try { reader.get_buf<true>(5); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

// Tests the base cvt_writer<> template error paths.
void test_abs_cvt_base_writer_errors_1()
{
    using namespace IOv2;
    using namespace IOv2::Crypt::Classic;
    dump_info("Test abs_cvt base cvt_writer error paths...");

    using KernelT = vigenere_cvt<rb_root_cvt<mem_device<char>>>;

    auto make_output_kernel = []() {
        KernelT k{rb_root_cvt<mem_device<char>>{mem_device<char>{""}}, "key"};
        k.bos(); k.main_cont_beg();
        return k;
    };

    // put_buf(0) → throw
    {
        auto k = make_output_kernel();
        std::vector<char> buf;
        cvt_writer<KernelT> writer(k, buf);
        writer.reset(10);
        bool threw = false;
        try { writer.put_buf(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // put_buf(too_large) → throw
    {
        auto k = make_output_kernel();
        std::vector<char> buf;
        cvt_writer<KernelT> writer(k, buf);
        writer.reset(5);
        bool threw = false;
        try { writer.put_buf(6); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback(0) → throw
    {
        auto k = make_output_kernel();
        std::vector<char> buf;
        cvt_writer<KernelT> writer(k, buf);
        writer.reset(10);
        writer.put_buf(5);
        bool threw = false;
        try { writer.rollback(0); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // rollback(too_large) → throw
    {
        auto k = make_output_kernel();
        std::vector<char> buf;
        cvt_writer<KernelT> writer(k, buf);
        writer.reset(10);
        writer.put_buf(5);
        bool threw = false;
        try { writer.rollback(100); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

// Tests abs_cvt::retrieve(), bos() error path, switch_to_get(), switch_to_put(),
// and the "cannot switch" error paths in abs_cvt::get/put.
void test_abs_cvt_optional_methods_1()
{
    using namespace IOv2;
    using namespace IOv2::Crypt::Classic;
    dump_info("Test abs_cvt optional methods and error paths...");

    using VigKernelT = rb_root_cvt<mem_device<char>>;
    using VigCvtT    = vigenere_cvt<VigKernelT>;

    // retrieve() — covers abs_cvt::retrieve
    {
        VigCvtT k{VigKernelT{mem_device<char>{"hello"}}, "key"};
        cvt_status s;
        k.retrieve(s);
    }

    // bos() with non-neutral m_io_status → throw
    {
        VigCvtT k{VigKernelT{mem_device<char>{"hello"}}, "key"};
        k.bos();   // sets m_io_status = input
        bool threw = false;
        try { k.bos(); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // switch_to_put() and switch_to_get() — covers abs_cvt::switch_to_put/get
    {
        VigCvtT k{VigKernelT{mem_device<char>{"hello"}}, "key"};
        k.bos(); k.main_cont_beg();    // m_io_status = input
        k.switch_to_put();             // covers abs_cvt::switch_to_put
        k.switch_to_get();             // covers abs_cvt::switch_to_get
    }

    // "cannot switch to input mode" — no-io-switch cvt in output mode calls get()
    {
        using NSKernel = rb_root_cvt<mem_device<char>>;
        using NSCvt    = no_switch_cvt<NSKernel>;
        NSCvt cvt{NSKernel{mem_device<char>{""}}};   // empty → bos returns output
        cvt.bos(); cvt.main_cont_beg();               // m_io_status = output
        char buf[1];
        bool threw = false;
        try { cvt.get(buf, 1); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    // "cannot switch to output mode" — no-io-switch cvt in input mode calls put()
    {
        using NSKernel = rb_root_cvt<mem_device<char>>;
        using NSCvt    = no_switch_cvt<NSKernel>;
        NSCvt cvt{NSKernel{mem_device<char>{"hello"}}};  // non-empty → bos returns input
        cvt.bos(); cvt.main_cont_beg();                   // m_io_status = input
        const char src[1] = {'x'};
        bool threw = false;
        try { cvt.put(src, 1); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

// Tests the BOS partial-unit path in abs_cvt::get and abs_cvt::put.
// Triggered when to_max * sizeof(internal_type) is not a multiple of
// sizeof(external_type). Uses wext_char_cvt (ext=wchar_t 4B, int=char 1B)
// so that to_max=3 leaves a 3-byte remainder in a 4-byte wchar_t unit.
void test_abs_cvt_bos_partial_1()
{
    using namespace IOv2;
    dump_info("Test abs_cvt BOS partial-unit path...");

    using WKernelT = rb_root_cvt<mem_device<wchar_t>>;
    using WCvtT    = wext_char_cvt<WKernelT>;

    // BOS get: kernel has data, read 3 chars (3 % sizeof(wchar_t) != 0)
    {
        WCvtT cvt{WKernelT{mem_device<wchar_t>{L"\x01020304"}}};
        VERIFY(cvt.bos() == io_status::input);
        char buf[4] = {};
        auto n = cvt.get(buf, 3);   // BOS phase, ext_size=4, partial path
        VERIFY(n == 3);
    }

    // BOS put: empty kernel, write 3 chars (3 % sizeof(wchar_t) != 0)
    {
        WCvtT cvt{WKernelT{mem_device<wchar_t>{L""}}};
        VERIFY(cvt.bos() == io_status::output);
        const char src[3] = {'a', 'b', 'c'};
        cvt.put(src, 3);   // BOS phase, ext_size=4, partial path
    }

    dump_info("Done\n");
}

// Tests that a put_main exception sets m_is_tainted = true, and that
// subsequent get()/put() calls fail via assert_not_tainted() (line 1501).
void test_abs_cvt_tainted_state_1()
{
    using namespace IOv2;
    dump_info("Test abs_cvt tainted state (put_main throws → taint → assert_not_tainted)...");

    using KernelT = rb_root_cvt<mem_device<char>>;
    using CvtT    = failing_put_cvt<KernelT>;

    {
        CvtT cvt{KernelT{mem_device<char>{""}}};  // empty → bos returns output
        cvt.bos(); cvt.main_cont_beg();

        // put() enters the try block, put_main throws → catch sets m_is_tainted
        bool threw = false;
        const char data[] = {'x'};
        try { cvt.put(data, 1); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);

        // get() on tainted converter → assert_not_tainted() throws
        threw = false;
        char buf[1];
        try { cvt.get(buf, 1); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}

// Tests the second guard in bos(): if m_is_bos_done is already true while
// m_io_status happens to be neutral, bos() must refuse the second call.
// This state is impossible via the public API alone, so we use the protected
// member exposed by bos_guard_hack_cvt.
void test_abs_cvt_bos_second_guard_1()
{
    using namespace IOv2;
    dump_info("Test abs_cvt bos() second guard (m_is_bos_done)...");

    using KernelT = rb_root_cvt<mem_device<char>>;
    using CvtT    = bos_guard_hack_cvt<KernelT>;

    {
        CvtT cvt{KernelT{mem_device<char>{""}}};
        cvt.bos();            // m_io_status = output (empty device)
        cvt.main_cont_beg();  // m_is_bos_done = true

        // Force m_io_status back to neutral while m_is_bos_done stays true
        cvt.force_neutral_status();

        // bos(): first check passes (neutral), second check fires (m_is_bos_done)
        bool threw = false;
        try { cvt.bos(); } catch (const cvt_error&) { threw = true; }
        VERIFY(threw);
    }

    dump_info("Done\n");
}
