// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

/**
 * @file messages_backend.h
 * @lang{ZH}
 * `messages` facet 的 Windows 后端。
 *
 * Windows 没有存放 `.mo` 的约定目录。GNU gettext 在那里按自身的安装位置定 `LOCALEDIR`，
 * 而一个没有安装前缀的库做不到，故不设缺省：未绑定目录的文本域没有翻译。
 *
 * 用户的语言偏好存在系统设置里，不经过环境变量：`GetUserPreferredUILanguages` 以
 * `MUI_LANGUAGE_NAME` 给出用户界面语言列表（如 `zh-CN`、`en-US`），GNU libintl 也从这里取。
 * @endif
 * @lang{EN}
 * Windows backend of the `messages` facet.
 *
 * Windows has no agreed directory for `.mo` files. GNU gettext there works its `LOCALEDIR`
 * out from where it is installed, which a library with no installation prefix cannot do,
 * so there is no default: a text domain bound to no directory has no translations.
 *
 * The user's language preference lives in the system settings, not in the environment:
 * `GetUserPreferredUILanguages` with `MUI_LANGUAGE_NAME` lists the user-interface languages
 * (`zh-CN`, `en-US`, ...), which is where GNU libintl takes it from as well.
 * @endif
 */

#pragma once

#include <IOv2/facet/platform/windows/nls_api.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace IOv2::detail
{
// The directory an unbound text domain is looked up in; empty for none.
inline std::filesystem::path messages_default_dirname()
{
    return {};
}

// The user's preferred UI languages, most preferred first, named as Windows names them
// (BCP-47); empty if they cannot be had.
inline std::vector<std::string> messages_system_languages()
{
    unsigned long count = 0;
    unsigned long size = 0;   // in wchar_t, the closing empty name included
    if (!GetUserPreferredUILanguages(win_mui_language_name, &count, nullptr, &size) || size == 0)
        return {};
    std::wstring buf(size, L'\0');
    if (!GetUserPreferredUILanguages(win_mui_language_name, &count, buf.data(), &size))
        return {};
    buf.resize(size);

    // Null-terminated names, the last followed by an empty one. A name is ASCII; one
    // that is not is no name a .mo directory could have, and is left out.
    std::vector<std::string> res;
    std::size_t pos = 0;
    while (pos < buf.size() && buf[pos] != L'\0')
    {
        std::size_t end = buf.find(L'\0', pos);
        if (end == std::wstring::npos)
            end = buf.size();
        std::string name;
        for (std::size_t i = pos; i < end && buf[i] < 0x80; ++i)
            name.push_back(static_cast<char>(buf[i]));
        if (name.size() == end - pos)
            res.push_back(std::move(name));
        pos = end + 1;
    }
    return res;
}
}
