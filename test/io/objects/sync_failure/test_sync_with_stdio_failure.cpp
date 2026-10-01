// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * What cin / wcin.sync_with_stdio() does when rebuilding the iochannel fails.
 *
 * The rebuild allocates (the root buffer and the kernel) and, on wcin, builds a
 * locale with newlocale and copies the decoder's with duplocale. None of them fails
 * in a healthy process, so this suite links with -Wl,--wrap=_Znwm,
 * -Wl,--wrap=newlocale and -Wl,--wrap=duplocale and makes them fail while one
 * sync_with_stdio() call runs. Wrapping, unlike defining a global operator new,
 * leaves AddressSanitizer's own allocator in place.
 *
 * The contract, from the @note on stdin_api::sync_with_stdio: the flag keeps its
 * old value and so does the return; the failure is a state bit (otherfailbit
 * for memory, cvtfailbit for the locale), thrown as the original exception when
 * the mask has that bit; the stream is left unattached, so after clear() an
 * extraction still fails, and reset() attaches a fresh device, after which the
 * input reads on -- as far as the mode the flag reports. A byte a deof() probe
 * had read ahead is lost with the old iochannel.
 */
#include <IOv2/io/objects/objects.h>
#include <IOv2/io/traits/arithmetic.h>

#include <support/stdio_guard.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <cstddef>
#include <new>
#include <string>
#include <type_traits>

#include <locale.h>
#include <unistd.h>

using namespace IOv2;

#if defined(IOV2_TEST_WRAP_ALLOC)
namespace
{
    enum class inject { off, count, every_new, nth_new, every_newlocale, nth_duplocale };

    inject g_mode = inject::off;
    long g_nth = 0;
    long g_new_calls = 0;
    long g_newlocale_calls = 0;
    long g_duplocale_calls = 0;
}

extern "C" void* __real__Znwm(std::size_t);
extern "C" locale_t __real_newlocale(int, const char*, locale_t);
extern "C" locale_t __real_duplocale(locale_t);

extern "C" void* __wrap__Znwm(std::size_t n)
{
    if (g_mode != inject::off && g_mode != inject::every_newlocale && g_mode != inject::nth_duplocale)
    {
        ++g_new_calls;
        if (g_mode == inject::every_new || (g_mode == inject::nth_new && g_new_calls == g_nth))
            throw std::bad_alloc{};
    }
    return __real__Znwm(n);
}

extern "C" locale_t __wrap_newlocale(int mask, const char* name, locale_t base)
{
    if (g_mode == inject::count || g_mode == inject::every_newlocale)
    {
        ++g_newlocale_calls;
        if (g_mode == inject::every_newlocale)
        {
            errno = ENOMEM;
            return nullptr;
        }
    }
    return __real_newlocale(mask, name, base);
}

extern "C" locale_t __wrap_duplocale(locale_t loc)
{
    if (g_mode == inject::count || g_mode == inject::nth_duplocale)
    {
        ++g_duplocale_calls;
        if (g_mode == inject::nth_duplocale && g_duplocale_calls == g_nth)
        {
            errno = ENOMEM;
            return nullptr;
        }
    }
    return __real_duplocale(loc);
}

namespace
{
    // Arms the wrappers for one call only, and counts what that call did.
    struct armed
    {
        explicit armed(inject mode, long nth = 0)
        {
            g_new_calls = g_newlocale_calls = g_duplocale_calls = 0;
            g_nth = nth;
            g_mode = mode;
        }
        ~armed() { g_mode = inject::off; }
        armed(const armed&) = delete;
        armed& operator=(const armed&) = delete;
    };

    // Each test leaves the singleton as it found it: synchronized, attached, clean.
    template <typename S>
    struct restore_stream
    {
        S& s;
        explicit restore_stream(S& stream) : s(stream) {}
        ~restore_stream()
        {
            s.exceptions(ios_defs::goodbit);
            s.clear();
            s.reset();
            s.sync_with_stdio(true);
        }
    };

    // The mode the flag reports is the mode it reads in: synchronized reading
    // took only "12" and the delimiter it probed, a buffered one took it all.
    template <typename S>
    void expect_twelve_in_the_reported_mode(S& s)
    {
        int x = -1;
        s >> x;
        EXPECT_EQ(s.rdstate(), ios_defs::goodbit);
        EXPECT_EQ(x, 12);

        char next = 0;
        const auto n = ::read(STDIN_FILENO, &next, 1);
        if (s.synced_with_stdio())
        {
            EXPECT_EQ(n, 1);
            EXPECT_EQ(next, '3');
        }
        else
            EXPECT_EQ(n, 0);
    }

    template <typename S>
    void expect_unattached_then_reset_reads_on(S& s)
    {
        s.clear();
        int x = -1;
        s >> x;
        EXPECT_EQ(s.rdstate(), ios_defs::cvtfailbit | ios_defs::strfailbit)
            << "the failed rebuild left a stream that still reads";

        s.reset();
        EXPECT_EQ(s.exceptions(), ios_defs::goodbit);
        expect_twelve_in_the_reported_mode(s);
    }

    // Starts in `from`, then switches away from it with every allocation failing.
    template <typename S>
    void a_failed_allocation_changes_nothing_but_the_state_bit(S& s, bool from)
    {
        iguard g("12 34 56\n");
        restore_stream<S> restore(s);
        s.reset();
        s.sync_with_stdio(from);

        bool ret = !from;
        {
            armed a(inject::every_new);
            ret = s.sync_with_stdio(!from);
            EXPECT_GT(g_new_calls, 0) << "the wrapper never fired";
        }
        EXPECT_EQ(ret, from);
        EXPECT_EQ(s.synced_with_stdio(), from);
        EXPECT_EQ(s.rdstate(), ios_defs::otherfailbit);
        expect_unattached_then_reset_reads_on(s);
    }
}
#endif

#if defined(IOV2_TEST_WRAP_ALLOC)
#define IOV2_REQUIRE_WRAP()
#else
#define IOV2_REQUIRE_WRAP() GTEST_SKIP() << "needs the linker's --wrap=_Znwm, --wrap=newlocale and --wrap=duplocale"
#endif

TEST(SyncWithStdioFailure, AFailedAllocationChangesNothingButTheStateBit)
{
    IOV2_REQUIRE_WRAP();
#if defined(IOV2_TEST_WRAP_ALLOC)
    a_failed_allocation_changes_nothing_but_the_state_bit(IOv2::cin, true);
    a_failed_allocation_changes_nothing_but_the_state_bit(IOv2::cin, false);
    a_failed_allocation_changes_nothing_but_the_state_bit(IOv2::wcin, true);
    a_failed_allocation_changes_nothing_but_the_state_bit(IOv2::wcin, false);
#endif
}

// Only wcin builds a locale when it rebuilds; cin never calls newlocale.
TEST(SyncWithStdioFailure, AFailedLocaleIsACvtFailureOnWcinOnly)
{
    IOV2_REQUIRE_WRAP();
#if defined(IOV2_TEST_WRAP_ALLOC)
    for (const bool from : {true, false})
    {
        iguard g("12 34 56\n");
        restore_stream r(IOv2::wcin);
        IOv2::wcin.reset();
        IOv2::wcin.sync_with_stdio(from);
        {
            armed a(inject::every_newlocale);
            EXPECT_EQ(IOv2::wcin.sync_with_stdio(!from), from);
            EXPECT_GT(g_newlocale_calls, 0) << "the wrapper never fired";
        }
        EXPECT_EQ(IOv2::wcin.synced_with_stdio(), from);
        EXPECT_EQ(IOv2::wcin.rdstate(), ios_defs::cvtfailbit);
        expect_unattached_then_reset_reads_on(IOv2::wcin);
    }
    {
        iguard g("12 34 56\n");
        restore_stream r(IOv2::cin);
        IOv2::cin.reset();
        armed a(inject::every_newlocale);
        IOv2::cin.sync_with_stdio(false);
        EXPECT_EQ(g_newlocale_calls, 0);
        EXPECT_TRUE(IOv2::cin.good());
    }
#endif
}

// With the bit in the mask the original exception comes out, the bit already set
// and the flag unchanged. Arm the mask on a clean stream: arming it on a failed one
// rethrows the stored cause at once.
TEST(SyncWithStdioFailure, TheMaskRethrowsTheOriginalException)
{
    IOV2_REQUIRE_WRAP();
#if defined(IOV2_TEST_WRAP_ALLOC)
    {
        iguard g("12 34 56\n");
        restore_stream r(IOv2::cin);
        IOv2::cin.reset();
        IOv2::cin.exceptions(ios_defs::otherfailbit);
        armed a(inject::every_new);
        EXPECT_THROW(IOv2::cin.sync_with_stdio(false), std::bad_alloc);
        EXPECT_EQ(IOv2::cin.rdstate(), ios_defs::otherfailbit);
        EXPECT_TRUE(IOv2::cin.synced_with_stdio());
    }
    {
        iguard g("12 34 56\n");
        restore_stream r(IOv2::wcin);
        IOv2::wcin.reset();
        IOv2::wcin.exceptions(ios_defs::cvtfailbit);
        armed a(inject::every_newlocale);
        EXPECT_THROW(IOv2::wcin.sync_with_stdio(false), cvt_error);
        EXPECT_EQ(IOv2::wcin.rdstate(), ios_defs::cvtfailbit);
        EXPECT_TRUE(IOv2::wcin.synced_with_stdio());
    }
#endif
}

// Whichever allocation of the rebuild fails, the outcome is the same; one past the
// last, the switch goes through. The count is measured, not assumed.
TEST(SyncWithStdioFailure, EveryAllocationOfTheRebuildFailsTheSameWay)
{
    IOV2_REQUIRE_WRAP();
#if defined(IOV2_TEST_WRAP_ALLOC)
    const auto scan = [](auto& s) {
        long total = 0;
        {
            iguard g("12 34 56\n");
            restore_stream r(s);
            s.reset();
            armed a(inject::count);
            s.sync_with_stdio(false);
            total = g_new_calls;
        }
        ASSERT_GT(total, 0);
        for (long k = 1; k <= total + 1; ++k)
        {
            SCOPED_TRACE(k);
            iguard g("12 34 56\n");
            restore_stream r(s);
            s.reset();
            {
                armed a(inject::nth_new, k);
                s.sync_with_stdio(false);
            }
            if (k <= total)
            {
                EXPECT_TRUE(s.synced_with_stdio());
                EXPECT_EQ(s.rdstate(), ios_defs::otherfailbit);
                expect_unattached_then_reset_reads_on(s);
            }
            else
            {
                EXPECT_FALSE(s.synced_with_stdio());
                EXPECT_TRUE(s.good());
            }
        }
    };
    scan(IOv2::cin);
    scan(IOv2::wcin);
#endif
}

// reset() on the failed stream only attaches a default device on the same fd: it
// allocates nothing and builds no locale, so it cannot fail the same way again.
TEST(SyncWithStdioFailure, ResetAfterTheFailureNeitherAllocatesNorBuildsALocale)
{
    IOV2_REQUIRE_WRAP();
#if defined(IOV2_TEST_WRAP_ALLOC)
    iguard g("12 34 56\n");
    restore_stream r(IOv2::wcin);
    IOv2::wcin.reset();
    {
        armed a(inject::every_new);
        IOv2::wcin.sync_with_stdio(false);
    }
    ASSERT_EQ(IOv2::wcin.rdstate(), ios_defs::otherfailbit);
    {
        armed a(inject::count);
        IOv2::wcin.reset();
        EXPECT_EQ(g_new_calls, 0);
        EXPECT_EQ(g_newlocale_calls, 0);
    }
    int x = -1;
    IOv2::wcin >> x;
    EXPECT_EQ(x, 12);
#endif
}

// The byte a deof() probe read ahead lives in the device; a failed rebuild destroys
// it with the old iochannel, so the input resumes one byte later.
TEST(SyncWithStdioFailure, ABytePeekedByDeofIsLostWithTheFailedRebuild)
{
    IOV2_REQUIRE_WRAP();
#if defined(IOV2_TEST_WRAP_ALLOC)
    iguard g("12 34 56\n");
    restore_stream r(IOv2::cin);
    IOv2::cin.reset();
    ASSERT_FALSE(IOv2::cin.device().deof());
    {
        armed a(inject::every_new);
        IOv2::cin.sync_with_stdio(false);
    }
    ASSERT_EQ(IOv2::cin.rdstate(), ios_defs::otherfailbit);
    IOv2::cin.reset();
    int x = -1;
    IOv2::cin >> x;
    EXPECT_EQ(x, 2);
#endif
}

// wcin copies its decoder out before it detaches; putting the copy into the new
// iochannel is a move and cannot fail. So a failed duplocale leaves the stream
// as it was, attached, in the mode the flag reports, and none fails after the
// new iochannel is in.
TEST(SyncWithStdioFailure, AFailedDuplocaleLeavesWcinAsItWas)
{
    IOV2_REQUIRE_WRAP();
#if defined(IOV2_TEST_WRAP_ALLOC)
    for (const bool from : {true, false})
    {
        SCOPED_TRACE(from);
        long total = 0;
        {
            iguard g("12 34 56\n");
            restore_stream r(IOv2::wcin);
            IOv2::wcin.reset();
            IOv2::wcin.sync_with_stdio(from);
            armed a(inject::count);
            IOv2::wcin.sync_with_stdio(!from);
            total = g_duplocale_calls;
        }
        ASSERT_GT(total, 0);
        for (long k = 1; k <= total + 1; ++k)
        {
            SCOPED_TRACE(k);
            iguard g("12 34 56\n");
            restore_stream r(IOv2::wcin);
            IOv2::wcin.reset();
            IOv2::wcin.sync_with_stdio(from);
            bool ret = !from;
            {
                armed a(inject::nth_duplocale, k);
                ret = IOv2::wcin.sync_with_stdio(!from);
            }
            EXPECT_EQ(ret, from);
            if (k <= total)
            {
                EXPECT_EQ(IOv2::wcin.synced_with_stdio(), from);
                EXPECT_EQ(IOv2::wcin.rdstate(), ios_defs::cvtfailbit);
                IOv2::wcin.clear();
            }
            else
            {
                EXPECT_EQ(IOv2::wcin.synced_with_stdio(), !from);
                EXPECT_TRUE(IOv2::wcin.good());
            }
            expect_twelve_in_the_reported_mode(IOv2::wcin);
        }
    }
#endif
}
