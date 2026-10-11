// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

// compose_read / compose_write: a composite's iterator-form members reading and writing their
// components without spelling io_traits<TChar, T>.
//
// The composite is a point written as "(x, y)". Its swrite hands compose_write string literals,
// which only work because the key is the decayed type -- io_traits has no specialization for
// const char[N]. Its sread skips whitespace between components with compose_read(..., ws), since
// a component's sread does not skip leading whitespace. The width case pins the documented
// caveat: the first component that pads takes the width, not the point as a whole.
#include <IOv2/device/mem_device.h>
#include <IOv2/facet/ctype.h>
#include <IOv2/io/io_manip.h>
#include <IOv2/io/istream.h>
#include <IOv2/io/ostream.h>
#include <IOv2/io/traits/arithmetic.h>
#include <IOv2/io/traits/char_and_str.h>
#include <IOv2/io/traits/traits_base.h>
#include <IOv2/locale/locale.h>

#include <gtest/gtest.h>

#include <iterator>
#include <string>
#include <type_traits>

using namespace IOv2;

namespace
{
struct point
{
    int x = 0;
    int y = 0;
};
}

namespace IOv2
{
template <typename TChar>
struct io_traits<TChar, point>
{
    template <typename TIter>
        requires (char_sink_for<TIter, TChar>)
    static TIter swrite(TIter it, ios_base<TChar>& io, const locale<TChar>& loc, const point& p)
    {
        it = compose_write(it, io, loc, "(");
        it = compose_write(it, io, loc, p.x);
        it = compose_write(it, io, loc, ", ");
        it = compose_write(it, io, loc, p.y);
        return compose_write(it, io, loc, ")");
    }

    template <typename TIter, std::sentinel_for<TIter> TSent>
        requires (std::is_same_v<TChar, typename TIter::value_type>)
    static TIter sread(TIter it, TSent end, ios_base<TChar>& io, const locale<TChar>& loc,
                       point& p)
    {
        auto ct = loc.template get<ctype<TChar>>();
        if (!ct)
            throw stream_error("cannot get ctype facet");

        auto expect = [&](char c) {
            if (it == end || *it != ct->widen(c))
                throw stream_error("point: unexpected character");
            ++it;
        };

        point tmp;
        expect('(');
        it = compose_read(it, end, io, loc, ws);
        it = compose_read(it, end, io, loc, tmp.x);
        it = compose_read(it, end, io, loc, ws);
        expect(',');
        it = compose_read(it, end, io, loc, ws);
        it = compose_read(it, end, io, loc, tmp.y);
        it = compose_read(it, end, io, loc, ws);
        expect(')');

        p = tmp;
        return it;
    }
};
}

TEST(IoTraitsCompose, ACompositeWritesItsComponentsLiteralsIncluded)
{
    {
        ostream os{mem_device{std::string{}}, locale<char>("C")};
        os << point{3, -4};
        EXPECT_TRUE(os.good());
        EXPECT_EQ(os.device().str(), "(3, -4)");
    }
    {
        // A narrow literal into a wide stream is widened, as with wos << "...".
        ostream os{mem_device{std::wstring{}}, locale<wchar_t>("C")};
        os << point{3, -4};
        EXPECT_TRUE(os.good());
        EXPECT_EQ(os.device().str(), L"(3, -4)");
    }
}

TEST(IoTraitsCompose, ACompositeReadsBackWhatItWrote)
{
    {
        ostream os{mem_device{std::string{}}, locale<char>("C")};
        os << point{12, 34} << ' ' << point{-5, 6};

        istream is{mem_device{os.device().str()}, locale<char>("C")};
        point a;
        point b;
        is >> a >> b;
        EXPECT_TRUE(static_cast<bool>(is));
        EXPECT_EQ(a.x, 12);
        EXPECT_EQ(a.y, 34);
        EXPECT_EQ(b.x, -5);
        EXPECT_EQ(b.y, 6);
    }
    {
        istream is{mem_device{std::wstring(L" (  7 ,8)")}, locale<wchar_t>("C")};
        point p;
        is >> p;
        EXPECT_TRUE(static_cast<bool>(is));
        EXPECT_EQ(p.x, 7);
        EXPECT_EQ(p.y, 8);
    }
}

// The documented caveat: each component's swrite pads to width() and resets it, so the width
// lands on the first component -- here the "(" literal -- and not on the point as a whole.
TEST(IoTraitsCompose, TheWidthGoesToTheFirstComponentThatPads)
{
    ostream os{mem_device{std::string{}}, locale<char>("C")};
    os << setw(6) << point{1, 2};
    EXPECT_EQ(os.device().str(), "     (1, 2)");
    EXPECT_EQ(os.width(), 0);
}
