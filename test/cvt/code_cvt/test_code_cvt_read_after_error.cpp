#include <IOv2/common/defs.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/file_device.h>
#include <IOv2/device/mem_device.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <clocale>
#include <cstddef>
#include <cwchar>
#include <exception>
#include <string>
#include <utility>

#include <locale.h>

using namespace IOv2;

namespace
{
    // Each root reads a different way: straight out of the mem_device, out of
    // rb_root_cvt's own buffer, or through abs_cvt's read area (no_rb_root_cvt over
    // a file). The bytes a failed get() had already taken must survive in all three.
    template <typename T>
    void expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::u32string buf(16, U'#');
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
        EXPECT_EQ(buf.substr(0, 2), U"ab");

        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);

        EXPECT_EQ(obj.get(buf.data(), buf.size()), 5u);
        EXPECT_EQ(buf.substr(0, 5), U"cd ef");
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 0u);
    }

    // The lead byte is held across the two fetches of one get(); the byte that
    // proves it wrong is the start of the next character, not part of the error.
    template <typename T>
    void expect_a_held_lead_byte_is_all_an_error_drops(T& obj)
    {
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::u32string buf(16, U'#');
        EXPECT_EQ(obj.get(buf.data(), 2), 1u);
        EXPECT_EQ(buf[0], U'x');

        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);

        EXPECT_EQ(obj.get(buf.data(), buf.size()), 3u);
        EXPECT_EQ(buf.substr(0, 3), U"a b");
    }

    const std::string kBadByte("ab\xff" "cd ef");
    const std::string kHeldLead("x\xe4" "a b");

    using RODev = basic_file_device<true, false, char>;
}

TEST(CodeCvtReadAfterError, BlockReadOverAMemDevice)
{
    code_cvt<no_rb_root_cvt<mem_device<char>>, char32_t> obj{no_rb_root_cvt{mem_device(kBadByte)}, "zh_CN.UTF-8"};
    expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(obj);
}

TEST(CodeCvtReadAfterError, BlockReadOverARootBuffer)
{
    file_guard g("code_cvt_read_after_error", kBadByte);
    code_cvt<rb_root_cvt<RODev>, char32_t> obj{rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(obj);
}

TEST(CodeCvtReadAfterError, BlockReadOverTheReadArea)
{
    file_guard g("code_cvt_read_after_error", kBadByte);
    code_cvt<no_rb_root_cvt<RODev>, char32_t> obj{no_rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    expect_a_block_read_delivers_up_to_the_bad_byte_and_resumes_after_it(obj);
}

TEST(CodeCvtReadAfterError, TheReadAreaSurvivesAMove)
{
    file_guard g("code_cvt_read_after_error", kBadByte);
    code_cvt<no_rb_root_cvt<RODev>, char32_t> obj{no_rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    std::u32string buf(16, U'#');
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);

    auto moved = std::move(obj);
    EXPECT_THROW(moved.get(buf.data(), buf.size()), cvt_error);
    EXPECT_EQ(moved.get(buf.data(), buf.size()), 5u);
    EXPECT_EQ(buf.substr(0, 5), U"cd ef");
}

TEST(CodeCvtReadAfterError, HeldLeadByteOverAMemDevice)
{
    code_cvt<rb_root_cvt<mem_device<char>>, char32_t> obj{rb_root_cvt{mem_device(kHeldLead)}, "zh_CN.UTF-8"};
    expect_a_held_lead_byte_is_all_an_error_drops(obj);
}

TEST(CodeCvtReadAfterError, HeldLeadByteOverTheReadArea)
{
    file_guard g("code_cvt_read_after_error", kHeldLead);
    code_cvt<no_rb_root_cvt<RODev>, char32_t> obj{no_rb_root_cvt{RODev("code_cvt_read_after_error")}, "zh_CN.UTF-8"};
    expect_a_held_lead_byte_is_all_an_error_drops(obj);
}

// A deferred error is still to come, so the stream is not at its end: switching a
// variable-length encoding to writing is refused until a get has reported it.
TEST(CodeCvtReadAfterError, APendingErrorKeepsTheStreamFromEnding)
{
    code_cvt<rb_root_cvt<mem_device<char>>, char32_t> obj{rb_root_cvt{mem_device(std::string("ab\xff"))}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    std::u32string buf(16, U'#');
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
    EXPECT_FALSE(obj.is_eof());
    EXPECT_THROW(obj.switch_to_put(), cvt_error);

    EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
    EXPECT_TRUE(obj.is_eof());
    EXPECT_NO_THROW(obj.switch_to_put());
}

// Half a character held at EOF is still to be reported as well.
TEST(CodeCvtReadAfterError, HeldBytesAtEofKeepTheStreamFromEnding)
{
    code_cvt<rb_root_cvt<mem_device<char>>, char32_t> obj{rb_root_cvt{mem_device(std::string("ab\xe6"))}, "zh_CN.UTF-8"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    std::u32string buf(16, U'#');
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
    EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
    EXPECT_FALSE(obj.is_eof());
    EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
}

namespace
{
    // glibc's "C" is a fixed-length encoding that rejects bytes above 0x7F.
    bool c_locale_rejects_ff()
    {
        locale_t c = newlocale(LC_ALL_MASK, "C", nullptr);
        locale_t old = uselocale(c);
        std::mbstate_t st{};
        wchar_t w = 0;
        const char b = '\xff';
        const bool rejects = std::mbrtowc(&w, &b, 1, &st) == static_cast<std::size_t>(-1);
        uselocale(old);
        freelocale(c);
        return rejects;
    }
}

// A fixed-length encoding may switch halfway through reading: it goes back to where
// the caller stopped (dropping the read-ahead and the pending error), and the write
// starts there -- over the bad byte. Reading on finds nothing stale.
TEST(CodeCvtReadAfterError, AFixedLengthSwitchWritesWhereReadingStopped)
{
    if (!c_locale_rejects_ff())
        GTEST_SKIP() << "the C locale here accepts 0xFF, so there is no decode error to defer";

    code_cvt<rb_root_cvt<mem_device<char>>, char32_t> obj{rb_root_cvt{mem_device(std::string("ab\xff" "cd"))}, "C"};
    EXPECT_EQ(obj.bos(), io_status::input);
    obj.main_cont_beg();

    std::u32string buf(16, U'#');
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
    EXPECT_EQ(obj.tell(), 2u);

    EXPECT_NO_THROW(obj.switch_to_put());
    const char32_t x = U'X';
    obj.put(&x, 1);

    obj.switch_to_get();
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
    EXPECT_EQ(buf.substr(0, 2), U"cd");

    auto [dev, err] = obj.detach();
    EXPECT_FALSE(err);
    EXPECT_EQ(dev.str(), "abXcd");
}

namespace
{
    bool g_refuse_switch_to_put = false;

    // A layer below that forwards to rb_root_cvt<mem_device<char>> without being one,
    // so code_cvt reads through abs_cvt's read area. Positioning only when Seekable;
    // switch_to_put() fails while g_refuse_switch_to_put is set.
    template <bool Seekable>
    class wrapped_root
    {
        using root_type = rb_root_cvt<mem_device<char>>;

    public:
        using device_type   = root_type::device_type;
        using internal_type = root_type::internal_type;
        using external_type = root_type::external_type;

        explicit wrapped_root(mem_device<char> dev) : m_root(std::move(dev)) {}

        std::size_t get(char* to, std::size_t n) { return m_root.get(to, n); }
        bool is_eof() { return m_root.is_eof(); }
        void put(const char* from, std::size_t n) { m_root.put(from, n); }
        void flush() { m_root.flush(); }

        device_type& device() noexcept { return m_root.device(); }
        std::pair<device_type, std::exception_ptr> detach() noexcept { return m_root.detach(); }
        void attach(device_type dev) { m_root.attach(std::move(dev)); }
        io_status bos() { return m_root.bos(); }
        void main_cont_beg() { m_root.main_cont_beg(); }
        void adjust(const cvt_behavior& b) { m_root.adjust(b); }
        void retrieve(cvt_status& s) const { m_root.retrieve(s); }

        void switch_to_get() { m_root.switch_to_get(); }
        void switch_to_put()
        {
            if (g_refuse_switch_to_put)
                throw cvt_error("wrapped_root: switch_to_put refused");
            m_root.switch_to_put();
        }

        std::size_t tell() const requires Seekable { return m_root.tell(); }
        void seek(std::size_t pos) requires Seekable { m_root.seek(pos); }
        void rseek(std::size_t pos) requires Seekable { m_root.rseek(pos); }

    private:
        root_type m_root;
    };

    static_assert(cvt_cpt::support_io_switch<wrapped_root<false>>);
    static_assert(!cvt_cpt::support_positioning<wrapped_root<false>>);
    static_assert(cvt_cpt::support_positioning<wrapped_root<true>>);

    template <bool Seekable>
    code_cvt<wrapped_root<Seekable>, char32_t> opened_c_reader(const char* bytes)
    {
        code_cvt<wrapped_root<Seekable>, char32_t> obj{wrapped_root<Seekable>{mem_device(std::string(bytes))}, "C"};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();
        return obj;
    }
}

// Below a layer that cannot be positioned, a fixed-length switch has nothing to
// undo a half read with: read-ahead bytes or a pending error refuse it, and
// leave the input as it was.
TEST(CodeCvtReadAfterError, AFixedLengthSwitchWithoutPositioningIsRefusedWhileInputIsPending)
{
    if (!c_locale_rejects_ff())
        GTEST_SKIP() << "the C locale here accepts 0xFF, so there is no decode error to defer";

    std::u32string buf(16, U'#');
    {
        // The bad byte comes first: the get fails outright, "cd" stays read ahead.
        auto obj = opened_c_reader<false>("\xff" "cd");
        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
        EXPECT_THROW(obj.switch_to_put(), cvt_error);
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
        EXPECT_EQ(buf.substr(0, 2), U"cd");
    }
    {
        // Characters first: the error is deferred, and that refuses the switch too.
        auto obj = opened_c_reader<false>("ab\xff" "cd");
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
        EXPECT_THROW(obj.switch_to_put(), cvt_error);
        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
        EXPECT_EQ(buf.substr(0, 2), U"cd");
    }
    {
        // The bad byte last: nothing is read ahead, the pending error alone refuses
        // the switch; once a get has reported it, the switch goes through.
        auto obj = opened_c_reader<false>("ab\xff");
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
        EXPECT_THROW(obj.switch_to_put(), cvt_error);
        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
        EXPECT_NO_THROW(obj.switch_to_put());
    }
}

// If the layer below fails to switch after the half read was undone, the converter
// is still reading, at the logical position: reading on gives what it would have
// given without the attempt -- the bad byte's error, then the rest.
TEST(CodeCvtReadAfterError, AFailedSwitchAfterRepositioningReadsOnAsBefore)
{
    if (!c_locale_rejects_ff())
        GTEST_SKIP() << "the C locale here accepts 0xFF, so there is no decode error to defer";

    auto obj = opened_c_reader<true>("ab\xff" "cd");
    std::u32string buf(16, U'#');
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);

    g_refuse_switch_to_put = true;
    EXPECT_THROW(obj.switch_to_put(), cvt_error);
    g_refuse_switch_to_put = false;

    EXPECT_EQ(obj.tell(), 2u);
    EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 2u);
    EXPECT_EQ(buf.substr(0, 2), U"cd");
    EXPECT_EQ(obj.get(buf.data(), buf.size()), 0u);
}
