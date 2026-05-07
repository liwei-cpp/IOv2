#pragma once
#include <common/clocale_wrapper.h>
#include <common/defs.h>
#include <cvt/abs_cvt.h>
#include <cvt/cvt_concepts.h>

#include <climits>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>

namespace IOv2
{
template <typename TExt, typename TInt>
struct codecvt_kernel;

/**
 * @brief Locale-based character encoding conversion kernel (char <-> wchar_t/char32_t).
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization. Additionally,
 *       concurrent construction of multiple instances is not thread-safe due to the
 *       use of mbtowc's internal static state.
 */
template <typename TInt>
    requires std::is_same_v<TInt, wchar_t> ||
                (std::is_same_v<TInt, char32_t> &&
                 (sizeof(char32_t) == sizeof(wchar_t)) &&
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
struct codecvt_kernel<char, TInt>
{
    static_assert(MB_LEN_MAX <= std::numeric_limits<unsigned>::max(),
                  "MB_LEN_MAX exceeds unsigned range");

    explicit codecvt_kernel(const std::string& name)
        : m_inter_locale(name.c_str())
    {
        clocale_user guard(m_inter_locale);

        // there are no known constant length encodings
        // m_epc == 1 means fixed length
        // Defensive check: the C standard requires MB_CUR_MAX to be a positive
        // integer, but a misconfigured CRT / exotic libc could report 0, which
        // would lead to division-by-zero UB in seek / rseek. Reject it here so
        // m_epc >= 1 holds as a class invariant for any constructed instance.
        const auto cur_max = MB_CUR_MAX;
        if (cur_max == 0) [[unlikely]]
            throw cvt_error("codecvt_kernel: locale reports MB_CUR_MAX == 0");
        m_epc = static_cast<unsigned>(cur_max);

        // Note: mbtowc uses internal static state, so concurrent construction of
        // codecvt_kernel instances is NOT thread-safe. However, this is acceptable
        // because this class does not support multi-threaded construction.
        // Once constructed, instance-level operations use explicit mbstate_t (m_state)
        // and are safe for single-instance usage.
        // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
        m_is_state_dep = (std::mbtowc(nullptr, nullptr, MB_CUR_MAX) != 0);
        init_state();
    }

    void init_state() { m_state = std::mbstate_t{}; }
    [[nodiscard]] bool is_init_state() const { return std::mbsinit(&m_state); }
    [[nodiscard]] unsigned epc() const { return m_epc; }
    [[nodiscard]] bool is_var_length() const { return m_epc != 1; }
    [[nodiscard]] bool is_state_dep() const { return m_is_state_dep; }

    // Precondition: `to` and `to_end` must point into the same array.
    // Behavior is undefined otherwise — std::greater enforces a total order
    // across pointers regardless of provenance, but the pointer subtraction
    // below requires same-object provenance per [expr.add]/5.
    bool out_helper(TInt ch, char*& to, char* to_end)
    {
        if (std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::out_helper fail: invalid pointer range");
        clocale_user guard(m_inter_locale);
        if (static_cast<size_t>(to_end - to) < m_epc)
            return false;

        const size_t conv = std::wcrtomb(to, ch, &m_state);
        if (conv == static_cast<size_t>(-1))
        {
            init_state();  // Reset to known state per C standard
            return false;
        }
        to += conv;

        return true;
    }

    // Precondition: `from`/`from_end` and `to`/`to_end` must each point into
    // the same array. Behavior is undefined otherwise — std::greater enforces
    // a total order across pointers regardless of provenance, but the pointer
    // subtractions below require same-object provenance per [expr.add]/5.
    std::pair<bool, size_t> in_helper(const char*& from, const char* from_end,
                                      TInt*& to, TInt* to_end)
    {
        if (std::greater<>{}(from, from_end) || std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::in_helper fail: invalid pointer range");
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
                // Find the actual byte length of the null character encoding
                // before writing to output buffer to ensure consistency
                size_t n = 1;
                const auto max_n = static_cast<size_t>(from_end - from);
                for (; n <= max_n; ++n)
                {
                    auto tmp_state2(m_state);
                    if (mbrtowc(nullptr, from, n, &tmp_state2) == 0)
                    {
                        m_state = tmp_state2;
                        break;
                    }
                }
                if (n > max_n)
                    return std::pair{false, i_count};

                // Only write to output after validation succeeds
                *to++ = static_cast<TInt>(0);
                from += n;
                ++i_count;
            }
            else
            {
                // mbrtowc returning > 0 guarantees a complete character was converted;
                // no partial character can exist in m_state at this point
                m_state = tmp_state;
                *to++ = wch;
                from += conv;
                ++i_count;
            }
        }

        return std::pair{true, i_count};
    }

private:
    clocale_wrapper m_inter_locale;
    unsigned        m_epc = 0;
    std::mbstate_t  m_state{};
    bool            m_is_state_dep = false;
};

/**
 * @brief UTF-8 encoding conversion kernel (char8_t <-> char32_t/wchar_t).
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization.
 */
template <typename TInt>
    requires std::is_same_v<TInt, char32_t> ||
                (std::is_same_v<TInt, wchar_t> &&
                 (sizeof(char32_t) == sizeof(wchar_t)) &&
                 (static_cast<wchar_t>(U'李') == L'李') &&
                 (static_cast<char32_t>(L'伟') == U'伟'))
struct codecvt_kernel<char8_t, TInt>
{
    codecvt_kernel() = default;

    void init_state() { /* no-op for stateless UTF-8 */ }
    [[nodiscard]] bool is_init_state() const { return true; }
    [[nodiscard]] unsigned epc() const { return 4; }
    [[nodiscard]] bool is_var_length() const { return true; }
    [[nodiscard]] bool is_state_dep() const { return false; }

    // Precondition: `to` and `to_end` must point into the same array.
    // Behavior is undefined otherwise — std::greater enforces a total order
    // across pointers regardless of provenance, but the pointer subtraction
    // below requires same-object provenance per [expr.add]/5.
    bool out_helper(TInt ch, char8_t*& to, char8_t* to_end)
    {
        if (std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::out_helper fail: invalid pointer range");
        // Check for maximum UTF-8 length (4 bytes) upfront
        if (to_end - to < 4)
            return false;
        const auto c = static_cast<uint32_t>(ch);
        if (0xD800U <= c && c <= 0xDFFFU) [[unlikely]]
            return false;

        if (c < 0x80U)
            *to++ = static_cast<char8_t>(c);
        else if (c <= 0x7ffU)
        {
            *to++ = static_cast<char8_t>((c >> 6) + 0xC0U);
            *to++ = static_cast<char8_t>((c & 0x3FU) + 0x80U);
        }
        else if (c <= 0xFFFFU)
        {
            *to++ = static_cast<char8_t>((c >> 12) + 0xE0U);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3FU) + 0x80U);
            *to++ = static_cast<char8_t>((c & 0x3FU) + 0x80U);
        }
        else if (c <= 0x10FFFFU)
        {
            *to++ = static_cast<char8_t>((c >> 18) + 0xF0U);
            *to++ = static_cast<char8_t>(((c >> 12) & 0x3FU) + 0x80U);
            *to++ = static_cast<char8_t>(((c >> 6) & 0x3FU) + 0x80U);
            *to++ = static_cast<char8_t>((c & 0x3FU) + 0x80U);
        }
        else [[unlikely]]
            return false;
        return true;
    }

    // Precondition: `from`/`from_end` and `to`/`to_end` must each point into
    // the same array. Behavior is undefined otherwise — std::greater enforces
    // a total order across pointers regardless of provenance, but the pointer
    // subtractions below require same-object provenance per [expr.add]/5.
    std::pair<bool, size_t> in_helper(const char8_t*& from, const char8_t* from_end,
                                      TInt*& to, TInt* to_end)
    {
        if (std::greater<>{}(from, from_end) || std::greater<>{}(to, to_end)) [[unlikely]]
            throw cvt_error("codecvt_kernel::in_helper fail: invalid pointer range");
        const TInt* const ori_to = to;

        while ((from != from_end) && (to != to_end))
        {
            auto c1 = static_cast<uint32_t>(*from);
            if (c1 < 0x80U) [[likely]]
            {
                ++from;
                *to++ = static_cast<TInt>(c1);
            }
            else if (c1 < 0xE0U)
            {
                if (c1 < 0xC0U) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                if (from_end - from < 2) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                if ((c2 & 0xC0U) != 0x80U) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 6) + c2 - 0x3080U;
                if (c < 0x80U) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 2;
            }
            else if (c1 < 0xF0U)
            {
                if (from_end - from < 3) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                if (((c2 & 0xC0U) != 0x80U) || ((c3 & 0xC0U) != 0x80U)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 12) + (c2 << 6) + c3 - 0xE2080U;
                if (c < 0x800U) return std::pair{false, static_cast<size_t>(to - ori_to)};
                if (c >= 0xD800U && c <= 0xDFFFU) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 3;
            }
            else if (c1 < 0xF8U)
            {
                if (from_end - from < 4) break;
                auto c2 = static_cast<uint32_t>(from[1]);
                auto c3 = static_cast<uint32_t>(from[2]);
                auto c4 = static_cast<uint32_t>(from[3]);
                if (((c2 & 0xC0U) != 0x80U) || ((c3 & 0xC0U) != 0x80U) || ((c4 & 0xC0U) != 0x80U)) [[unlikely]]
                    return std::pair{false, static_cast<size_t>(to - ori_to)};
                auto c = (c1 << 18) + (c2 << 12) + (c3 << 6) + c4 - 0x3C82080U;
                if (c < 0x10000U || c > 0x10FFFFU) return std::pair{false, static_cast<size_t>(to - ori_to)};
                *to++ = c;
                from += 4;
            }
            else
                return std::pair{false, static_cast<size_t>(to - ori_to)};
        }

        return std::pair{true, static_cast<size_t>(to - ori_to)};
    }
};

/**
 * @brief Character encoding converter that transforms between external and internal character types.
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization.
 */
template <io_converter KernelType, typename CharType>
class code_cvt : public abs_cvt<code_cvt<KernelType, CharType>, KernelType, CharType, true, false, false>
{
    using BT = abs_cvt<code_cvt<KernelType, CharType>, KernelType, CharType, true, false, false>;
    friend BT; // for put_main, get_main

public:
    using device_type = typename KernelType::device_type;
    using internal_type = CharType;
    using external_type = typename KernelType::internal_type;

private:
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
    {}

    code_cvt(const code_cvt& val)
        requires (std::copy_constructible<KernelType>)
        : BT(val)
        , m_cvt_kernel(val.m_cvt_kernel)
        , m_accu_len(val.m_accu_len)
    {}

    code_cvt& operator=(const code_cvt& val)
        requires (std::copy_constructible<KernelType>)
    {
        if (this == &val) return *this;
        code_cvt tmp(val);
        *this = std::move(tmp);
        return *this;
    }

    code_cvt(code_cvt&& val) noexcept(
        std::is_nothrow_move_constructible_v<BT> &&
        std::is_nothrow_move_constructible_v<codecvt_kernel<external_type, internal_type>>)
        : BT(std::move(val))
        , m_cvt_kernel(std::move(val.m_cvt_kernel))
        , m_accu_len(val.m_accu_len)
    {
        val.m_accu_len = 0;
    }

    code_cvt& operator=(code_cvt&& val) noexcept(
        std::is_nothrow_move_assignable_v<BT> &&
        std::is_nothrow_move_assignable_v<codecvt_kernel<external_type, internal_type>>)
    {
        if (this == &val) return *this;
        BT::operator=(std::move(val));
        m_cvt_kernel = std::move(val.m_cvt_kernel);
        m_accu_len = val.m_accu_len;
        val.m_accu_len = 0;
        return *this;
    }

    ~code_cvt() = default;

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
        return BT::bos();
    }

    void main_cont_beg()
    {
        m_cvt_kernel.init_state();
        m_accu_len = 0;
        BT::main_cont_beg();
    }

// optional methods
private:
    void put_main(cvt_writer<KernelType>& writer, const internal_type* to, size_t to_size)
        requires (cvt_cpt::support_put<KernelType>)
    {
        writer.reset(s_max_buf_size);
        const size_t buf_len = m_cvt_kernel.epc();
        for (size_t i = 0; i < to_size; ++i)
        {
            external_type* out_beg = writer.put_buf(buf_len);
            external_type* out_next = out_beg;

            internal_type ch = *to++;

            if (!m_cvt_kernel.out_helper(ch, out_next, out_beg + buf_len))
            {
                writer.rollback(buf_len);
                writer.commit();
                m_accu_len += i;
                throw cvt_error("code_cvt::put fail: input character cannot be encoded");
            }

            if (out_next < out_beg + buf_len)
                writer.rollback(out_beg + buf_len - out_next);
        }
        writer.commit();
        m_accu_len += to_size;
    }

    size_t get_main(cvt_reader<KernelType>& reader, internal_type* to, size_t to_max)
        requires (cvt_cpt::support_get<KernelType>)
    {
        reader.reset(s_max_buf_size);
        size_t total_size = 0;

        size_t prev_rollback = 0;
        while (total_size < to_max)
        {
            size_t dest_size = std::min<size_t>(to_max - total_size, s_max_buf_size);
            dest_size = std::max(dest_size, prev_rollback + 1);

            if (dest_size > s_max_buf_size) [[unlikely]]
                throw cvt_error("code_cvt::get fail: input sequence too long");

            auto [ptr, cur_size] = reader.get_buf(dest_size);
            if (cur_size == prev_rollback)
            {
                if (cur_size == 0) return total_size;
                throw cvt_error("code_cvt::get fail: partial input sequence");
            }

            auto ext_cur = ptr;
            auto [succ, int_len] = m_cvt_kernel.in_helper(ext_cur, ptr + cur_size, to, to + to_max - total_size);

            // Update accumulated length BEFORE the failure check: in_helper may
            // have written `int_len` chars into the caller's `to` buffer before
            // hitting an invalid byte. Throwing without this update would leave
            // m_accu_len inconsistent with the chars already produced.
            m_accu_len += int_len;
            total_size += int_len;

            if (!succ)
                throw cvt_error("code_cvt::get fail: invalid external sequence");

            if (ext_cur == ptr + cur_size)
                prev_rollback = 0;
            else
            {
                prev_rollback = ptr + cur_size - ext_cur;
                reader.rollback(prev_rollback);
            }
        }
        return total_size;
    }

public:
    /// positioning
    [[nodiscard]] size_t tell() const
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
                throw cvt_error("code_cvt::seek fail: cannot seek with dependent converter");
        }

        const unsigned epc = m_cvt_kernel.epc();
        if (pos > std::numeric_limits<size_t>::max() / epc)
            throw cvt_error("code_cvt::seek fail: position overflow");

        BT::m_kernel.seek(pos * epc);
        m_accu_len = pos;
    }

    void rseek(size_t pos)
        requires (cvt_cpt::support_positioning<KernelType>)
    {
        if (m_cvt_kernel.is_var_length() || m_cvt_kernel.is_state_dep())
        {
            throw cvt_error("code_cvt::rseek fail: cannot seek with dependent converter");
        }

        const unsigned epc = m_cvt_kernel.epc();
        if (pos > std::numeric_limits<size_t>::max() / epc)
            throw cvt_error("code_cvt::rseek fail: position overflow");

        BT::m_kernel.rseek(pos * epc);
        if (BT::m_kernel.tell() % epc != 0)
            throw cvt_error("code_cvt::rseek fail: partial sequence");

        m_accu_len = BT::m_kernel.tell() / epc;
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
                if (!this->is_eof())
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
            if (!m_cvt_kernel.is_init_state())
                throw cvt_error("code_cvt::switch_to_get fail: converter not in initial state");
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
        BT::m_is_bos_done = false;
        m_accu_len = 0;
    }

protected:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    codecvt_kernel<external_type, internal_type> m_cvt_kernel;

private:
    size_t m_accu_len = 0;
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
    explicit code_cvt_creator(std::string name)
        : m_name(std::move(name)) {}

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
