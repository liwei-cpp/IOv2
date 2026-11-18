// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file messages_backend.h
 * @lang{ZH}
 * `messages` facet 的 POSIX 后端。
 *
 * 未绑定目录的文本域在缺省目录下查找。glibc 系统上是 `/usr/share/locale`，即发行版存放
 * `.mo` 的位置，也是 GNU gettext 安装在 `/usr` 下时的 `LOCALEDIR`。macOS 没有这样的约定位置
 * （第三方翻译装在各自的前缀下，如 Homebrew），故不设缺省：未绑定的域没有翻译。
 *
 * 系统的语言偏好：Linux 上没有环境变量以外的来源（POSIX 的 locale 就是 `LC_ALL`、`LC_*`、
 * `LANG`，都没设即 `"C"`），故列表为空。macOS 上用户的语言偏好存在系统设置里，从 Finder 启动的
 * 程序拿不到 `LANG`；这里用 CoreFoundation 的 `CFLocaleCopyPreferredLanguages` 读取，与 GNU
 * libintl 相同。因此 macOS 上要链接 `-framework CoreFoundation`：CMake 目标 `IOv2::iov2` 与
 * pkg-config 文件会带上它，只用头文件的用户要自己加。
 * @endif
 * @lang{EN}
 * POSIX backend of the `messages` facet.
 *
 * A text domain bound to no directory is looked up in a default one. On a glibc system that
 * is `/usr/share/locale`, where distributions keep their `.mo` files and GNU gettext's
 * `LOCALEDIR` when it is installed under `/usr`. macOS has no such agreed place (third-party
 * translations live under their own prefixes, Homebrew's say), so there is no default: an
 * unbound domain has no translations.
 *
 * The system's language preference: Linux has no source besides the environment (POSIX's
 * locale is `LC_ALL`, `LC_*` and `LANG`, and `"C"` when none is set), so the list is empty.
 * macOS keeps the user's language preference in the system settings, and a program started
 * from the Finder has no `LANG`; it is read here with CoreFoundation's
 * `CFLocaleCopyPreferredLanguages`, as GNU libintl does. macOS therefore has to link
 * `-framework CoreFoundation`: the CMake target `IOv2::iov2` and the pkg-config files carry
 * it, and a user of the headers alone adds it themselves.
 * @endif
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <array>
#include <memory>
#include <type_traits>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/CFString.h>
#endif

namespace IOv2::detail
{
// The directory an unbound text domain is looked up in; empty for none.
inline std::filesystem::path messages_default_dirname()
{
#if defined(__APPLE__)
    return {};
#else
    return "/usr/share/locale";
#endif
}

// The user's preferred languages, most preferred first, named as the system names them
// (BCP-47 on macOS: "zh-Hans-CN", "en-US"); empty where there is no such list (Linux).
inline std::vector<std::string> messages_system_languages()
{
#if defined(__APPLE__)
    // Released however the loop below ends; CFRelease takes no null, unique_ptr passes none.
    struct cf_release
    {
        void operator()(CFArrayRef a) const noexcept { CFRelease(a); }
    };
    const std::unique_ptr<std::remove_pointer_t<CFArrayRef>, cf_release> langs(CFLocaleCopyPreferredLanguages());

    std::vector<std::string> res;
    if (!langs)
        return res;
    const CFIndex count = CFArrayGetCount(langs.get());
    for (CFIndex i = 0; i < count; ++i)
    {
        const auto name = static_cast<CFStringRef>(CFArrayGetValueAtIndex(langs.get(), i));
        std::array<char, 128> buf{};   // a language tag is a few ASCII letters
        if (name && CFStringGetCString(name, buf.data(), static_cast<CFIndex>(buf.size()), kCFStringEncodingUTF8))
            res.emplace_back(buf.data());
    }
    return res;
#else
    return {};
#endif
}
}
