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
 * Replacing the whole converter must not cost it the shift state either: a
 * fresh kernel starts in ASCII, so code_cvt_stdio_state carries the old one over.
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

    // What wcin.sync_with_stdio() does: read part of stdin through one converter,
    // detach it, and read the rest through a new one on the same device. The first
    // one has no read buffer, so the device stands right after what it decoded.
    using SyncIn = code_cvt_stdio<no_rb_root_cvt<std_device<STDIN_FILENO>>>;
    using BufferedIn = code_cvt_stdio<rb_root_cvt<std_device<STDIN_FILENO>>>;

    std::wstring read_all(BufferedIn& obj)
    {
        std::wstring res;
        wchar_t c = 0;
        while (obj.get(&c, 1) == 1)
            res += c;
        return res;
    }

    // Returns what the new converter reads after `a 中`, handing it `state` first.
    std::wstring read_on_after_a_switch(const std::string& bytes, bool carry)
    {
        iguard g(bytes);
        SyncIn first{no_rb_root_cvt{std_device<STDIN_FILENO>{}}, kLocaleName};
        EXPECT_EQ(first.bos(), io_status::input);
        first.main_cont_beg();
        wchar_t c = 0;
        EXPECT_EQ(first.get(&c, 1), 1u);
        EXPECT_EQ(c, L'a');
        EXPECT_EQ(first.get(&c, 1), 1u);
        EXPECT_EQ(c, L'中');

        code_cvt_stdio_state state;
        first.retrieve(state);
        EXPECT_TRUE(state.kernel.has_value());
        auto [dev, err] = first.detach();
        EXPECT_FALSE(err);

        BufferedIn second{rb_root_cvt{std::move(dev)}, kLocaleName};
        EXPECT_EQ(second.bos(), io_status::input);
        second.main_cont_beg();
        if (carry)
            second.adjust(state);
        return read_all(second);
    }

    void the_shift_state_goes_over_to_a_new_converter()
    {
        const std::string text = "a" + kZhong + "Cf" + kShiftOut + "b";
        EXPECT_EQ(read_on_after_a_switch(text, true), L"中b");
        // What a fresh kernel makes of the same bytes: JIS X 0208 read as ASCII.
        EXPECT_EQ(read_on_after_a_switch(text, false), L"Cfb");
    }

    // The query still answers code_cvt_access, and an empty state changes nothing.
    void other_queries_and_an_empty_state_are_unaffected()
    {
        iguard g("ab");
        BufferedIn obj{rb_root_cvt{std_device<STDIN_FILENO>{}}, kLocaleName};
        EXPECT_EQ(obj.bos(), io_status::input);
        obj.main_cont_beg();

        code_cvt_access acc;
        obj.retrieve(acc);
        EXPECT_EQ(acc.code, kLocaleName);

        const code_cvt_stdio_state empty;
        obj.adjust(empty);
        EXPECT_EQ(read_all(obj), L"ab");
    }

    // Half a character held at the end of the input goes over as well: the new
    // converter still reports the input as cut off.
    void a_held_half_character_goes_over_too()
    {
        iguard g("a" + kShiftIn + "C");
        SyncIn first{no_rb_root_cvt{std_device<STDIN_FILENO>{}}, kLocaleName};
        EXPECT_EQ(first.bos(), io_status::input);
        first.main_cont_beg();
        wchar_t c = 0;
        EXPECT_EQ(first.get(&c, 1), 1u);
        EXPECT_THROW(first.get(&c, 1), cvt_error);

        code_cvt_stdio_state state;
        first.retrieve(state);
        ASSERT_TRUE(state.kernel.has_value());
        EXPECT_TRUE(state.kernel->is_mid_seq());
        auto [dev, err] = first.detach();

        BufferedIn second{rb_root_cvt{std::move(dev)}, kLocaleName};
        EXPECT_EQ(second.bos(), io_status::input);
        second.main_cont_beg();
        second.adjust(state);
        EXPECT_THROW(second.get(&c, 1), cvt_error);
        EXPECT_THROW(second.adjust(code_cvt_switch{"C"}), cvt_error);
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

TEST(CodeCvtStatefulEncoding, TheDecoderStateGoesOverToANewConverter)
{
    if (!in_stateful_child())
    {
        EXPECT_EQ(run_stateful_child("CodeCvtStatefulEncoding.TheDecoderStateGoesOverToANewConverter"), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    the_shift_state_goes_over_to_a_new_converter();
    other_queries_and_an_empty_state_are_unaffected();
    a_held_half_character_goes_over_too();
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
