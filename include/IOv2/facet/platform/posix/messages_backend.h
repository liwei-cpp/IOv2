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
 * @endif
 * @lang{EN}
 * POSIX backend of the `messages` facet.
 *
 * A text domain bound to no directory is looked up in a default one. On a glibc system that
 * is `/usr/share/locale`, where distributions keep their `.mo` files and GNU gettext's
 * `LOCALEDIR` when it is installed under `/usr`. macOS has no such agreed place (third-party
 * translations live under their own prefixes, Homebrew's say), so there is no default: an
 * unbound domain has no translations.
 * @endif
 */

#pragma once

#include <filesystem>

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
}
