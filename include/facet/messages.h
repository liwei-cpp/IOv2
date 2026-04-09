#pragma once
#include <common/metafunctions.h>
#include <facet/messages_details.h>
#include <memory>

namespace IOv2
{
template <typename CharT>
class messages
{
public:
    using create_rules = facet_create_rule<messages_conf<CharT>>;

    using char_type = CharT;

    template <shared_ptr_to<messages_conf<CharT>> TConfPtr>
    messages(TConfPtr p_obj)
        : m_obj(avail_ptr(p_obj)) {}

    const std::basic_string<char_type>& translate(const std::basic_string<char_type>& ori) const
    {
        const static std::basic_string<char_type> empty_res;
        if (ori.empty()) return empty_res;

        return m_obj->translate(ori);
    }

    const std::string& filtered_lang() const
    {
        return m_obj->filtered_lang();
    }

    const std::string& domain_info() const
    {
        return m_obj->domain_info();
    }

    const std::basic_string<char_type>& head_entry() const
    {
        const static std::basic_string<char_type> input;
        return m_obj->translate(input);
    }

private:
    std::shared_ptr<const messages_conf<CharT>> m_obj;
};

template<typename TConfPtr>
messages(TConfPtr) -> messages<typename TConfPtr::element_type::char_type>;
}