#pragma once
#include <cvt/cvt_concepts.h>

#include <utility>

namespace IOv2
{
template <io_converter TRootCvt, cvt_creator TCreator>
using ext_to_int =
    typename decltype(std::declval<TCreator>().create(std::declval<TRootCvt>()))::internal_type;
}
