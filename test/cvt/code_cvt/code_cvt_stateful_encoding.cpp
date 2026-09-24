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
 * glibc ships no stateful LC_CTYPE, so the suite builds one: a hand-written
 * charmap that names the ISO-2022-JP codeset (glibc's gconv module does the
 * actual encoding, so ASCII entries are all the charmap needs), compiled by
 * localedef into this suite's working directory. A locale outside the system
 * paths can only be reached through LOCPATH, which changes how every
 * newlocale in the process resolves names, and glibc leaks a little memory
 * on each lookup made through it. So the checks run in a child process that
 * has LOCPATH set, no locale variables, and LeakSanitizer turned off.
 */
#include <IOv2/cvt/code_cvt.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <support/exe_path.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace IOv2;

namespace
{
    const char* const kChildEnv = "IOV2_STATEFUL_ENCODING_CHILD";
    const char* const kLocaleName = "xx_XX.ISO-2022-JP";

    // U+4E2D in JIS X 0208 is 0x4366, entered from ASCII with ESC $ B.
    const std::string kShiftIn = "\x1b$B";
    const std::string kZhong = kShiftIn + "Cf";
    const std::string kShiftOut = "\x1b(B";

    // Not representable in ISO-2022-JP: neither ASCII nor JIS X 0208 has it.
    constexpr wchar_t kUnencodable = L'é';

    std::filesystem::path resource(const char* leaf)
    {
        std::filesystem::path p = exe_path();
        p = p.remove_filename() / ".." / "IOv2TestResources" / "iso2022jp" / leaf;
        return std::filesystem::canonical(p);
    }

    // localedef exits 1 with -c when it only warned (the source leaves six
    // categories undefined), so success is read from the output, not the status.
    bool build_locale(const std::filesystem::path& locpath)
    {
        const std::filesystem::path out = locpath / kLocaleName;
        std::filesystem::remove_all(out);
        std::filesystem::create_directories(locpath);
        const std::string charmap = resource("ISO-2022-JP.cm").string();
        const std::string source = resource("mini.src").string();

        const pid_t child = ::fork();
        if (child == -1)
            return false;
        if (child == 0)
        {
            const int null_fd = ::open("/dev/null", O_WRONLY);
            ::dup2(null_fd, STDOUT_FILENO);
            ::dup2(null_fd, STDERR_FILENO);
            ::setenv("LC_ALL", "C", 1);
            ::execlp("localedef", "localedef", "-c", "-f", charmap.c_str(),
                     "-i", source.c_str(), out.c_str(), static_cast<char*>(nullptr));
            ::_exit(127);
        }

        int status = 0;
        if (::waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) > 1)
            return false;
        return std::filesystem::exists(out / "LC_CTYPE");
    }

    int run_child(const std::filesystem::path& locpath)
    {
        const std::string executable = exe_path();
        const pid_t child = ::fork();
        if (child == -1)
            return -1;

        if (child == 0)
        {
            for (const char* name : {"LC_ALL", "LC_CTYPE", "LC_COLLATE", "LC_MONETARY",
                                     "LC_NUMERIC", "LC_TIME", "LC_MESSAGES", "LANG"})
                ::unsetenv(name);
            ::setenv("LOCPATH", locpath.c_str(), 1);
            std::string asan = "detect_leaks=0";
            if (const char* old = std::getenv("ASAN_OPTIONS"); old != nullptr && *old != '\0')
                asan = std::string(old) + ":" + asan;
            ::setenv("ASAN_OPTIONS", asan.c_str(), 1);
            ::setenv(kChildEnv, "1", 1);
            ::execl(executable.c_str(), executable.c_str(),
                    "--gtest_filter=CodeCvtStatefulEncoding.AFailedEncodeKeepsTheShiftState",
                    "--gtest_color=no", static_cast<char*>(nullptr));
            ::_exit(127);
        }

        int status = 0;
        if (::waitpid(child, &status, 0) != child || !WIFEXITED(status))
            return -1;
        return WEXITSTATUS(status);
    }

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
}

TEST(CodeCvtStatefulEncoding, AFailedEncodeKeepsTheShiftState)
{
    if (std::getenv(kChildEnv) == nullptr)
    {
        const std::filesystem::path locpath =
            std::filesystem::current_path() / "stateful-locales";
        ASSERT_TRUE(build_locale(locpath))
            << "localedef could not build " << kLocaleName << " under " << locpath;
        EXPECT_EQ(run_child(locpath), 0)
            << "the checks under the ISO-2022-JP locale failed; see the child's output above";
        return;
    }

    // --- child ---
    kernel_resumes_from_the_shifted_state();
    kernel_unshifts_after_a_failed_encode();
    detach_after_a_failed_put_returns_to_ascii<rb_root_cvt<mem_device<char>>>();
    detach_after_a_failed_put_returns_to_ascii<no_rb_root_cvt<mem_device<char>>>();
}
