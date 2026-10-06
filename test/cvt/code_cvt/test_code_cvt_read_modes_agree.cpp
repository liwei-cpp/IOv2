// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * However a code_cvt is read -- a character at a time, as formatted extraction does, or in
 * blocks of any size -- the same input decodes to the same text and the same errors.
 *
 * An incomplete or invalid sequence used to be split between the read buffer and bytes the
 * kernel held back. A character-at-a-time read had the first bytes of such a sequence in the
 * kernel, and an error dropped them all; a block read had them in the buffer and skipped
 * only the first. Where glibc needs a third or fourth byte to call a sequence invalid
 * (GB18030, EUC-JP's 0x8F plane), the bytes it dropped were valid characters.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <clocale>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include <locale.h>

using namespace IOv2;

namespace
{
    constexpr char32_t kError = U'�';

    bool have_locale(const char* name)
    {
        locale_t l = newlocale(LC_CTYPE_MASK, name, nullptr);
        if (l == nullptr)
            return false;
        freelocale(l);
        return true;
    }

    // Reads everything in calls of `chunk` characters; an error shows as U+FFFD.
    template <typename TRoot>
    std::u32string read_all(const std::string& bytes, const char* locale, std::size_t chunk)
    {
        code_cvt<TRoot, char32_t> obj{TRoot{mem_device(bytes)}, locale};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::u32string out;
        std::vector<char32_t> buf(chunk);
        for (std::size_t guard = 0; guard < 4 * bytes.size() + 8; ++guard)
        {
            try
            {
                const std::size_t n = obj.get(buf.data(), buf.size());
                if (n == 0)
                    return out;
                out.append(buf.data(), n);
            }
            catch (const cvt_error&)
            {
                out += kError;
            }
        }
        ADD_FAILURE() << "no end after " << 4 * bytes.size() + 8 << " calls";
        return out;
    }

    template <typename TRoot>
    void expect_every_read_mode_agrees(const std::string& bytes, const char* locale)
    {
        const std::u32string by_block = read_all<TRoot>(bytes, locale, 64);
        for (std::size_t chunk : {1u, 2u, 3u, 5u})
            EXPECT_EQ(read_all<TRoot>(bytes, locale, chunk), by_block) << "chunk " << chunk;
    }

    void expect_text(const std::string& bytes, const char* locale, const std::u32string& want)
    {
        for (std::size_t chunk : {1u, 64u})
        {
            SCOPED_TRACE(chunk);
            EXPECT_EQ(read_all<no_rb_root_cvt<mem_device<char>>>(bytes, locale, chunk), want);
            EXPECT_EQ(read_all<rb_root_cvt<mem_device<char>>>(bytes, locale, chunk), want);
        }
    }
}

// glibc calls 81 30 41 invalid only at the fourth byte, 62; the 0 and A after the bad
// lead byte are characters of their own.
TEST(CodeCvtReadModesAgree, Gb18030)
{
    if (!have_locale("zh_CN.gb18030"))
        GTEST_SKIP() << "zh_CN.gb18030 not installed";
    expect_text("a\x81\x30\x41" "b", "zh_CN.gb18030", U"a�0Ab");
}

// 8F A4 A2 41: invalid at the 41; A4 A2 is あ.
TEST(CodeCvtReadModesAgree, EucJp)
{
    if (!have_locale("ja_JP.eucjp"))
        GTEST_SKIP() << "ja_JP.eucjp not installed";
    expect_text("a\x8f\xa4\xa2\x41" "b", "ja_JP.eucjp", U"a�あAb");
}

// E4 B8 41: two bad sequences, E4 and B8, whichever way it is read.
TEST(CodeCvtReadModesAgree, Utf8)
{
    expect_text("a\xe4\xb8\x41" "b", "zh_CN.UTF-8", U"a��Ab");
}

// Random bytes drawn mostly from lead and trail ranges, so that sequences break at
// every length; whole blocks, small blocks and single characters must agree.
TEST(CodeCvtReadModesAgree, RandomInput)
{
    struct case_t { const char* locale; const char* alphabet; };
    const case_t cases[] = {
        {"zh_CN.UTF-8",   "ab\x80\xbf\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80\xff"},
        {"zh_CN.gb18030", "ab09\x81\x30\x39\x84\xfe\xa1\xd6\xd0"},
        {"ja_JP.eucjp",   "abA\x8e\x8f\xa1\xa4\xa2\xb0\xfe"},
    };

    std::mt19937 gen(20261006);
    for (const auto& c : cases)
    {
        if (!have_locale(c.locale))
            continue;
        SCOPED_TRACE(c.locale);
        const std::string alphabet = c.alphabet;
        std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
        std::uniform_int_distribution<std::size_t> length(1, 12);
        for (int i = 0; i < 300; ++i)
        {
            std::string bytes;
            for (std::size_t n = length(gen); n != 0; --n)
                bytes += alphabet[pick(gen)];
            // Never end inside a sequence (reported for good): glibc may want all four
            // bytes of a GB18030 sequence before it calls it invalid.
            bytes += "zzzz";
            std::string hex;
            for (unsigned char b : bytes)
                hex += "0123456789abcdef"[b >> 4], hex += "0123456789abcdef"[b & 15], hex += ' ';
            SCOPED_TRACE(hex);
            expect_every_read_mode_agrees<no_rb_root_cvt<mem_device<char>>>(bytes, c.locale);
            expect_every_read_mode_agrees<rb_root_cvt<mem_device<char>>>(bytes, c.locale);
            if (HasFailure())
                return;
        }
    }
}
