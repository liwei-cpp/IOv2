#pragma once
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace
{
    // Redirects stdin from a pipe whose write end stays open until close_write().
    // Unlike iguard (a regular file, where poll() always reports ready and read()
    // never waits), this reproduces a stream that has data but has not ended --
    // the only way to catch an operation that waits on input it was not asked for.
    struct pipe_iguard
    {
        explicit pipe_iguard(const std::string& buf)
        {
            if (::pipe(m_fds) != 0)
                throw std::runtime_error("Cannot create pipe");

            m_old_stdin = ::dup(STDIN_FILENO);
            if (m_old_stdin == -1 || ::dup2(m_fds[0], STDIN_FILENO) == -1)
                throw std::runtime_error("Cannot re-direct stdin");

            if (!buf.empty() && ::write(m_fds[1], buf.c_str(), buf.size())
                                    != static_cast<ssize_t>(buf.size()))
                throw std::runtime_error("Cannot fill pipe");
        }

        void close_write()
        {
            if (m_fds[1] != -1) { ::close(m_fds[1]); m_fds[1] = -1; }
        }

        ~pipe_iguard()
        {
            close_write();
            ::dup2(m_old_stdin, STDIN_FILENO);
            ::close(m_old_stdin);
            ::close(m_fds[0]);
        }

        pipe_iguard(const pipe_iguard&) = delete;
        pipe_iguard& operator=(const pipe_iguard&) = delete;

    private:
        int m_fds[2] = {-1, -1};
        int m_old_stdin = -1;
    };

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
                m_old_std = dup(STDOUT_FILENO);
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