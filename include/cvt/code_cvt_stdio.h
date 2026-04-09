#pragma once

#include <common/metafunctions.h>
#include <cvt/code_cvt.h>
namespace IOv2
{
struct code_cvt_switch : cvt_behavior
{
    code_cvt_switch(const std::string& p_code)
        : code(p_code) {}

    const std::string code;
};

struct code_cvt_access : cvt_status
{
    std::string code;
};

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
    code_cvt_stdio& operator= (const code_cvt_stdio& val) = default;
    code_cvt_stdio(code_cvt_stdio&& val) = default;
    code_cvt_stdio& operator= (code_cvt_stdio&& val) = default;

public:
    void adjust(const cvt_behavior& acc)
    {
        if (const code_cvt_switch* ptr = dynamic_cast<const code_cvt_switch*>(&acc); ptr)
        {
            if (!this->m_cvt_kernel.is_init_state())
                throw cvt_error("codecvt switch encoder fail: invalid state");
            m_code = ptr->code;
            this->m_cvt_kernel = codecvt_kernel<char, wchar_t>(m_code);
        }

        return BT::adjust(acc);
    }

    void retrieve(cvt_status& s) const
    {
        if (code_cvt_access* ptr = dynamic_cast<code_cvt_access*>(&s); ptr)
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
    code_cvt_stdio_creator(const std::string& name)
        : m_name(name) {}

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