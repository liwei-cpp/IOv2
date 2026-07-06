#pragma once
#include <cvt/cvt_concepts.h>
#include <cvt/root_cvt.h>
#include <device/device_concepts.h>

#include <utility>

namespace IOv2
{
template <io_device TDevice, cvt_creator TCreator>
struct ext_to_int_helper
{
    using RootCvt = decltype(rb_root_cvt{std::declval<TDevice>()});
    using Kernel = decltype(std::declval<TCreator>().create(std::declval<RootCvt>()));
    using type = typename Kernel::internal_type;
};

template <io_device TDevice, cvt_creator TCreator>
using ext_to_int = typename ext_to_int_helper<TDevice, TCreator>::type;
}