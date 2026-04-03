#pragma once
#include <common/defs.h>
#include <device/device_concepts.h>

#include <concepts>
#include <utility>

namespace IOv2
{
    struct cvt_behavior
    {
        virtual ~cvt_behavior() = default;
    };

    struct cvt_status
    {
        virtual ~cvt_status() = default;
    };
    
    enum class io_status : char
    {
        neutral,
        input,
        output
    };

    namespace cvt_cpt
    {
        template <typename T>
        concept support_put = requires(T a)
            {
                { a.put(std::declval<const typename T::internal_type*>(), std::declval<size_t>()) } -> std::same_as<void>;
                { a.flush() } -> std::same_as<void>;
            };

        template <typename T>
        concept support_get = requires(T a)
            {
                { a.get(std::declval<typename T::internal_type*>(), std::declval<size_t>()) } -> std::same_as<size_t>;
            };

        template <typename T>
        concept support_positioning = requires(T a, const T& c)
            {
                { c.tell() } -> std::same_as<size_t>;
                { a.seek(std::declval<size_t>()) } -> std::same_as<void>;
                { a.rseek(std::declval<size_t>()) } -> std::same_as<void>;
            };

        template <typename T>
        concept support_io_switch = support_put<T> &&
            support_get<T> &&
            requires(T a)
            {
                { a.switch_to_get() } -> std::same_as<void>;
                { a.switch_to_put() } -> std::same_as<void>;
            };

        template <typename T>
        concept mandatory_methods = requires(T a, const T& c,
                                             const cvt_behavior& b, cvt_status& s)
            {
                { c.device() } -> std::same_as<const typename T::device_type&>;
                { a.detach() } -> std::same_as<typename T::device_type>;
                { a.attach(std::declval<typename T::device_type>()) } -> std::same_as<typename T::device_type>;

                { a.bos() } -> std::same_as<io_status>;
                { a.main_cont_beg() } -> std::same_as<void>;
                { a.is_eos() } -> std::same_as<bool>;

                { a.adjust(b) } -> std::same_as<void>;
                { c.retrieve(s) } -> std::same_as<void>;
            };
    }

    template<typename T>
    concept io_converter = (cvt_cpt::support_put<T> || cvt_cpt::support_get<T>) &&
        cvt_cpt::mandatory_methods<T> &&
        requires {
            typename T::internal_type;
            typename T::external_type;
        };

    struct CvtCreatorCategory;

    template<typename T>
    concept cvt_creator = std::is_same_v<typename T::category, CvtCreatorCategory>; 
}