#pragma once

#include <iterator>

#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/streambuf.h>
#include <io/streambuf_iterator.h>
#include <io/traits/traits_base.h>
#include <locale/locale.h>

// One io_traits<TChar, TValue> carries both directions, so the class existing says nothing
// about which direction is available: io_traits<TChar, nullptr_t> is write-only,
// io_traits<TChar, TChar[N]> is read-only, and the arithmetic specialization drops sread for
// signed char / unsigned char. These probe swrite / sread themselves, against the same kind of
// iterator the generic operator<< / operator>> hand them.
//
// Do not substitute `requires { os << v; }` / `requires { is >> v; }`. Those are valid probes now
// that the operators are constrained, but they answer a broader question: they also fold in value
// category, the insertion side's decay rung, the parse-context rung and the function-pointer
// manipulator overload. Asking io_traits directly keeps a failure pointing at the extension point
// rather than at the layers above it.
//
// These probe the iterator form only -- the form the generic operators use for formatted I/O.
// Manipulators go through the stream form instead and are not covered here.

template <typename TChar>
using probe_out_iter = IOv2::ostreambuf_iterator<IOv2::ostreambuf<IOv2::mem_device<TChar>, TChar>>;

template <typename TChar>
using probe_in_iter = IOv2::istreambuf_iterator<IOv2::istreambuf<IOv2::mem_device<TChar>, TChar>>;

template <typename TChar, typename TValue>
concept insertable = requires (probe_out_iter<TChar> it, IOv2::ios_base<TChar>& io,
                               const IOv2::locale<TChar>& loc, const TValue& v)
{
    IOv2::io_traits<TChar, TValue>::swrite(it, io, loc, v);
};

template <typename TChar, typename TValue>
concept extractable = requires (probe_in_iter<TChar> it, IOv2::ios_base<TChar>& io,
                                const IOv2::locale<TChar>& loc, TValue& v)
{
    IOv2::io_traits<TChar, TValue>::sread(it, std::default_sentinel, io, loc, v);
};
