// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * A character that cannot be encoded must not cost a stateful encoding its shift state.
 *
 * In ISO-2022-JP, writing U+4E2D leaves the byte stream in JIS X 0208 mode
 * (`ESC $ B` first), and only a later `ESC ( B` takes it back to ASCII. When
 * the next character cannot be encoded, wcrtomb writes nothing, so the stream is
 * still in JIS X 0208 mode. If the kernel then reset its mbstate_t to the
 * initial state, unshift() would conclude that nothing needs undoing and write
 * no `ESC ( B`, and every ASCII byte after it would be read back as JIS X 0208.
 * Through wcout that was `中` + an unencodable `é` + `clear()` + `ab` decoding
 * as `中痰`.
 *
 * Reading has the mirror problem. Fed a shift sequence one byte at a time,
 * glibc's mbrtowc returns 1 for the byte that completes it without storing a
 * character, and loses the shift state on the way; a shift sequence that ends
 * the input likewise comes back as a positive count with no character. get(&c, 1)
 * feeds one byte at a time, so `a 中 中 b c` used to read as `a \0 C f C f \0 b c`,
 * and a whole-buffer get repeated the last character. The kernel now holds a
 * partial sequence as raw bytes and feeds it to mbrtowc whole.
 *
 * code_cvt_stdio, behind the standard streams, takes no state-dependent encoding:
 * one fd is shared by several writers while the shift state lives in each
 * converter. Construction falls back to "C" and a switch is refused.
 *
 * The checks run under a locale built for the purpose, in a child process; see
 * support/stateful_locale.h.
 */
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/code_cvt_stdio.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>
#include <IOv2/device/std_device.h>

#include <support/stateful_locale.h>
#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <tuple>
#include <utility>

#include <unistd.h>

using namespace IOv2;

namespace
{
    const char* const kLocaleName = stateful_locale_name;

    // U+4E2D in JIS X 0208 is 0x4366, entered from ASCII with ESC $ B.
    const std::string kShiftIn = "\x1b$B";
    const std::string kZhong = kShiftIn + "Cf";
    const std::string kShiftOut = "\x1b(B";

    // Not representable in ISO-2022-JP: neither ASCII nor JIS X 0208 has it.
    constexpr wchar_t kUnencodable = L'é';

    // Encodes one character, appending whatever the kernel wrote to `out`.
    bool encode(codecvt_kernel<char, wchar_t>& kernel, wchar_t ch, std::string& out)
    {
        std::array<char, 16> buf{};
        char* next = buf.data();
        const bool ok = kernel.out_helper(ch, next, buf.data() + buf.size());
        out.append(buf.data(), next);
        return ok;
    }

    void kernel_resumes_from_the_shifted_state()
    {
        codecvt_kernel<char, wchar_t> kernel(kLocaleName);
        std::string out;

        ASSERT_TRUE(encode(kernel, L'中', out));
        ASSERT_EQ(out, kZhong) << "the locale did not load as ISO-2022-JP";

        EXPECT_FALSE(encode(kernel, kUnencodable, out));
        EXPECT_EQ(out, kZhong) << "a failed encode wrote bytes";
        EXPECT_FALSE(kernel.is_init_state()) << "a failed encode dropped the shift state";

        ASSERT_TRUE(encode(kernel, L'a', out));
        EXPECT_EQ(out, kZhong + kShiftOut + "a");
    }

    void kernel_unshifts_after_a_failed_encode()
    {
        codecvt_kernel<char, wchar_t> kernel(kLocaleName);
        std::string out;

        ASSERT_TRUE(encode(kernel, L'中', out));
        EXPECT_FALSE(encode(kernel, kUnencodable, out));

        std::array<char, 16> buf{};
        const std::size_t count = kernel.unshift(buf.data(), buf.size());
        EXPECT_EQ(std::string(buf.data(), count), kShiftOut);
        EXPECT_TRUE(kernel.is_init_state());
    }

    // What wcout goes through: the failed put taints the converter, and the
    // recovery detaches, which closes the stream with unshift().
    template <typename TRoot>
    void detach_after_a_failed_put_returns_to_ascii()
    {
        code_cvt<TRoot, wchar_t> obj{TRoot{mem_device("")}, kLocaleName};
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        const wchar_t zhong = L'中';
        obj.put(&zhong, 1);
        EXPECT_THROW(obj.put(&kUnencodable, 1), cvt_error);
        EXPECT_TRUE(obj.is_tainted());

        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        EXPECT_EQ(dev.str(), kZhong + kShiftOut);
    }

    // A partial sequence is held as raw bytes and completed by the next call; a
    // shift sequence alone yields no character. Holding half a sequence
    // (is_mid_seq) and being in a shift state (!is_init_state) are told apart.
    void kernel_holds_a_partial_sequence_until_it_completes()
    {
        codecvt_kernel<char, wchar_t> kernel(kLocaleName);
        std::array<wchar_t, 4> buf{};

        const std::string half = kShiftIn.substr(0, 2);
        const char* from = half.data();
        wchar_t* to = buf.data();
        auto [ok, n] = kernel.in_helper(from, half.data() + half.size(), to, buf.data() + buf.size());
        EXPECT_TRUE(ok);
        EXPECT_EQ(n, 0u);
        EXPECT_EQ(from, half.data() + half.size());
        EXPECT_TRUE(kernel.is_mid_seq()) << "half a shift sequence left no trace";
        EXPECT_TRUE(kernel.is_init_state()) << "the held bytes leaked into the shift state";

        // The rest of the shift sequence and 中: whole again, now in JIS X 0208.
        const std::string rest = kShiftIn.substr(2) + "Cf";
        from = rest.data();
        to = buf.data();
        std::tie(ok, n) = kernel.in_helper(from, rest.data() + rest.size(), to, buf.data() + buf.size());
        EXPECT_TRUE(ok);
        ASSERT_EQ(n, 1u) << "a shift sequence came back as a character";
        EXPECT_EQ(buf[0], L'中');
        EXPECT_EQ(from, rest.data() + rest.size());
        EXPECT_FALSE(kernel.is_mid_seq());
        EXPECT_FALSE(kernel.is_init_state());

        // The way back to ASCII on its own: no character, back to the initial state.
        from = kShiftOut.data();
        to = buf.data();
        std::tie(ok, n) = kernel.in_helper(from, kShiftOut.data() + kShiftOut.size(), to, buf.data() + buf.size());
        EXPECT_TRUE(ok);
        EXPECT_EQ(n, 0u);
        EXPECT_EQ(from, kShiftOut.data() + kShiftOut.size());
        EXPECT_TRUE(kernel.is_init_state());

        // init_state() drops held bytes too.
        from = half.data();
        kernel.in_helper(from, half.data() + half.size(), to, buf.data() + buf.size());
        ASSERT_TRUE(kernel.is_mid_seq());
        kernel.init_state();
        EXPECT_FALSE(kernel.is_mid_seq());
    }

    template <typename TRoot>
    std::wstring get_one_at_a_time(const std::string& bytes)
    {
        code_cvt<TRoot, wchar_t> obj{TRoot{mem_device(bytes)}, kLocaleName};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::wstring res;
        wchar_t c = 0;
        while (obj.get(&c, 1) == 1)
            res += c;
        return res;
    }

    template <typename TRoot>
    std::wstring get_in_one_call(const std::string& bytes)
    {
        code_cvt<TRoot, wchar_t> obj{TRoot{mem_device(bytes)}, kLocaleName};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::array<wchar_t, 16> buf{};
        const std::size_t n = obj.get(buf.data(), buf.size());
        return std::wstring(buf.data(), n);
    }

    template <typename TRoot>
    void shift_sequences_decode_to_no_character()
    {
        const std::string text = "a" + kZhong + "Cf" + kShiftOut + "bc";
        EXPECT_EQ(get_one_at_a_time<TRoot>(text), L"a中中bc");
        EXPECT_EQ(get_in_one_call<TRoot>(text), L"a中中bc");

        // The input ends with the shift sequence back to ASCII.
        const std::string tail = "a" + kZhong + kShiftOut;
        EXPECT_EQ(get_one_at_a_time<TRoot>(tail), L"a中");
        EXPECT_EQ(get_in_one_call<TRoot>(tail), L"a中");
    }

    // Half a JIS X 0208 character before EOF: the input was cut off, and the read
    // that reaches it fails; the character before it still comes out. The held
    // byte stays, so the converter refuses to turn round to writing. It used to
    // come back as `\0` and `C` with the state reported initial.
    template <typename TRoot>
    void a_truncated_character_fails_the_read()
    {
        code_cvt<TRoot, wchar_t> obj{TRoot{mem_device("a" + kShiftIn + "C")}, kLocaleName};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::array<wchar_t, 4> buf{};
        ASSERT_EQ(obj.get(buf.data(), buf.size()), 1u);
        EXPECT_EQ(buf[0], L'a');
        EXPECT_THROW(obj.get(buf.data(), buf.size()), cvt_error);
        EXPECT_EQ(obj.tell(), 1u);
        EXPECT_THROW(obj.switch_to_put(), cvt_error);
    }

    // Input that ends in JIS X 0208 without the way back to ASCII is not cut off:
    // every character is whole, so the read just ends.
    template <typename TRoot>
    void input_ending_in_a_shift_state_just_ends()
    {
        code_cvt<TRoot, wchar_t> obj{TRoot{mem_device("a" + kZhong)}, kLocaleName};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::array<wchar_t, 4> buf{};
        ASSERT_EQ(obj.get(buf.data(), buf.size()), 2u);
        EXPECT_EQ(std::wstring(buf.data(), 2), L"a中");
        EXPECT_EQ(obj.get(buf.data(), buf.size()), 0u);
    }

    // An invalid byte in JIS X 0208 text: only the state mbrtowc was handed is
    // unspecified after EILSEQ, so the kernel keeps the shift state it had before
    // the byte and drops nothing but held bytes. Resetting to the initial state
    // used to read the JIS X 0208 bytes after it as ASCII.
    void kernel_keeps_the_shift_state_across_an_invalid_byte()
    {
        codecvt_kernel<char, wchar_t> kernel(kLocaleName);
        std::array<wchar_t, 4> buf{};
        const auto decode = [&](const std::string& bytes, std::size_t& n) {
            const char* from = bytes.data();
            wchar_t* to = buf.data();
            auto [ok, count] = kernel.in_helper(from, bytes.data() + bytes.size(), to, buf.data() + buf.size());
            n = count;
            return ok;
        };

        std::size_t n = 0;
        ASSERT_TRUE(decode(kZhong, n));
        ASSERT_EQ(n, 1u);
        EXPECT_FALSE(kernel.is_init_state());

        EXPECT_FALSE(decode("\xff", n));
        EXPECT_FALSE(kernel.is_init_state()) << "the error dropped the shift state";
        EXPECT_FALSE(kernel.is_mid_seq());

        // Held bytes are what an error does drop: half a JIS X 0208 character.
        EXPECT_TRUE(decode("C", n));
        EXPECT_TRUE(kernel.is_mid_seq());
        EXPECT_FALSE(decode("\xff", n));
        EXPECT_FALSE(kernel.is_mid_seq());
        EXPECT_FALSE(kernel.is_init_state());

        ASSERT_TRUE(decode("Cf", n));
        ASSERT_EQ(n, 1u);
        EXPECT_EQ(buf[0], L'中');
    }

    // The same kernel serves char32_t streams (TInt = char32_t): what it writes it
    // reads back, one character at a time as well as in one call.
    template <typename TRoot>
    void char32_t_streams_round_trip()
    {
        const std::u32string text = U"a中中b";
        code_cvt<TRoot, char32_t> writer{TRoot{mem_device(std::string())}, kLocaleName};
        EXPECT_EQ(writer.bos(), io_status::output);
        writer.main_cont_beg();
        writer.put(text.data(), text.size());
        auto [dev, err] = writer.detach();
        EXPECT_FALSE(err);
        const std::string bytes = dev.str();
        EXPECT_EQ(bytes, "a" + kShiftIn + "CfCf" + kShiftOut + "b");

        code_cvt<TRoot, char32_t> one{TRoot{mem_device(bytes)}, kLocaleName};
        EXPECT_EQ(one.bos(), io_status::input);
        one.main_cont_beg();
        std::u32string got;
        char32_t c = 0;
        while (one.get(&c, 1) == 1)
            got += c;
        EXPECT_EQ(got, text);

        code_cvt<TRoot, char32_t> whole{TRoot{mem_device(bytes)}, kLocaleName};
        EXPECT_EQ(whole.bos(), io_status::input);
        whole.main_cont_beg();
        std::array<char32_t, 8> buf{};
        const std::size_t n = whole.get(buf.data(), buf.size());
        EXPECT_EQ(std::u32string(buf.data(), n), text);
    }

    using BufferedIn = code_cvt_stdio<rb_root_cvt<std_device<STDIN_FILENO>>>;

    void construction_falls_back_to_c()
    {
        iguard g("ab");
        BufferedIn obj{rb_root_cvt{std_device<STDIN_FILENO>{}}, kLocaleName};
        code_cvt_access acc;
        obj.retrieve(acc);
        EXPECT_EQ(acc.code, "C");
    }

    void a_switch_is_refused_and_changes_nothing()
    {
        iguard g("ab");
        BufferedIn obj{rb_root_cvt{std_device<STDIN_FILENO>{}}, "C"};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();
        EXPECT_THROW(obj.adjust(code_cvt_switch{kLocaleName}), cvt_error);

        // Nor through a hand-filled state: it is refused before the kernel moves.
        code_cvt_stdio_state state;
        state.kernel.emplace(kLocaleName);
        ASSERT_TRUE(state.kernel->is_state_dep());
        EXPECT_THROW(obj.adjust(state), cvt_error);
        EXPECT_TRUE(state.kernel.has_value());

        code_cvt_access acc;
        obj.retrieve(acc);
        EXPECT_EQ(acc.code, "C");
        wchar_t c = 0;
        EXPECT_EQ(obj.get(&c, 1), 1u);
        EXPECT_EQ(c, L'a');
    }

    // A 0x00 byte is a null character whatever the shift state (C11 5.2.1.2), and
    // decoding one leaves the initial state (C11 7.29.6.3.2). glibc's ISO-2022-JP
    // module keeps the JIS X 0208 state instead, and mbrtowc then fails an
    // assertion and aborts, so mbrtowc must never see a 0x00 byte here.
    std::pair<bool, std::wstring> decode(codecvt_kernel<char, wchar_t>& kernel, const std::string& in)
    {
        std::array<wchar_t, 16> buf{};
        const char* from = in.data();
        wchar_t* to = buf.data();
        auto [ok, n] = kernel.in_helper(from, in.data() + in.size(), to, buf.data() + buf.size());
        return {ok, std::wstring(buf.data(), n)};
    }

    void kernel_decodes_a_null_byte_in_a_shifted_state()
    {
        const std::string nul(1, '\0');
        {
            codecvt_kernel<char, wchar_t> kernel(kLocaleName);
            EXPECT_EQ(decode(kernel, kZhong + nul + "ab"), std::pair(true, std::wstring(L"中\0ab", 4)));
            EXPECT_TRUE(kernel.is_init_state());
        }
        {
            // The same split across calls, as get(&c, 1) feeds it: after the null
            // character `ab` is ASCII, not the JIS X 0208 U+75F0.
            codecvt_kernel<char, wchar_t> kernel(kLocaleName);
            EXPECT_EQ(decode(kernel, kZhong), std::pair(true, std::wstring(L"中")));
            EXPECT_FALSE(kernel.is_init_state());
            EXPECT_EQ(decode(kernel, nul + "ab"), std::pair(true, std::wstring(L"\0ab", 3)));
            EXPECT_TRUE(kernel.is_init_state());
        }
        {
            // A shift sequence right before it produces no character of its own.
            codecvt_kernel<char, wchar_t> kernel(kLocaleName);
            EXPECT_EQ(decode(kernel, kShiftIn + nul + "a"), std::pair(true, std::wstring(L"\0a", 2)));
        }
        {
            // Half a JIS X 0208 character cannot end at a 0x00 byte.
            codecvt_kernel<char, wchar_t> kernel(kLocaleName);
            EXPECT_EQ(decode(kernel, "a" + kShiftIn + "C" + nul + "a"), std::pair(false, std::wstring(L"a")));
        }
    }

    template <typename TRoot>
    void a_null_byte_in_a_shifted_state_reads_as_a_null_character()
    {
        const std::string text = "a" + kZhong + std::string(1, '\0') + "bc";
        const std::wstring expected(L"a中\0bc", 5);
        EXPECT_EQ(get_one_at_a_time<TRoot>(text), expected);
        EXPECT_EQ(get_in_one_call<TRoot>(text), expected);
    }

    // The locale declares <mb_cur_max> 1, but U+4E2D takes 5 bytes (ESC $ B C f).
    // wcrtomb does not stop at MB_CUR_MAX, so the kernel must not let it write
    // into a slot sized by it.
    void kernel_rejects_an_encoding_longer_than_mb_cur_max()
    {
        codecvt_kernel<char, wchar_t> kernel(undersized_locale_name);
        ASSERT_EQ(kernel.epc(), 1u) << "the locale did not load with mb_cur_max 1";

        std::array<char, 16> buf{};
        buf.fill('#');
        char* next = buf.data();
        EXPECT_THROW(kernel.out_helper(L'中', next, buf.data() + kernel.epc()), cvt_error);
        EXPECT_EQ(next, buf.data());
        EXPECT_EQ(std::string(buf.data(), buf.size()), std::string(buf.size(), '#'))
            << "the rejected character wrote bytes";
        EXPECT_TRUE(kernel.is_init_state()) << "the rejected character moved the shift state";

        std::string out;
        ASSERT_TRUE(encode(kernel, L'a', out));
        EXPECT_EQ(out, "a");
    }

    // The same through put_main: the slot is epc() bytes, the put fails and
    // nothing reaches the device.
    template <typename TRoot>
    void put_rejects_an_encoding_longer_than_mb_cur_max()
    {
        code_cvt<TRoot, wchar_t> obj{TRoot{mem_device("")}, undersized_locale_name};
        EXPECT_EQ(obj.bos(), io_status::output);
        obj.main_cont_beg();

        const wchar_t zhong = L'中';
        EXPECT_THROW(obj.put(&zhong, 1), cvt_error);
        EXPECT_TRUE(obj.is_tainted());

        auto [dev, err] = obj.detach();
        EXPECT_FALSE(err);
        EXPECT_EQ(dev.str(), "");
    }

    // Reads to the end in calls of at most `n` characters, going on after each
    // error the way a stream does after clear(); an error shows as '!'.
    template <typename TRoot>
    std::wstring read_through_errors(const std::string& bytes, std::size_t n)
    {
        code_cvt<TRoot, wchar_t> obj{TRoot{mem_device(bytes)}, kLocaleName};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        std::wstring res;
        std::array<wchar_t, 64> buf{};
        for (int round = 0; round < 64; ++round)
        {
            try
            {
                const std::size_t got = obj.get(buf.data(), n);
                if (got == 0)
                    return res;
                res.append(buf.data(), got);
            }
            catch (const cvt_error&)
            {
                res += L'!';
            }
        }
        ADD_FAILURE() << "the read did not end";
        return res;
    }

    // glibc judges a shift sequence and a bad byte behind it as one bad run, so a
    // block read used to skip just the ESC and hand out `$ B` as ASCII, with the
    // JIS X 0208 state lost. A block read now recovers as get(&c, 1) does.
    template <typename TRoot>
    void a_block_read_recovers_like_one_character_at_a_time()
    {
        const std::pair<std::string, std::wstring> cases[] = {
            {"a" + kShiftIn + "C\xff" "Cf" + kShiftOut + "b", L"a!!中b"},
            {kShiftIn + "C\xff" + kShiftOut + "b", L"!!b"},
            {"a" + kShiftIn + "\xff" "Cf", L"a!中"},
        };
        for (const auto& [bytes, expected] : cases)
            for (const std::size_t n : {1, 2, 3, 64})
                EXPECT_EQ(read_through_errors<TRoot>(bytes, n), expected) << "n = " << n;
    }

    // Redundant shift sequences are valid ISO-2022-JP. They used to pile up in the
    // held bytes until 16 of them failed the read, for some block sizes only.
    template <typename TRoot>
    void redundant_shift_sequences_read_at_any_block_size()
    {
        for (const int k : {5, 6, 8})
        {
            std::string bytes = "a";
            for (int i = 0; i < k; ++i)
                bytes += kShiftIn;
            bytes += "Cf" + kShiftOut + "b";
            for (std::size_t n = 1; n <= 40; ++n)
                EXPECT_EQ(read_through_errors<TRoot>(bytes, n), L"a中b") << "k = " << k << ", n = " << n;
        }
    }
}

TEST(CodeCvtStatefulEncoding, AFailedEncodeKeepsTheShiftState)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.AFailedEncodeKeepsTheShiftState"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    kernel_resumes_from_the_shifted_state();
    kernel_unshifts_after_a_failed_encode();
    detach_after_a_failed_put_returns_to_ascii<rb_root_cvt<mem_device<char>>>();
    detach_after_a_failed_put_returns_to_ascii<no_rb_root_cvt<mem_device<char>>>();
}

TEST(CodeCvtStatefulEncoding, ANullByteInAShiftedStateIsANullCharacter)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.ANullByteInAShiftedStateIsANullCharacter"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    kernel_decodes_a_null_byte_in_a_shifted_state();
    a_null_byte_in_a_shifted_state_reads_as_a_null_character<rb_root_cvt<mem_device<char>>>();
    a_null_byte_in_a_shifted_state_reads_as_a_null_character<no_rb_root_cvt<mem_device<char>>>();
}

TEST(CodeCvtStatefulEncoding, AnUndersizedMbCurMaxIsRejected)
{
    if (!in_stateful_child())
    {
        ASSERT_TRUE(build_undersized_locale(stateful_locpath()));
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.AnUndersizedMbCurMaxIsRejected"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    kernel_rejects_an_encoding_longer_than_mb_cur_max();
    put_rejects_an_encoding_longer_than_mb_cur_max<rb_root_cvt<mem_device<char>>>();
    put_rejects_an_encoding_longer_than_mb_cur_max<no_rb_root_cvt<mem_device<char>>>();
}

TEST(CodeCvtStatefulEncoding, AShiftSequenceDecodesToNoCharacter)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.AShiftSequenceDecodesToNoCharacter"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    kernel_holds_a_partial_sequence_until_it_completes();
    shift_sequences_decode_to_no_character<rb_root_cvt<mem_device<char>>>();
    shift_sequences_decode_to_no_character<no_rb_root_cvt<mem_device<char>>>();
    a_truncated_character_fails_the_read<rb_root_cvt<mem_device<char>>>();
    a_truncated_character_fails_the_read<no_rb_root_cvt<mem_device<char>>>();
    input_ending_in_a_shift_state_just_ends<rb_root_cvt<mem_device<char>>>();
    input_ending_in_a_shift_state_just_ends<no_rb_root_cvt<mem_device<char>>>();
}

TEST(CodeCvtStatefulEncoding, TheStdioConverterTakesNoStateDependentEncoding)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.TheStdioConverterTakesNoStateDependentEncoding"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    construction_falls_back_to_c();
    a_switch_is_refused_and_changes_nothing();
}

TEST(CodeCvtStatefulEncoding, AnInvalidByteKeepsTheShiftState)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.AnInvalidByteKeepsTheShiftState"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    kernel_keeps_the_shift_state_across_an_invalid_byte();
}

TEST(CodeCvtStatefulEncoding, Char32StreamsRoundTrip)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.Char32StreamsRoundTrip"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    char32_t_streams_round_trip<rb_root_cvt<mem_device<char>>>();
    char32_t_streams_round_trip<no_rb_root_cvt<mem_device<char>>>();
}

TEST(CodeCvtStatefulEncoding, ABlockReadRecoversLikeOneCharacterAtATime)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.ABlockReadRecoversLikeOneCharacterAtATime"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    a_block_read_recovers_like_one_character_at_a_time<rb_root_cvt<mem_device<char>>>();
    a_block_read_recovers_like_one_character_at_a_time<no_rb_root_cvt<mem_device<char>>>();
}

TEST(CodeCvtStatefulEncoding, RedundantShiftSequencesReadAtAnyBlockSize)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.RedundantShiftSequencesReadAtAnyBlockSize"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    redundant_shift_sequences_read_at_any_block_size<rb_root_cvt<mem_device<char>>>();
    redundant_shift_sequences_read_at_any_block_size<no_rb_root_cvt<mem_device<char>>>();
}
