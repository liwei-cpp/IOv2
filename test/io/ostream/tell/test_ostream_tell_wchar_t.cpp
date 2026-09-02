// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * ostream<wchar_t>::tell.
 *
 * tell answers with an optional: a position when it can be had, nothing when
 * the stream is in no state to have one. Two ways to reach the empty answer are
 * covered here -- a stream that has already failed, and a device that cannot
 * report its own position -- because the difference between "no position" and
 * "position zero" is exactly what a caller who writes tell().value() gets
 * wrong.
 *
 * Where the position moves as characters are written is the seek suite's
 * business; what is left here is where it starts and when it stops answering.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/device/file_device.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/io/io_base.h>
#include <IOv2/io/iostream.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>

#include <support/file_guard.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace IOv2;

namespace
{
// A mem_device wrapper whose dtell() throws once a shared flag is flipped. The stream reads
// the device position during construction, so the flag starts false (construction succeeds)
// and is flipped afterwards to make a later tell() fail. The flag is shared so the copy held
// inside the stream sees the flip.
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

// A fresh output stream is at the beginning, including over a device that
// already holds something -- what is in the device is not what has been
// written through the stream.
TEST(OstreamTellWchar, ANewStreamStartsAtTheBeginning)
{
    // trunc is spelled out so that both devices create the file: ofile_device opens "w" either
    // way, but file_device without it opens "r+" and would fail on a file that does not exist.
    auto helper = []<template <typename, typename> class T, typename TDevice>()
    {
        const std::string path = "test_ostream_tell_start.txt";
        file_guard        guard(path);

        T empty{mem_device{L""}};
        T seeded{mem_device{L"already there"}};
        T file{TDevice{path, file_open_flag::trunc}, code_cvt_creator<char, wchar_t>("C")};

        EXPECT_EQ(empty.tell(), 0u);
        EXPECT_EQ(seeded.tell(), 0u);
        EXPECT_EQ(file.tell(), 0u);

        auto [dev, err] = file.detach();
        dev.close();
    };

    helper.template operator()<ostream, ofile_device<char>>();
    helper.template operator()<iostream, file_device<char>>();
}

TEST(OstreamTellWchar, TheAnswerCountsWhatHasBeenWritten)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{L""}};
        EXPECT_EQ(os.tell(), 0u);

        os << L"four";
        EXPECT_EQ(os.tell(), 4u);

        os << L' ' << 42;
        EXPECT_EQ(os.tell(), 7u);
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// A failed stream has no position to give, and says so rather than giving zero.
TEST(OstreamTellWchar, AFailedStreamHasNoPosition)
{
    auto helper = []<template <typename, typename> class T>()
    {
        T os{mem_device{L""}};
        os << L"abc";
        ASSERT_TRUE(os.tell().has_value());

        os.seek(99);                   // refused by the device
        EXPECT_FALSE(os.tell().has_value());

        os.clear();
        EXPECT_EQ(os.tell(), 3u);
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// When the underlying device's dtell() throws, tell() routes it through handle_exception
// (-> devfailbit) and returns an empty optional; with no exception mask set it does not throw.
// This drives the catch branch of stream_common_operators::tell().
TEST(OstreamTellWchar, ADeviceThatCannotReportItsPositionIsReportedNotThrown)
{
    auto helper = []<template <typename, typename> class T>()
    {
        throw_tell_device<wchar_t> dev{std::wstring(L"abc")};
        auto                    flag = dev.fail_flag();
        T                       os{dev};

        EXPECT_EQ(os.tell(), 0u);      // succeeds while the flag is false
        *flag = true;                  // now the device's dtell() throws

        std::optional<size_t> pos = 0;
        EXPECT_NO_THROW(pos = os.tell());
        EXPECT_FALSE(pos.has_value());
        EXPECT_TRUE(os.rdstate() & ios_defs::devfailbit);

        *flag = false;                 // let teardown succeed
    };

    helper.template operator()<ostream>();
    helper.template operator()<iostream>();
}

// The answer is in characters even when the device is counting bytes: five
// bytes reach the file, but the stream has written two characters.
TEST(OstreamTellWchar, TheAnswerIsInCharactersNotInDeviceUnits)
{
    const std::string path = "test_ostream_tell_units.txt";
    file_guard        guard(path);

    {
        ostream os(ofile_device<char>{path, file_open_flag::trunc},
                   code_cvt_creator<char, wchar_t>("zh_CN.UTF-8"));
        ASSERT_TRUE(static_cast<bool>(os));

        os << L"\u4e2d\u00e9";
        EXPECT_EQ(os.tell(), 2u);

        auto [dev, err] = os.detach();
        dev.close();
    }

    istream is{ifile_device<char>{path}};
    ASSERT_TRUE(static_cast<bool>(is));
    EXPECT_EQ(is.tell(), 0u);

    std::string got(16, '\0');
    char*       end = is.read(got.data(), static_cast<std::ptrdiff_t>(got.size()));
    EXPECT_EQ(static_cast<std::size_t>(end - got.data()), 5u);
}
