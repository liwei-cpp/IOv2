// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    template <typename TChar = char>
        requires (std::is_same_v<TChar, char> || std::is_same_v<TChar, char8_t>)
    struct file_guard
    {
        file_guard(const std::string& fileName, const std::basic_string<TChar>& buf = std::basic_string<TChar>{})
            : m_fileName(fileName)
        {
            std::filesystem::remove(m_fileName);
            if (!buf.empty())
            {
                // Binary, as file_device opens files: text mode would turn \n into
                // \r\n on Windows.
                std::ofstream t(m_fileName, std::ios::binary);
                t.write(reinterpret_cast<const char*>(buf.c_str()), buf.size());
            }
        }
        
        // Windows refuses to remove a file something still holds open, and a test
        // may declare its guard after the object that opens the file; the throwing
        // overload would then end the process from a destructor.
        ~file_guard()
        {
            std::error_code ec;
            std::filesystem::remove(m_fileName, ec);
        }
        
        std::basic_string<TChar> contents() const
        {
            std::ifstream t(m_fileName, std::ios::binary);
            std::stringstream buffer;
            buffer << t.rdbuf();
            return reinterpret_cast<const TChar*>(buffer.str().c_str());
        }
        
    private:
        std::string m_fileName;
    };
    
template<typename TChar>
file_guard(const std::string& fileName, const TChar*) -> file_guard<TChar>;
}
