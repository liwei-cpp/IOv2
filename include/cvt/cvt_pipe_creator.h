#pragma once
#include <tuple>
#include <type_traits>

#include <common/metafunctions.h>
#include <cvt/root_cvt.h>

namespace IOv2
{
template <typename... T>
class cvt_pipe_creator;

template<typename T>
struct cpt_cvt_pipe_creator_helper
{
    constexpr static bool value = false;
};

template<typename... T>
struct cpt_cvt_pipe_creator_helper<cvt_pipe_creator<T...>>
{
    constexpr static bool value = true;
};

template<typename T>
concept cpt_cvt_pipe_creator = cpt_cvt_pipe_creator_helper<T>::value;

template <typename T1, typename T2>
struct cvt_creator_type_gen;

template <typename T1, typename T2>
    requires (std::is_same_v<typename T1::category, CvtCreatorCategory> &&
              std::is_same_v<typename T2::category, CvtCreatorCategory> &&
              !cpt_cvt_pipe_creator<T1> && !cpt_cvt_pipe_creator<T2>)
struct cvt_creator_type_gen<T1, T2>
{
    using type = cvt_pipe_creator<T1, T2>;
};

template <typename T1, typename... T2>
    requires (std::is_same_v<typename T1::category, CvtCreatorCategory> &&
              !cpt_cvt_pipe_creator<T1>)
struct cvt_creator_type_gen<T1, cvt_pipe_creator<T2...>>
{
    using type = cvt_pipe_creator<T1, T2...>;
};

template <typename... T1, typename T2>
    requires (std::is_same_v<typename T2::category, CvtCreatorCategory> &&
              !cpt_cvt_pipe_creator<T2>)
struct cvt_creator_type_gen<cvt_pipe_creator<T1...>, T2>
{
    using type = cvt_pipe_creator<T1..., T2>;
};

template <typename... T1, typename... T2>
struct cvt_creator_type_gen<cvt_pipe_creator<T1...>, cvt_pipe_creator<T2...>>
{
    using type = cvt_pipe_creator<T1..., T2...>;
};

template <typename T1, typename T2>
class cvt_pipe_creator<T1, T2>
{
    template <typename... U>
    friend class cvt_pipe_creator;

public:
    using category = CvtCreatorCategory;
    
    cvt_pipe_creator(const T1& c1, const T2& c2)
        : m_creators(c1, c2) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        auto c1 = std::get<0>(m_creators).create(std::forward<TKernel>(kernel));
        return std::get<1>(m_creators).create(std::move(c1));
    }
private:
    std::tuple<T1, T2> m_creators;
};

template <typename... T>
    requires (sizeof...(T) > 2)
class cvt_pipe_creator<T...>
{
    template <typename... U>
    friend class cvt_pipe_creator;

public:
    using category = CvtCreatorCategory;

    template <cpt_cvt_pipe_creator T1, cpt_cvt_pipe_creator T2>
    cvt_pipe_creator(const T1& t1, const T2& t2)
        : m_creators(std::tuple_cat(t1.m_creators, t2.m_creators)) {}

    template <typename TH, cpt_cvt_pipe_creator TR>
        requires (!cpt_cvt_pipe_creator<TH>)
    cvt_pipe_creator(const TH& h, const TR& hr)
        : m_creators(std::tuple_cat(std::make_tuple(h), hr.m_creators)) {}

    template <cpt_cvt_pipe_creator TR, typename TT>
        requires (!cpt_cvt_pipe_creator<TT>)
    cvt_pipe_creator(const TR& tr, const TT& t)
        : m_creators(std::tuple_cat(tr.m_creators, std::make_tuple(t))) {}

    template <io_converter TKernel>
    auto create(TKernel&& kernel) const
    {
        return create_helper<0>(std::forward<TKernel>(kernel));
    }
    
private:
    template <size_t N, typename TKernel>
    auto create_helper(TKernel&& kernel) const
    {
        if constexpr (sizeof...(T) == N)
            return std::forward<TKernel>(kernel);
        else
        {
            auto x = std::get<N>(m_creators).create(std::forward<TKernel>(kernel));
            return create_helper<N+1>(std::move(x));
        }
    }
private:
    std::tuple<T...> m_creators;
};

template <typename T1, typename T2>
    requires (cvt_creator<T1> && cvt_creator<T2>)
auto operator | (const T1& t1, const T2& t2)
{
    using type = typename cvt_creator_type_gen<T1, T2>::type;
    return type{t1, t2};
}
}