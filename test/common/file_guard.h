#pragma once
#include <type_traits>
#include <filesystem>
#include <fstream>
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
                std::ofstream t(m_fileName);
                t.write(reinterpret_cast<const char*>(buf.c_str()), buf.size());
            }
        }
        
        ~file_guard()
        {
            std::filesystem::remove(m_fileName);
        }
        
        std::basic_string<TChar> contents() const
        {
            std::ifstream t(m_fileName);
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
