#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <common/sing_temp.h>
#include <facet/facet_common.h>
#include <facet/messages_details.h>

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
        
        std::lock_guard guard(m_mutex);
        auto& sub_cache = m_cache[id];

        auto it = sub_cache.find(name);
        if (it == sub_cache.end())
        {
            bool emplace_res = false;
            
            std::tie(it, emplace_res) = sub_cache.emplace(std::pair(name, std::make_shared<TF>(name)));
            if (!emplace_res) throw std::runtime_error("ori_facet_buf::get fails, cannot emplace new facet");
        }
        return it->second;
    }

    template <typename TChar>
    std::shared_ptr<messages_conf<TChar>> try_get_msg(const std::string& domain, const std::string& lang, const std::string& cvt = "")
    {
        const auto name = domain + '\n' + lang + '\n' + cvt;
        const std::size_t id = messages_conf<TChar>::id();
        
        std::lock_guard guard(m_mutex);
        auto& sub_cache = m_cache[id];

        auto it = sub_cache.find(name);
        if (it == sub_cache.end())
            return nullptr;
        return std::static_pointer_cast<messages_conf<TChar>>(it->second);
    }

    template <typename TChar>
    void put_msg(std::shared_ptr<messages_conf<TChar>> ptr, const std::string& domain, const std::string& lang, const std::string& cvt = "")
    {
        const auto name = domain + '\n' + lang + '\n' + cvt;
        const std::size_t id = messages_conf<TChar>::id();
        
        std::lock_guard guard(m_mutex);
        auto& sub_cache = m_cache[id];
        sub_cache.emplace(std::pair(name, std::move(ptr)));
    }

private:
    ori_facet_buf() = default;
    ori_facet_buf(const ori_facet_buf&) = delete;
    const ori_facet_buf& operator= (const ori_facet_buf&) = delete;

private:
    std::unordered_map<size_t, std::unordered_map<std::string, std::shared_ptr<abs_ft>>> m_cache;
    std::mutex m_mutex;
};

static ori_facet_buf::init _ori_facet_buf_init;
static ori_facet_buf& s_ori_facet_buf = *ori_facet_buf::ptr();
}