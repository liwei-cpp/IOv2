#pragma once
#include <facet/collate.h>
#include <facet/ctype.h>
#include <facet/messages.h>
#include <facet/monetary.h>
#include <facet/numeric.h>
#include <facet/timeio.h>
#include <locale/ori_facet_buf.h>

#include <bit>
#include <clocale>
#include <concepts>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace IOv2
{
template <typename T>
struct type_id
{
    inline static const void* s_id = nullptr;
};

template <typename T>
static const size_t type_id_v = std::bit_cast<size_t>(&(type_id<T>::s_id));

// type_id_v keys m_facets by the address of type_id<T>::s_id. This mirrors
// ft_basic::id() (facet_common.h), which guards the same idiom. bit_cast already
// requires the pointer and size_t to have equal size; the assert documents the
// invariant and yields a clear diagnostic so a platform with
// sizeof(void*) > sizeof(size_t) fails to compile instead of silently aliasing
// distinct facet types in m_facets.
static_assert(sizeof(void*) <= sizeof(size_t),
              "type_id_v relies on a pointer fitting losslessly into size_t");

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
            if constexpr(is_nonempty_facet_create_pack<TC>)
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
            if constexpr(is_nonempty_facet_create_pack<TC>)
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
        init<ctype_conf>(safe_setlocale(LC_CTYPE));
        init<collate_conf>(safe_setlocale(LC_COLLATE));
        init<monetary_conf>(safe_setlocale(LC_MONETARY));
        init<numeric_conf>(safe_setlocale(LC_NUMERIC));
        init<timeio_conf>(safe_setlocale(LC_TIME));
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
    {
        std::shared_lock g(val.m_facet_mutex);
        m_facet_confs = val.m_facet_confs;
        m_facets = val.m_facets;
    }

    locale(locale&& val)
    {
        std::lock_guard g(val.m_facet_mutex);
        m_facet_confs = std::move(val.m_facet_confs);
        m_facets = std::move(val.m_facets);
    }

    locale& operator=(const locale& val)
    {
        if (this == &val) return *this;
        std::scoped_lock g(val.m_facet_mutex, m_facet_mutex);
        m_facet_confs = val.m_facet_confs;
        m_facets = val.m_facets;
        return *this;
    }

    locale& operator=(locale&& val)
    {
        if (this == &val) return *this;
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
            ft = s_ori_facet_buf.put_msg<TChar>(ft, domain, filtered_lang);
        }

        locale res(*this);
        res.m_facet_confs[ft->id()] = std::move(ft);
        res.m_facets.clear();
        return res;
    }

    locale involve_msg(const std::string& domain, const std::string& lang = "",
                       const std::string& cvt = safe_setlocale(LC_CTYPE)) const requires (std::is_same_v<TChar, char>)
    {
        const std::string filtered_lang = base_ft<messages>::filter_lang(domain, lang);
        auto ft = s_ori_facet_buf.try_get_msg<char>(domain, filtered_lang, cvt);
        if (!ft)
        {
            ft = std::make_shared<messages_conf<char>>(domain, filtered_lang, cvt, false);
            ft = s_ori_facet_buf.put_msg<char>(ft, domain, filtered_lang, cvt);
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
        std::shared_lock g(m_facet_mutex);
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return false;

        return std::dynamic_pointer_cast<TF>(it->second) != nullptr;
    }

    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>)
    bool has() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>); it != m_facets.end())
                return true;
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        return obj.has();
    }

    template <std::derived_from<abs_ft> TF>
    std::shared_ptr<TF> get() const
    {
        std::shared_lock g(m_facet_mutex);
        std::size_t id = TF::id();
        auto it = m_facet_confs.find(id);
        if (it == m_facet_confs.end()) return nullptr;

        return std::dynamic_pointer_cast<TF>(it->second);
    }

    template <typename TF>
        requires (is_nonempty_facet_create_rule<typename TF::create_rules>)
    std::shared_ptr<TF> get() const
    {
        {
            std::shared_lock g(m_facet_mutex);
            if (auto it = m_facets.find(type_id_v<TF>); it != m_facets.end())
                return std::static_pointer_cast<TF>(it->second);
        }

        ft_wrapper<typename TF::create_rules> obj(*this);
        auto res = obj.template get<TF>();
        if (res)
        {
            std::lock_guard g(m_facet_mutex);
            auto [it, inserted] = m_facets.insert({type_id_v<TF>, res});
            if (!inserted)
                return std::static_pointer_cast<TF>(it->second);
        }
        return res;
    }

private:
    /**
     * @lang{ZH}
     * 查询当前全局 C locale 的名称；不可用时回退为 "C"。
     *
     * @warning 本函数通过 `std::setlocale(category, nullptr)` 读取**进程级全局**
     * locale：返回的指针指向 C 库的共享缓冲区，随后被拷入 `std::string`。该读取
     * **假设没有其他线程并发以非空实参调用 `std::setlocale` 来修改全局 locale**——
     * 否则按 C 标准（`setlocale` 无需线程安全），该缓冲区可能在拷贝期间被改写或失效，
     * 从而构成数据竞争（UB）。注意：纯查询读取彼此之间并不冲突，只有"读 vs 并发写"
     * 才有竞态；因此在"启动期单线程配置一次 locale、之后不再修改"的常规模型下不存在
     * 该竞态。这一假设与 messages 对 `getenv` 所做的假设同源；库层面无法对此加以同步，
     * 因为外部的 `setlocale` 写者不会经过本库的任何锁。调用方应在程序启动期以单线程
     * 方式一次性配置 locale。
     *
     * 另外注意：`involve_msg(char 重载)` 的默认实参 `cvt = safe_setlocale(LC_CTYPE)`
     * 以及默认构造函数对各 `LC_*` 类别的调用，都会隐式触发此全局读取。
     * @endif
     *
     * @lang{EN}
     * Query the current global C locale name; fall back to "C" if unavailable.
     *
     * @warning This reads the *process-wide global* locale via
     * `std::setlocale(category, nullptr)`: the returned pointer aliases a shared
     * C-library buffer that is then copied into a `std::string`. The read *assumes
     * no other thread concurrently calls `std::setlocale` with a non-null argument*
     * to mutate the global locale -- otherwise, since `setlocale` need not be
     * thread-safe, the buffer may be rewritten/invalidated mid-copy, a data race
     * (UB). Note that pure query reads do not conflict with each other; only a
     * read racing a concurrent write is a problem, so under the usual model
     * (configure the locale once, single-threaded, at startup, then never modify
     * it) no such race exists. This assumption is the same one messages makes for
     * `getenv`; it cannot be synchronized at the library level because external
     * `setlocale` writers do not go through any lock this library controls.
     * Callers should configure the locale once, single-threaded, at startup.
     *
     * Also note that `involve_msg` (char overload)'s default argument
     * `cvt = safe_setlocale(LC_CTYPE)`, and the default constructor's calls for the
     * various `LC_*` categories, trigger this global read implicitly.
     * @endif
     */
    static std::string safe_setlocale(int category) noexcept
    {
        const char* p = std::setlocale(category, nullptr);
        return p ? p : "C";
    }

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
    mutable std::shared_mutex m_facet_mutex;
};
}
