// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// A device whose dput() can be made to fail partway, for the partial-write
// tests of root_cvt and of a layer above it.

#include <IOv2/common/defs.h>
#include <IOv2/cvt/root_cvt.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

namespace
{
    using namespace IOv2;

    std::string g_out;
    std::optional<std::size_t> g_accept;
    std::optional<std::size_t> g_report;
    bool g_plain = false;

    // Takes everything, except that one dput armed through g_accept takes that many
    // characters and throws dput_error, and one armed through g_plain takes none and
    // throws a plain device_error. g_report, if set, is what that dput_error claims was
    // taken, in place of the truth.
    struct partial_write_device
    {
        using char_type = char;

        void dput(const char* s, std::size_t n)
        {
            if (g_plain)
            {
                g_plain = false;
                throw device_error("partial_write_device: nothing accepted");
            }
            if (g_accept)
            {
                const std::size_t k = std::min(*g_accept, n);
                const std::size_t reported = g_report.value_or(k);
                g_accept.reset();
                g_report.reset();
                g_out.append(s, k);
                throw dput_error("partial_write_device: partial write", reported);
            }
            g_out.append(s, n);
        }
        void dflush() {}
    };

    using Root = rb_root_cvt<partial_write_device>;
    constexpr std::size_t kLen = Root::s_buffer_length;
}
