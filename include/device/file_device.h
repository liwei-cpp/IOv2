#pragma once
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>

#include <common/defs.h>
#include <common/metafunctions.h>
#include <device/device_concepts.h>

namespace IOv2
{
enum class file_open_flag : unsigned
{
    none        = 0,
    binary      = (unsigned)1 << 0,
    trunc       = (unsigned)1 << 1,
    noreplace   = (unsigned)1 << 2
};

constexpr file_open_flag operator | (file_open_flag v1, file_open_flag v2)
{
    return (file_open_flag)((unsigned) v1 | (unsigned)v2);
}

constexpr file_open_flag operator & (file_open_flag v1, file_open_flag v2)
{
    return (file_open_flag)((unsigned) v1 & (unsigned)v2);
}

constexpr file_open_flag operator ~ (file_open_flag v1)
{
    return (file_open_flag)(~((unsigned) v1));
}

template <bool IsIn, bool IsOut, typename CharType>
class basic_file_device;

template <bool IsIn, bool IsOut, typename CharType>
    requires ((IsIn || IsOut) && (std::is_same_v<CharType, char> || std::is_same_v<CharType, char8_t>))
class basic_file_device<IsIn, IsOut, CharType>
{
public:
    using char_type = CharType;
    
public:
    basic_file_device() = default;

    explicit basic_file_device(const std::string& file_name, file_open_flag flags = file_open_flag::none)
    {
        const char* mode = fopen_mode(flags);
        m_file = std::fopen(file_name.c_str(), mode);
        if (!m_file)
            return;

        if (fseek_64(m_file, 0, SEEK_END) != 0)
            throw device_error("file_device::constructor fail: cannot get file length");
        int64_t len = ftell_64(m_file);
        if (len < 0)
            throw device_error("file_device::constructor fail: cannot get file length.");

        m_file_len = static_cast<size_t>(len);

        if (fseek_64(m_file, 0, SEEK_SET) != 0)
            throw device_error("file_device::constructor fail: cannot reset the file");
    }
    
    basic_file_device(const basic_file_device&) = delete;
    basic_file_device& operator= (const basic_file_device&) = delete;
    
    basic_file_device(basic_file_device&& obj)
        : m_file(obj.m_file)
        , m_file_len(obj.m_file_len)
    {
        obj.m_file = nullptr;
        obj.m_file_len = 0;
    }
    
    basic_file_device& operator= (basic_file_device&& obj)
    {
        close();
        m_file = obj.m_file;
        m_file_len = obj.m_file_len;

        obj.m_file = nullptr;
        obj.m_file_len = 0;
        return *this;
    }
    
    ~basic_file_device()
    {
        try {
            close();
        } catch (...) {}
    }

public:
    bool deos()
    {
        if (!is_open())
            return true;

        return (dtell() >= m_file_len);
    }

    bool is_open() const
    {
        return m_file != nullptr;
    }
    
    void close()
    {
        if (is_open())
        {
            std::exception_ptr ptr = nullptr;
            if constexpr (IsOut)
            {
                try {
                    dflush();
                } catch (...) {
                    ptr = std::current_exception();
                }
            }
            std::fclose(m_file);
            m_file = nullptr;
            m_file_len = 0;
            if (ptr) std::rethrow_exception(ptr);
        }
    }
    
public:
    size_t dget(CharType* s, size_t n)
        requires (IsIn)
    {
        if (!is_open()) return 0;
        size_t count = fread(s, sizeof(CharType), n, m_file);
        return count;
    }

    size_t dtell() const
    {
        if (!is_open()) return 0;
        
        auto res = ftell_64(m_file);
        if (res < 0)
            throw device_error("file_device::dtell fail: got invalid position.");
        return static_cast<size_t>(res);
    }
    
    void dseek(size_t v)
    {
        if (!is_open())
            throw device_error("file_device::dseek fail: file is closed");
        if constexpr (!IsOut)
        {
            if (v > m_file_len)
                throw device_error("file_device::dseek fail: invalid parameter");
        }
        
        if (fseek_64(m_file, static_cast<int64_t>(v), SEEK_SET) != 0)
            throw device_error("file_device::dseek fail: fseek invalid response");
    }

    void drseek(size_t offset)
    {
        if (!is_open())
            throw device_error("file_device::drseek fail: file is closed");

        if (offset > m_file_len)
            throw device_error("file_device::drseek fail: invalid parameter");
        const int64_t l_off = -static_cast<int64_t>(offset);

        if (fseek_64(m_file, l_off, SEEK_END) != 0)
            throw device_error("file_device::drseek fail: fseek invalid response");
    }
    
    void dput(const CharType* ch, size_t n)
        requires (IsOut)
    {
        if (!is_open())
            throw device_error("file_device::dput fail: file closed.");
        if (std::fwrite(ch, sizeof(CharType), n, m_file) != n)
            throw device_error("file_device::dput fail: partial success.");
        
        auto cur_pos = dtell();
        if (cur_pos > m_file_len)
            m_file_len = cur_pos;
    }
    
    void dflush()
        requires (IsOut)
    {
        if (!is_open()) return;
        if (fflush(m_file) == EOF)
            throw device_error("file_device::dflush fail: fflush fail.");
    }

private:
    static const char* fopen_mode(file_open_flag flags)
    {
        using enum file_open_flag;
        
        file_open_flag checkres = flags & (trunc | binary | noreplace);
        
        if constexpr (IsIn && IsOut)
        {
            if (checkres == none)
                return "r+";
            else if (checkres == trunc)
                return "w+";
            else if (checkres == (trunc | noreplace))
                return "w+x";
            else if (checkres == noreplace)
                return "w+x";
            else if (checkres == binary)
                return "r+b";
            else if (checkres == (trunc | binary))
                return "w+b";
            else if (checkres == (trunc | binary | noreplace))
                return "w+bx";
            else if (checkres == (binary | noreplace))
                return "w+bx";
            else
                throw device_error("Invalid open mode combination for read-write file");
        }
        else if constexpr (IsIn)
        {
           if (checkres == none)
                return "r";
            else if (checkres == binary)
                return "rb";
            else
                throw device_error("Invalid open mode combination for read-only file");
        }
        else if constexpr (IsOut)
        {
           if (checkres == none)
                return "w";
            else if (checkres == noreplace)
                return "wx";
            else if (checkres == trunc)
                return "w";
            else if (checkres == (trunc | noreplace))
                return "wx";
            else if (checkres == binary)
                return "wb";
            else if (checkres == (binary | noreplace))
                return "wbx";
            else if (checkres == (trunc | binary))
                return "wb";
            else
                throw device_error("Invalid open mode combination for write-only file");
        }
        else
        {
            static_assert(DependencyFalseV<IsIn, IsOut>, "file device cannot open with neither in nor out mode");
        }
    }
    
private:
    static int fseek_64(FILE* stream, int64_t offset, int origin)
    {
#if defined(_WIN32)
        return _fseeki64(stream, offset, origin);
#else
        return fseeko(stream, offset, origin);
#endif
    }

    static int64_t ftell_64(FILE* stream)
    {
#if defined(_WIN32)
        return _ftelli64(stream);
#else
        return ftello(stream);
#endif
    }

private:
    FILE*  m_file = nullptr;
    size_t m_file_len = 0;
};

template <typename CharType>
using ifile_device = basic_file_device<true, false, CharType>;

template <typename CharType>
using ofile_device = basic_file_device<false, true, CharType>;

template <typename CharType>
using file_device = basic_file_device<true, true, CharType>;
}