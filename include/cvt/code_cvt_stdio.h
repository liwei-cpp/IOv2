#pragma once
#include <cvt/code_cvt.h>

#include <string>
#include <utility>

namespace IOv2
{
struct code_cvt_switch : cvt_behavior
{
    explicit code_cvt_switch(std::string p_code)
        : code(std::move(p_code)) {}

    std::string code;
};

struct code_cvt_access : cvt_status
{
    std::string code;
};

/**
 * @brief Character encoding converter with runtime locale switching support.
 *
 * @note Thread Safety: This class is NOT thread-safe. Concurrent access to the same
 *       instance from multiple threads requires external synchronization.
 */
template <io_converter KernelType>
class code_cvt_stdio : public code_cvt<KernelType, wchar_t>
{
private:
    using BT = code_cvt<KernelType, wchar_t>;

public:
    code_cvt_stdio(KernelType kernel, const std::string& code)
        : BT(std::move(kernel), code)
        , m_code(code)
    {}

    code_cvt_stdio(const code_cvt_stdio& val) = default;
    code_cvt_stdio& operator=(const code_cvt_stdio& val) = default;
    code_cvt_stdio(code_cvt_stdio&& val) = default;
    code_cvt_stdio& operator=(code_cvt_stdio&& val) = default;
    ~code_cvt_stdio() = default;

public:
    void adjust(const cvt_behavior& acc)
    {
        if (const auto* ptr = dynamic_cast<const code_cvt_switch*>(&acc); ptr)
        {
            if (!this->m_cvt_kernel.is_init_state())
                throw cvt_error("code_cvt_stdio::adjust fail: invalid state");
            // Perform all potentially-throwing operations first, then commit with noexcept moves
            codecvt_kernel<char, wchar_t> new_kernel(ptr->code);
            std::string new_code = ptr->code;
            this->m_cvt_kernel = std::move(new_kernel);
            m_code = std::move(new_code);
        }

        return BT::adjust(acc);
    }

    void retrieve(cvt_status& s) const
    {
        if (auto* ptr = dynamic_cast<code_cvt_access*>(&s); ptr)
        {
            ptr->code = m_code;
            return;
        }
        BT::retrieve(s);
    }

private:
    std::string m_code;
};

class code_cvt_stdio_creator
{
public:
    using category = CvtCreatorCategory;
    explicit code_cvt_stdio_creator(std::string name)
        : m_name(std::move(name)) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        static_assert(std::is_same_v<typename TKernel::internal_type, char>);
        return code_cvt_stdio{std::forward<TKernel>(kernel), m_name};
    }
private:
    std::string m_name;
};
}
