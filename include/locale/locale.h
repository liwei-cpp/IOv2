#pragma once
#include <concepts>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <facet/ctype.h>
#include <facet/collate.h>
#include <facet/monetary.h>
#include <facet/numeric.h>
#include <facet/timeio.h>
#include <facet/messages.h>
#include <locale/ori_facet_buf.h>

namespace IOv2
{
template <typename T>
struct type_id
{
    inline static const void* s_id = nullptr;
};

template <typename T>
static const size_t type_id_v = reinterpret_cast<size_t>(&(type_id<T>::s_id));

template <typename TChar>
class locale
{
    template <typename T>
    struct ft_wrapper;
    
    template <typename... T>
    struct ft_wrapper<facet_create_pack<T...>>
    {
        ft_wrapper(const locale& l)
            : m_ref(l) {}

        bool has() const
        {
            return (m_ref.has<T>() && ...);
        }

        template<typename TF>
        std::shared_ptr<TF> get()
        {
            return get_helper<TF, facet_create_pack<T...>>();
        }

    private:
        template<typename TF, typename TRemArray, typename... TProcessed>
        std::shared_ptr<TF> get_helper(TProcessed&&... params)
        {
            if constexpr(facet_create_pack_size<TRemArray> > 0)
            {
                using head = typename facet_create_pack_head<TRemArray>::type;
                using tail = typename facet_create_pack_tail<TRemArray>::type;

                std::shared_ptr<head> cur = m_ref.get<head>();
                if (cur)
                    return get_helper<TF, tail>(std::forward<TProcessed>(params)..., std::move(cur));
                else
                    return nullptr;
            }
            else
            {
                return std::make_shared<TF>(std::forward<TProcessed>(params)...);
            }
        }
        
        const locale& m_ref;
    };
    
    template <typename... T>
    struct ft_wrapper<facet_create_rule<T...>>
    {
        ft_wrapper(const locale& l)
            : m_ref(l) {}
        
        bool has() const
        {
            return has_helper<T...>();
        }
        
        template<typename TF>
        std::shared_ptr<TF> get()
        {
            return get_helper<TF, T...>();
        }

    private:
        template <typename... TRem>
            requires (sizeof...(TRem) == 0)
        bool has_helper() const
        {
            return false;
        }
        
        template <typename TC, typename... TRem>
        bool has_helper() const
        {
            if constexpr(is_facet_create_pack<TC>)
            {
                auto checked = ft_wrapper<TC>(m_ref);
                if (checked.has()) return true;
                else return has_helper<TRem...>();
            }
            else
            {
                if (m_ref.has<TC>()) return true;
                else return has_helper<TRem...>();
            }
        }
        
        
        template <typename TF>
        std::shared_ptr<TF> get_helper()
        {
            return nullptr;
        }

        template <typename TF, typename TC, typename... TSub>
        std::shared_ptr<TF> get_helper()
        {
            if constexpr(is_facet_create_pack<TC>)
            {
                auto creator = ft_wrapper<TC>(m_ref);
                
                std::shared_ptr<TF> obj = creator.template get<TF>();
                if (obj) return obj;
                return get_helper<TF, TSub...>();
            }
            else
            {

                std::shared_ptr<TC> obj = m_ref.get<TC>();
                if (obj)
                    return std::make_shared<TF>(std::move(obj));
                else
                    return get_helper<TF, TSub...>();
            }
        }

        const locale& m_ref;
    };

public:
    locale()
    {
        init<ctype_conf>(std::setlocale(LC_CTYPE, nullptr));
        init<collate_conf>(std::setlocale(LC_COLLATE, nullptr));
        init<monetary_conf>(std::setlocale(LC_MONETARY, nullptr));
        init<numeric_conf>(std::setlocale(LC_NUMERIC, nullptr));
        init<timeio_conf>(std::setlocale(LC_TIME, nullptr));
    }
    
    explicit locale(const std::string& name)
    {
        init<ctype_conf>(name);
        init<collate_conf>(name);
        init<monetary_conf>(name);
        init<numeric_conf>(name);
        init<timeio_conf>(name);
    }

    locale(const locale& val)
        : m_facet_confs(val.m_facet_confs)
    {
        std::lock_guard g(val.m_facet_mutex);
        m_facets = val.m_facets;
    }

    locale(locale&& val)
        : m_facet_confs(std::move(val.m_facet_confs))
    {
        std::lock_guard g(val.m_facet_mutex);
        m_facets = val.m_facets;
    }
    
    locale& operator=(const locale& val)
    {
        std::scoped_lock g(val.m_facet_mutex, m_facet_mutex);
        m_facet_confs = val.m_facet_confs;
        m_facets = val.m_facets;
        return *this;
    }

    locale& operator=(locale&& val)
    {
        std::scoped_lock g(val.m_facet_mutex, m_facet_mutex);
        m_facet_confs = std::move(val.m_facet_confs);
        m_facets = std::move(val.m_facets);
        return *this;
    }

    locale involve(std::shared_ptr<abs_ft> ft) const
    {
        if (!ft) throw std::runtime_error("cannot add empty facet pointer into locale.");

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    locale involve_msg(const std::string& domain, const std::string& lang = "") const requires (!std::is_same_v<TChar, char>)
    {
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        auto ft = s_ori_facet_buf.try_get_msg<TChar>(domain, filtered_lang);
        if (!ft)
        {
            ft = std::make_shared<messages_conf<TChar>>(domain, filtered_lang, false);
            s_ori_facet_buf.put_msg<TChar>(ft, domain, filtered_lang);
        }
        
        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    locale involve_msg(const std::string& domain, const std::string& lang = "",
                       const std::string& cvt = std::setlocale(LC_CTYPE, nullptr)) const requires (std::is_same_v<TChar, char>)
    {
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        auto ft = s_ori_facet_buf.try_get_msg<char>(domain, filtered_lang, cvt);
        if (!ft)
        {
            ft = std::make_shared<messages_conf<char>>(domain, filtered_lang, cvt, false);
            s_ori_facet_buf.put_msg<char>(ft, domain, filtered_lang, cvt);
        }

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    template <std::derived_from<abs_ft> TF>
    locale remove() const
    {
        locale res(*this);
        if (res.has<TF>())
        {
            res.m_facet_confs.erase(TF::id());
            res.m_facets.clear();
        }
        return res;
    }

    template <std::derived_from<abs_ft> TF>
    bool has() const
    {
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return false;

        return std::dynamic_pointer_cast<TF>(it->second) != nullptr;
    }
    
    template <typename TF>
        requires (is_facet_create_rule<typename TF::create_rules>)
    bool has() const
    {
        {
            std::lock_guard g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>); it != m_facets.end())
                return true;
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        return obj.has();
    }

    template <std::derived_from<abs_ft> TF>
    std::shared_ptr<TF> get() const
    {
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return nullptr;

        return std::dynamic_pointer_cast<TF>(it->second);
    }

    template <typename TF>
        requires (is_facet_create_rule<typename TF::create_rules>)
    std::shared_ptr<TF> get() const
    {
        {
            std::lock_guard g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>); it != m_facets.end())
                return std::static_pointer_cast<TF>(it->second);
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        auto res = obj.template get<TF>();
        if (res)
        {
            std::lock_guard g(m_facet_mutex);
            m_facets.insert({type_id_v<TF>, res});
        }
        return res;
    }

private:
    template <template <typename> class T>
    void init(const std::string& ft_name)
    {
        std::size_t k = T<TChar>::id();
        auto v = s_ori_facet_buf.try_get<T<TChar>>(ft_name);
        m_facet_confs.insert({k, v});
    }

private:
    std::unordered_map<std::size_t, std::shared_ptr<abs_ft>> m_facet_confs;
    mutable std::unordered_map<std::size_t, std::shared_ptr<void>> m_facets;
    mutable std::mutex m_facet_mutex;
};
}