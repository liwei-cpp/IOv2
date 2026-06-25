#pragma once
#include <common/sing_temp.h>
#include <facet/facet_common.h>
#include <facet/messages_details.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace IOv2
{
class ori_facet_buf : public sing_temp<ori_facet_buf>
{
    friend sing_temp<ori_facet_buf>;

public:
    template <typename TF>
    std::shared_ptr<abs_ft> try_get(const std::string& name)
    {
        const std::size_t id = TF::id();

        {
            std::lock_guard guard(m_mutex);
            auto& sub_cache = m_cache[id];
            auto it = sub_cache.find(name);
            if (it != sub_cache.end())
                return it->second;
        }

        // Construct the facet *outside* the lock: a facet constructor may be
        // expensive and -- crucially -- may itself call back into ori_facet_buf, so
        // building under m_mutex would both serialize all construction and risk a
        // self-deadlock on this non-recursive mutex. Re-lock only to insert, and
        // double-check via emplace: if another thread (or a re-entrant call)
        // inserted the same (id, name) meanwhile, emplace no-ops and returns the
        // existing entry, so the object built here is simply discarded.
        auto obj = std::make_shared<TF>(name);

        std::lock_guard guard(m_mutex);
        auto& sub_cache = m_cache[id];
        return sub_cache.emplace(name, std::move(obj)).first->second;
    }

    template <typename TChar>
    std::shared_ptr<messages_conf<TChar>> try_get_msg(const std::string& domain, const std::string& lang, const std::string& cvt = "")
    {
        const std::size_t id = messages_conf<TChar>::id();

        std::lock_guard guard(m_mutex);
        auto& sub_cache = m_msg_cache[id];

        auto it = sub_cache.find(msg_key{domain, lang, cvt});
        if (it == sub_cache.end())
            return nullptr;
        return std::static_pointer_cast<messages_conf<TChar>>(it->second);
    }

    template <typename TChar>
    std::shared_ptr<messages_conf<TChar>> put_msg(std::shared_ptr<messages_conf<TChar>> ptr, const std::string& domain, const std::string& lang, const std::string& cvt = "")
    {
        if (ptr)
        {
            const std::size_t id = messages_conf<TChar>::id();

            std::lock_guard guard(m_mutex);
            auto& sub_cache = m_msg_cache[id];

            auto it = sub_cache.find(msg_key{domain, lang, cvt});
            if (it != sub_cache.end())
                return std::static_pointer_cast<messages_conf<TChar>>(it->second);
            sub_cache.emplace(msg_key{domain, lang, cvt}, ptr);
        }
        return ptr;
    }

private:
    ori_facet_buf() = default;
    ori_facet_buf(const ori_facet_buf&) = delete;
    const ori_facet_buf& operator=(const ori_facet_buf&) = delete;

private:
    // Key for the messages cache: (domain, lang, cvt). Using a tuple instead of a
    // '\n'-joined string removes the ambiguity collision that arises when a
    // component itself contains the separator (e.g. a domain with an embedded
    // '\n'): distinct (domain, lang, cvt) triples always map to distinct keys.
    using msg_key = std::tuple<std::string, std::string, std::string>;

    struct msg_key_hash
    {
        std::size_t operator()(const msg_key& k) const noexcept
        {
            const std::hash<std::string> h;
            std::size_t seed = h(std::get<0>(k));
            seed ^= h(std::get<1>(k)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h(std::get<2>(k)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    std::unordered_map<size_t, std::unordered_map<std::string, std::shared_ptr<abs_ft>>> m_cache;
    std::unordered_map<size_t, std::unordered_map<msg_key, std::shared_ptr<abs_ft>, msg_key_hash>> m_msg_cache;
    // Guards m_cache / m_msg_cache. Facet construction in try_get happens *outside*
    // this lock (it is taken only to look up and to insert), so the mutex is never
    // held across a facet constructor. That keeps construction unserialized and
    // makes a re-entrant facet ctor safe even though the mutex is non-recursive.
    std::mutex m_mutex;
};

static ori_facet_buf::init _ori_facet_buf_init;
static ori_facet_buf& s_ori_facet_buf = *ori_facet_buf::ptr();
}
