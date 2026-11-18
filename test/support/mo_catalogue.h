// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

// Small gettext catalogues for the messages tests: the bytes of a .mo file with a
// single entry, and a locale tree on disk holding some .mo bytes under a domain name.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace iov2_test
{
    inline void append_u32(std::string& bytes, std::uint32_t value, bool big_endian)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            const unsigned shift = big_endian ? 24 - 8 * i : 8 * i;
            bytes.push_back(static_cast<char>((value >> shift) & 0xffu));
        }
    }

    // A .mo file whose only entry maps original to translation, both taken as the
    // bytes to store (UTF-8, as the reader assumes).
    inline std::string one_entry_mo(const std::string& original, const std::string& translation,
                                    bool big_endian = false, std::uint32_t revision = 0)
    {
        constexpr std::uint32_t header_size = 7 * sizeof(std::uint32_t);
        constexpr std::uint32_t original_table = header_size;
        constexpr std::uint32_t translation_table = original_table + 2 * sizeof(std::uint32_t);
        constexpr std::uint32_t string_data = translation_table + 2 * sizeof(std::uint32_t);

        std::string bytes;
        append_u32(bytes, 0x950412deu, big_endian);
        append_u32(bytes, revision, big_endian);
        append_u32(bytes, 1, big_endian);
        append_u32(bytes, original_table, big_endian);
        append_u32(bytes, translation_table, big_endian);
        append_u32(bytes, 0, big_endian);
        append_u32(bytes, 0, big_endian);
        append_u32(bytes, static_cast<std::uint32_t>(original.size()), big_endian);
        append_u32(bytes, string_data, big_endian);
        append_u32(bytes, static_cast<std::uint32_t>(translation.size()), big_endian);
        append_u32(bytes, string_data + static_cast<std::uint32_t>(original.size()), big_endian);
        bytes += original;
        bytes += translation;
        return bytes;
    }

    // root/<lang>/LC_MESSAGES/<domain>.mo holding mo_bytes, removed with everything
    // under root when the guard goes.
    struct mo_tree
    {
        mo_tree(std::filesystem::path root, const std::string& domain, const std::string& mo_bytes,
                const std::string& lang = "zh_CN")
            : m_root(std::move(root))
        {
            const std::filesystem::path dir = m_root / lang / "LC_MESSAGES";
            std::filesystem::create_directories(dir);
            std::ofstream out(dir / (domain + ".mo"), std::ios::binary);
            out.write(mo_bytes.data(), static_cast<std::streamsize>(mo_bytes.size()));
        }
        mo_tree(const mo_tree&) = delete;
        mo_tree& operator=(const mo_tree&) = delete;
        ~mo_tree()
        {
            std::error_code ec;
            std::filesystem::remove_all(m_root, ec);
        }
        const std::filesystem::path& root() const { return m_root; }

    private:
        std::filesystem::path m_root;
    };
}
