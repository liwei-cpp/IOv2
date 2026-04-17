#pragma once

#include <bit>
#include <cassert>
#include <climits>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include <common/clocale_wrapper.h>
#include <common/metafunctions.h>
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

namespace IOv2
{
template <typename TExt, typename TInt>
struct codecvt_kernel;

template <typename TInt>
    requires std::is_same_v<TInt, wchar_t> || 
                (std::is_same_v<TInt, char32_t> && 
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
struct codecvt_kernel<char, TInt>
{
    codecvt_kernel(const std::string& name)
        : m_inter_locale(name.c_str())
    {
        clocale_user guard(m_inter_locale);

        // there are no known constant length encodings
        // m_epc == 1 means fix len
        m_epc = MB_CUR_MAX;

        // Note: mbtowc might not thread-safe. But when checking the glibc source code, it only impact the internal mbstate_t.
        // This means that the status of internal mbstate_t is undefined when called by multiple threads.
        // But the return number can be used to determine whether it is state-dependend or not.
        m_is_state_dep = (std::mbtowc(nullptr, nullptr, MB_CUR_MAX) != 0);
        init_state();
    }

    void init_state() { m_state = std::mbstate_t{}; }
    bool is_init_state() const { return std::mbsinit(&m_state); }
    unsigned epc() const { return m_epc; }
    bool is_var_length() const { return m_epc != 1; }
    bool is_state_dep() const { return m_is_state_dep; }
    
    bool out_helper(TInt ch, char*& to, char* to_end)
    {
        clocale_user guard(m_inter_locale);
        if (to_end - to < m_epc)
            return false;

        const size_t conv = std::wcrtomb(to, ch, &m_state);
        if (conv == static_cast<size_t>(-1))
        {
            return false;
        }
        to += conv;

        return true;
    }
    
    std::pair<bool, size_t> in_helper(const char*& from, const char* from_end,
                                      TInt*& to, TInt* to_end)
    {
        clocale_user guard(m_inter_locale);
        wchar_t wch = 0;
        size_t i_count = 0;

        const size_t to_max = to_end - to;
        while (from < from_end && (i_count < to_max))
        {
            auto tmp_state = m_state;
            size_t conv = mbrtowc(&wch, from, from_end - from, &tmp_state);
            if (conv == static_cast<size_t>(-1))
                return std::pair{false, i_count};
            else if (conv == static_cast<size_t>(-2))
            {
                from = from_end;
                m_state = tmp_state;
                break;
            }
            else if (conv == 0)
            {
                *to++ = static_cast<TInt>(0);
                size_t n = 1;
                for (; n <= static_cast<size_t>(from_end - from); ++n)
                {
                    auto tmp_state2(m_state);
                    if (mbrtowc(nullptr, from, n, &tmp_state2) == 0)
                    {
                        m_state = tmp_state2;
                        break;
                    }
                }
                from += n;
                ++i_count;
            }
            else
            {
                m_state = tmp_state;
                assert(is_init_state());
                *to++ = wch;
                from += conv;
                ++i_count;
            }
        }

        return std::pair{true, i_count};
    }

private:
    clocale_wrapper m_inter_locale;
    unsigned        m_epc;
    std::mbstate_t  m_state;
    bool            m_is_state_dep;
};

template <typename TInt>
    requires std::is_same_v<TInt, char32_t> || 
                (std::is_same_v<TInt, wchar_t> && 
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
struct codecvt_kernel<char8_t, TInt>
{
    codecvt_kernel() = default;

    void init_state() { return; }
    bool is_init_state() const { return true; }
    unsigned epc() const { return 6; }
    bool is_var_length() const { return true; }
    bool is_state_dep() const { return false; }

    bool out_helper(TInt ch, char8_t*& to, char8_t* to_end)
    {
        if (to_end - to < 6)
            return false;
        const uint32_t c = static_cast<uint32_t>(ch);
        if ((uint32_t)0xD800 <= c && c <= (uint32_t)0xDFFF) [[unlikely]]
            return false;

        if (c < 0x80)
            *to++ = static_cast<char8_t>(c);
        else if (c <= (uint32_t)0x7ff)
        {
            if (to_end - to < 2) return false;
            *to++ = static_cast<char8_t>((c >> 6) + 0xC0);
            *to++ = static_cast<char8_t>((c & 0x3F) + 0x80);
        }
        else if (c <= (uint32_t)0xFFFF)
        {
            if (to_end - to < 3) return false;
            *to++ = static_cast<char8_t>((c >> 12) + 0xE0);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>((c & 0x3F) + 0x80);
        }
        else if (c <= (uint32_t)0x1FFFFF)
        {
            if (to_end - to < 4) return false;
            *to++ = static_cast<char8_t>((c >> 18) + 0xF0);
            *to++ = static_cast<char8_t>(((c >> 12) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>((c & 0x3F) + 0x80);
        }
        else if (c <= (uint32_t)0x3FFFFFF) [[unlikely]]
        {
            if (to_end - to < 5) return false;
            *to++ = static_cast<char8_t>((c >> 24) + 0xF8);
            *to++ = static_cast<char8_t>(((c >> 18) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>(((c >> 12) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>((c & 0x3F) + 0x80);
        }
        else if (c <= (uint32_t)0x7FFFFFFF)
        {
            if (to_end - to < 6) return false;
            *to++ = static_cast<char8_t>((c >> 30) + 0xFC);
            *to++ = static_cast<char8_t>(((c >> 24) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>(((c >> 18) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>(((c >> 12) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3F) + 0x80);
            *to++ = static_cast<char8_t>((c & 0x3F) + 0x80);
        }
        else [[unlikely]]
            return false;
        return true;
    }
    
    std::pair<bool, size_t> in_helper(const char8_t*& from, const char8_t* from_end,
                                      TInt*& to, TInt* to_end)
    {
        const TInt* const ori_to = to;

        while ((from != from_end) && (to != to_end))
        {
            auto c1 = static_cast<uint32_t>(*from);
            if (c1 < 0x80) [[likely]]
            {
                ++from;
                *to++ = static_cast<TInt>(c1);
            }
            else if (c1 < 0xE0)
            {
                if (from_end - from < 2) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                if ((c2 & 0xC0) != 0x80) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 6) + c2 - 0x3080;
                if (c < 0x80) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 2;
            }
            else if (c1 < 0xF0)
            {
                if (from_end - from < 3) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 12) + (c2 << 6) + c3 - 0xE2080;
                if (c < 0x800) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 3;
            }
            else if (c1 < 0xF8)
            {
                if (from_end - from < 4) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                auto c4 = static_cast<uint32_t>(from[3]);
                if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80) || ((c4 & 0xC0) != 0x80)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 18) + (c2 << 12) + (c3 << 6) + c4 - 0x3C82080;
                if (c < 0x10000) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 4;
            }
            else if (c1 < 0xFC)
            {
                if (from_end - from < 5) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                auto c4 = static_cast<uint32_t>(from[3]);
                auto c5 = static_cast<uint32_t>(from[4]);
                if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80) || ((c4 & 0xC0) != 0x80) || ((c5 & 0xC0) != 0x80)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = ((c1 & 3) << 24) + (c2 << 18) + (c3 << 12) + (c4 << 6) + c5 - 0x2082080;
                if (c < 0x200000) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 5;
            }
            else
            {
                if (from_end - from < 6) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                auto c4 = static_cast<uint32_t>(from[3]);
                auto c5 = static_cast<uint32_t>(from[4]);
                auto c6 = static_cast<uint32_t>(from[5]);
                if (((c2 & 0xC0) != 0x80) || ((c3 & 0xC0) != 0x80) || ((c4 & 0xC0) != 0x80) || ((c5 & 0xC0) != 0x80) || ((c6 & 0xC0) != 0x80)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = ((c1 & 1) << 30) + ((c2 & 0x3F) << 24) + (c3 << 18) + (c4 << 12) + (c5 << 6) + c6 - 0x2082080;
                if (c < 0x4000000) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 6;
            }
        }
        
        return std::pair{true, static_cast<size_t>(to - ori_to)};
    }
};

template <io_converter KernelType, typename CharType>
class code_cvt : public abs_cvt<KernelType, CharType, true, false, false>
{
public:
    using device_type = typename KernelType::device_type;
    using internal_type = CharType;
    using external_type = typename KernelType::internal_type;

private:
    using BT = abs_cvt<KernelType, CharType, true, false, false>;
    constexpr static size_t s_ie_ratio = sizeof(internal_type) / sizeof(external_type);
    constexpr static size_t s_max_buf_size = std::max<size_t>(MB_LEN_MAX * 16, s_ie_ratio);

    static_assert(sizeof(internal_type) >= sizeof(external_type));
    static_assert((sizeof(internal_type) % sizeof(external_type)) == 0);
    static_assert(s_max_buf_size % s_ie_ratio == 0);

public:
    template <typename ... TParams>
    code_cvt(KernelType kernel, TParams&&... params)
        : BT(std::move(kernel))
        , m_cvt_kernel(std::forward<TParams>(params)...)
        , m_is_bos_done(false)
        , m_accu_len(0)
    {}

    code_cvt(const code_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_cvt_kernel(val.m_cvt_kernel)
        , m_is_bos_done(val.m_is_bos_done)
        , m_accu_len(val.m_accu_len)
    {}

    code_cvt& operator= (const code_cvt& val)
    {
        close_stream();
        BT::operator=(val);
        m_cvt_kernel = val.m_cvt_kernel;
        m_accu_len = val.m_accu_len;
        m_is_bos_done = val.m_is_bos_done;
        return *this;
    }

    code_cvt(code_cvt&& val)
        : BT(std::move(val))
        , m_cvt_kernel(std::move(val.m_cvt_kernel))
        , m_is_bos_done(val.m_is_bos_done)
        , m_accu_len(std::move(val.m_accu_len))
    {
        val.m_accu_len = 0;
    }

    code_cvt& operator= (code_cvt&& val)
    {
        close_stream();
        BT::operator=(std::move(val));
        m_cvt_kernel = std::move(val.m_cvt_kernel);
        m_is_bos_done = val.m_is_bos_done;
        m_accu_len = val.m_accu_len;
        val.m_accu_len = 0;
        return *this;
    }
    
    ~code_cvt()
    {
        close_stream();
    }

// mandatory methods
public:
    device_type attach(device_type&& dev = device_type{})
    {
        close_stream();
        return BT::attach(std::move(dev));
    }

    device_type detach()
    {
        close_stream();
        return BT::detach();
    }

    io_status bos()
    {
        m_is_bos_done = false;
        return BT::bos();
    }

    void main_cont_beg()
    {
        m_cvt_kernel.init_state();
        m_accu_len = 0;
        m_is_bos_done = true;
        BT::main_cont_beg();
    }

// optional methods
public:
    /// put
    void put(const internal_type* to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        if (!m_is_bos_done)
            return BT::put_bos(to, to_size);

        if (BT::m_io_status != io_status::output)
        {
            if constexpr (cvt_cpt::support_io_switch<KernelType>)
                switch_to_put();
            else
                throw cvt_error("code_cvt::put fail: cannot switch to output mode.");
        }

        auto wt = this->writer(s_max_buf_size);
        const size_t buf_len = m_cvt_kernel.epc();
        for (size_t i = 0; i < to_size; ++i)
        {
            external_type* out_beg = wt.put_buf(buf_len);
            external_type* out_next = out_beg;

            internal_type ch = *to++;

            if (!m_cvt_kernel.out_helper(ch, out_next, out_beg + buf_len))
                throw cvt_error("[code_cvt::overflow] code convert fail.");

            ++m_accu_len;
            if (out_next < out_beg + buf_len)
                wt.rollback(out_beg + buf_len - out_next);
        }
        wt.commit();
    }

    /// get
    size_t get(internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        if (!m_is_bos_done)
            return BT::get_bos(to, to_max);

        if (BT::m_io_status != io_status::input)
        {
            if constexpr (cvt_cpt::support_io_switch<KernelType>)
                switch_to_get();
            else
                throw cvt_error("code_cvt::get fail: cannot switch to input mode");
        }

        auto rd = this->reader(s_max_buf_size);
        size_t total_size = 0;

        size_t prev_rollback = 0;
        while (total_size < to_max)
        {
            size_t dest_size = std::min<size_t>(to_max - total_size, s_max_buf_size);
            dest_size = std::max(dest_size, prev_rollback + 1);

            if (dest_size > s_max_buf_size) [[unlikely]]
                throw cvt_error("code_cvt::get fail, input sequence too long.");

            auto [ptr, cur_size] = rd.get_buf(dest_size);
            if (cur_size == prev_rollback)
            {
                if (cur_size == 0) return total_size;
                throw cvt_error("code_cvt::get fail, partial input sequence.");
            }

            auto ext_cur = ptr;
            auto [succ, int_len] = m_cvt_kernel.in_helper(ext_cur, ptr + cur_size, to, to + to_max - total_size);

            if (!succ)
                throw cvt_error("code_cvt::get fail, invalid external sequence.");
            m_accu_len += int_len;
            total_size += int_len;

            if (ext_cur == ptr + cur_size)
                prev_rollback = 0;
            else
            {
                prev_rollback = ptr + cur_size - ext_cur;
                rd.rollback(prev_rollback);
            }
        }
        return total_size;
    }

    /// positioning
    size_t tell() const
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        return m_accu_len;
    }

    void seek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        if (this->tell() == pos) return;

        if (m_cvt_kernel.is_var_length() || m_cvt_kernel.is_state_dep())
        {
            if ((pos == 0) && (BT::m_io_status != io_status::output))
                m_cvt_kernel.init_state();
            else
                throw cvt_error("code_cvt::seek fail: cannot seek with dependent convertor");
        }

        BT::m_kernel.seek(pos * m_cvt_kernel.epc());
        m_accu_len = pos;
    }

    void rseek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        if (m_cvt_kernel.is_var_length() || m_cvt_kernel.is_state_dep())
        {
            throw cvt_error("code_cvt::rseek fail: cannot seek with dependent convertor");
        }

        BT::m_kernel.rseek(pos * m_cvt_kernel.epc());
        if (BT::m_kernel.tell() % m_cvt_kernel.epc() != 0)
            throw cvt_error("code_cvt::rseek fail: partial sequence.");

        m_accu_len = BT::m_kernel.tell() / m_cvt_kernel.epc();
        m_cvt_kernel.init_state();
    }

    /// io-switch
    void switch_to_put()
        requires (cvt_cpt::support_io_switch<KernelType>)
    {
        switch(BT::m_io_status)
        {
        case io_status::output:
            return;
        case io_status::neutral:
            BT::m_kernel.switch_to_put();
            BT::m_io_status = io_status::output;
            return;
        default: // io_status::input
            if (!m_cvt_kernel.is_init_state())
                throw cvt_error("code_cvt::switch_to_put fail: internal state is not neutral");

            if (m_cvt_kernel.is_var_length() || m_cvt_kernel.is_state_dep())
            {
                if (!this->is_eos())
                    throw cvt_error("code_cvt::switch_to_put fail: internal buffer not empty");
            }
            BT::m_kernel.switch_to_put();
            BT::m_io_status = io_status::output;
            return;
        }
    }

    void switch_to_get()
        requires (cvt_cpt::support_io_switch<KernelType>)
    {
        switch(BT::m_io_status)
        {
        case io_status::input:
            return;
        case io_status::neutral:
            BT::m_kernel.switch_to_get();
            BT::m_io_status = io_status::input;
            return;
        default: // io_status::output
            assert(m_cvt_kernel.is_init_state());
            BT::m_kernel.switch_to_get();
            BT::m_io_status = io_status::input;
            return;
        }
    }

private:
    void close_stream()
    {
        m_cvt_kernel.init_state();
        BT::m_io_status = io_status::neutral;
        m_accu_len = 0;
        m_is_bos_done = false;
    }

protected:
    codecvt_kernel<external_type, internal_type> m_cvt_kernel;
    bool                            m_is_bos_done;
    size_t                          m_accu_len;
};

template <typename TExt, typename TInt>
class code_cvt_creator;

template <typename TInt>
    requires std::is_same_v<TInt, wchar_t> || 
                (std::is_same_v<TInt, char32_t> && 
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class code_cvt_creator<char, TInt>
{
public:
    using category = CvtCreatorCategory;
    code_cvt_creator(const std::string& name)
        : m_name(name) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        static_assert(std::is_same_v<typename TKernel::internal_type, char>);
        return code_cvt<TKernel, TInt>{std::forward<TKernel>(kernel), m_name};
    }
private:
    std::string m_name;
};

template <typename TInt>
    requires std::is_same_v<TInt, char32_t> || 
                (std::is_same_v<TInt, wchar_t> && 
                 (sizeof(char32_t) == sizeof(wchar_t)) && 
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
class code_cvt_creator<char8_t, TInt>
{
public:
    using category = CvtCreatorCategory;
    code_cvt_creator() = default;

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        static_assert(std::is_same_v<typename TKernel::internal_type, char8_t>);
        return code_cvt<TKernel, TInt>{std::forward<TKernel>(kernel)};
    }
};
}