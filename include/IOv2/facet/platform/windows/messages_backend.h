// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file messages_backend.h
 * @lang{ZH}
 * `messages` facet 的 Windows 后端。
 *
 * Windows 没有存放 `.mo` 的约定目录。GNU gettext 在那里按自身的安装位置定 `LOCALEDIR`，
 * 而一个没有安装前缀的库做不到，故不设缺省：未绑定目录的文本域没有翻译。
 * @endif
 * @lang{EN}
 * Windows backend of the `messages` facet.
 *
 * Windows has no agreed directory for `.mo` files. GNU gettext there works its `LOCALEDIR`
 * out from where it is installed, which a library with no installation prefix cannot do,
 * so there is no default: a text domain bound to no directory has no translations.
 * @endif
 */

#pragma once

#include <filesystem>

namespace IOv2::detail
{
// The directory an unbound text domain is looked up in; empty for none.
inline std::filesystem::path messages_default_dirname()
{
    return {};
}
}
