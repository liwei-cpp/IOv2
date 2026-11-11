// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * The BOS partial-unit cases of ../test_root_cvt_io.cpp, which take wchar_t to be a
 * four-byte external unit, as it is on Linux and macOS.
 */
#include <IOv2/common/defs.h>
#include <IOv2/cvt/abs_cvt.h>
#include <IOv2/cvt/cvt_concepts.h>
#include <IOv2/cvt/root_cvt.h>
#include <IOv2/device/mem_device.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <exception>
#include <type_traits>
#include <utility>

using namespace IOv2;

namespace
{
    // Minimal cvt: exposes `char` as internal_type while the kernel uses
    // `wchar_t` units. Lets us exercise the BOS partial-unit code path in
    // abs_cvt::get / abs_cvt::put (triggered when to_max * sizeof(char) is not a
    // multiple of sizeof(wchar_t)).
    template <io_converter KernelType>
        requires std::is_same_v<typename KernelType::internal_type, wchar_t>
    struct wext_char_cvt
        : public abs_cvt<KernelType, char,
                         /*position=*/false, /*io_switch=*/false>
    {
        using BT = abs_cvt<KernelType, char, false, false>;
        friend BT;

    public:
        using device_type   = typename KernelType::device_type;
        using internal_type = char;
        using external_type = wchar_t;
        explicit wext_char_cvt(KernelType k) : BT(std::move(k)) {}

        std::pair<std::size_t, std::exception_ptr>
                    get_main(char*, std::size_t) noexcept { return {0, nullptr}; }
        void        put_main(const char*, std::size_t) {}
    };
}

// The BOS phase reads and writes in whole external units. wext_char_cvt has a
// one-byte internal type over a four-byte external one, so a request for three
// characters ends part-way through a unit -- the partial-unit path in
// abs_cvt::get / abs_cvt::put.
TEST(CvtIo, GetDuringBosHandlesAPartialExternalUnit)
{
    using WKernelT = rb_root_cvt<mem_device<wchar_t>>;
    wext_char_cvt<WKernelT> cvt{WKernelT{mem_device<wchar_t>{L"\x01020304"}}};
    EXPECT_EQ(cvt.bos(), io_status::input);

    char buf[4] = {};
    EXPECT_EQ(cvt.get(buf, 3), 3u);
}

TEST(CvtIo, PutDuringBosHandlesAPartialExternalUnit)
{
    using WKernelT = rb_root_cvt<mem_device<wchar_t>>;
    wext_char_cvt<WKernelT> cvt{WKernelT{mem_device<wchar_t>{L""}}};
    EXPECT_EQ(cvt.bos(), io_status::output);

    const char src[3] = {'a', 'b', 'c'};
    EXPECT_NO_THROW(cvt.put(src, 3));
}
