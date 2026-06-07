#pragma once
#include <common/metafunctions.h>
#include <facet/facet_common.h>
#include <facet/messages_details.h>

#include <memory>
#include <stdexcept>
#include <string>

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
        : m_obj(p_obj)
    { if (!m_obj) throw std::runtime_error("shared_ptr is empty"); }

    // Gettext pass-through: the translation on a hit, otherwise the original
    // `ori`. The lvalue overload borrows (zero copy): the result aliases either
    // the dictionary (stable for the facet's lifetime) or `ori` itself on a miss,
    // so the caller's lvalue must outlive it. Only an lvalue selects this
    // overload; every rvalue — including a `const` one — routes to a by-value
    // overload below, so a temporary argument can never dangle.
    const std::basic_string<char_type>& translate(const std::basic_string<char_type>& ori) const
    {
        const static std::basic_string<char_type> empty_res;
        if (ori.empty()) return empty_res;

        const auto* p = m_obj->translate(ori);
        return p ? *p : ori;
    }

    // Rvalue input returns by value, so the result owns its data and can never
    // dangle: a miss moves the caller's temporary out (no copy), a hit copies the
    // dictionary entry out. This makes the common literal/temporary call shape
    // safe without forbidding it.
    std::basic_string<char_type> translate(std::basic_string<char_type>&& ori) const
    {
        if (ori.empty()) return {};

        const auto* p = m_obj->translate(ori);
        return p ? *p : std::move(ori);
    }

    // A `const` rvalue cannot bind to the non-const `&&` overload above; without
    // this overload it would fall back to the borrowing lvalue overload and, on a
    // miss, dangle (the result would reference the caller's expiring temporary).
    // Return by value here too: being const it cannot be moved from, so a miss
    // copies `ori` out and a hit copies the dictionary entry — either way the
    // result owns its data.
    std::basic_string<char_type> translate(const std::basic_string<char_type>&& ori) const
    {
        if (ori.empty()) return {};

        const auto* p = m_obj->translate(ori);
        return p ? *p : ori;
    }

    const std::string& filtered_lang() const
    {
        return m_obj->filtered_lang();
    }

    const std::string& domain_info() const
    {
        return m_obj->domain_info();
    }

    // The .mo header is stored as the entry whose msgid is the empty string.
    const std::basic_string<char_type>& head_entry() const
    {
        const static std::basic_string<char_type> empty_msgid;
        const auto* p = m_obj->translate(empty_msgid);
        return p ? *p : empty_msgid;
    }

private:
    std::shared_ptr<const messages_conf<CharT>> m_obj;
};

template<typename TConfPtr>
messages(TConfPtr) -> messages<typename TConfPtr::element_type::char_type>;
}
