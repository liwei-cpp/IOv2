#pragma once
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    struct iguard
    {
        iguard(const std::string& buf)
        {
            std::filesystem::remove("stdio_input_guard");
            {
                std::ofstream t("stdio_input_guard");
                t.write(buf.c_str(), buf.size());
            }
            
            m_old_stdin = dup(STDIN_FILENO);
            if (freopen("stdio_input_guard", "r", stdin) == nullptr)
                throw std::runtime_error("Cannot re-direct stdin");
            setbuf(stdin, NULL);
        }
        
        ~iguard()
        {
            fflush(stdin);
            dup2(m_old_stdin, STDIN_FILENO);
            close(m_old_stdin);
            setbuf(stdin, NULL);
            std::filesystem::remove("stdio_input_guard");
        }

    private:
        int         m_old_stdin;
    };
    
    template <bool isOut>
    struct oguard
    {
        oguard()
        {
            m_dir_file_name = isOut ? "stdio_output_guard" : "stdio_outerr_guard";
            std::filesystem::remove(m_dir_file_name);
            
            if constexpr (isOut)
            {
                m_old_std = dup(STDERR_FILENO);
                if (freopen(m_dir_file_name, "w", stdout) == nullptr)
                    throw std::runtime_error("Cannot re-direct stdout");
                setbuf(stdout, NULL);
            }
            else
            {
                m_old_std = dup(STDERR_FILENO);
                if (freopen(m_dir_file_name, "w", stderr) == nullptr)
                    throw std::runtime_error("Cannot re-direct stderr");
                setbuf(stderr, NULL);
            }
        }
        
        ~oguard()
        {
            if constexpr (isOut)
            {
                fflush(stdout);
                dup2(m_old_std, STDOUT_FILENO);
                close(m_old_std);
                setbuf(stdout, NULL);
            }
            else
            {
                fflush(stderr);
                dup2(m_old_std, STDERR_FILENO);
                close(m_old_std);
                setbuf(stderr, NULL);
            }
            std::filesystem::remove(m_dir_file_name);
        }

        std::string contents() const
        {
            std::ifstream t(m_dir_file_name);
            std::stringstream buffer;
            buffer << t.rdbuf();
            return buffer.str();
        }
    private:
        int         m_old_std;
        const char* m_dir_file_name;
    };
}