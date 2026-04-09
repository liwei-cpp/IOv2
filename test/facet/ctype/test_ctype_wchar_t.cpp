#include <facet/ctype.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <common/dump_info.h>

void test_ctype_facet_wchar_t_1()
{
    dump_info("Test ctype<wchar_t> 1...");
    static_assert(std::is_same_v<IOv2::ctype<wchar_t>::char_type, wchar_t>);
    
    if (IOv2::ctype_conf<wchar_t>::id() == IOv2::ctype_conf<char>::id())
        throw std::runtime_error("ctype id duplicate.");

    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_2()
{
    dump_info("Test ctype<wchar_t> 2...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));
    
    const wchar_t strlit00[] = L"manilla, cebu, tandag PHILIPPINES";
    const wchar_t strlit01[] = L"MANILLA, CEBU, TANDAG PHILIPPINES";
    const wchar_t c00 = L'S';
    const wchar_t c10 = L's';
    const wchar_t c20 = L'9';
    const wchar_t c30 = L' ';
    const wchar_t c40 = L'!';
    const wchar_t c50 = L'F';
    const wchar_t c60 = L'f';
    const wchar_t c80 = L'x';
 
    if (!obj.is_any(std::ctype_base::space, c30)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::upper, c00)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::lower, c10)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::digit, c20)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::punct, c40)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::alpha, c50)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::alpha, c60)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::xdigit, c20)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (obj.is_any(std::ctype_base::xdigit, c80)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::alnum, c50)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::alnum, c20)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::graph, c40)) throw std::runtime_error("ctype<wchar_t> is_any fails");
    if (!obj.is_any(std::ctype_base::graph, c20)) throw std::runtime_error("ctype<wchar_t> is_any fails");

    IOv2::base_ft<IOv2::ctype>::mask m00 = static_cast<IOv2::base_ft<IOv2::ctype>::mask>(0);
    IOv2::base_ft<IOv2::ctype>::mask m01[3];
    IOv2::base_ft<IOv2::ctype>::mask m02[13];
    const wchar_t* cc0 = strlit00;
    
    cc0 = strlit00;
    for (std::size_t i = 0; i < 3; ++i)
        m01[i] = m00;
    if (obj.is_seq(cc0, cc0, m01) != m01) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if ((m01[0] != m00) || (m01[1] != m00) || (m01[2] != m00)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");

    cc0 = strlit00;
    for (std::size_t i = 0; i < 3; ++i)
        m01[i] = m00;
    if (obj.is_seq(cc0, cc0 + 3, m01) != m01 + 3) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if ((m01[0] == m00) || (m01[1] == m00) || (m01[2] == m00)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if (!obj.is_any(m01[0], cc0[0])) throw std::runtime_error("ctype<wchar_t> is_any fails.");
    if (!obj.is_any(m01[1], cc0[1])) throw std::runtime_error("ctype<wchar_t> is_any fails.");
    if (!obj.is_any(m01[2], cc0[2])) throw std::runtime_error("ctype<wchar_t> is_any fails.");

    cc0 = strlit01;
    for (std::size_t i = 0; i < 13; ++i)
        m02[i] = m00;
    if (obj.is_seq(cc0, cc0 + 13, m02) != m02 + 13) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if ((m02[6] == m00) || (m02[7] == m00) || (m02[8] == m00)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if ((m02[8] == m02[6]) || (m02[7] == m02[6])) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if (!(m02[6] & IOv2::base_ft<IOv2::ctype>::alnum)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if (!(m02[6] & IOv2::base_ft<IOv2::ctype>::upper)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if (!(m02[6] & IOv2::base_ft<IOv2::ctype>::alpha)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if (!(m02[7] & IOv2::base_ft<IOv2::ctype>::punct)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    if (!(m02[8] & IOv2::base_ft<IOv2::ctype>::space)) throw std::runtime_error("ctype<wchar_t> is_seq fails.");
    
    if (!obj.is_any(m02[6], cc0[6])) throw std::runtime_error("ctype<wchar_t> is_any fails.");
    if (!obj.is_any(m02[7], cc0[7])) throw std::runtime_error("ctype<wchar_t> is_any fails.");
    if (!obj.is_any(m02[8], cc0[8])) throw std::runtime_error("ctype<wchar_t> is_any fails.");

    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_3()
{
    dump_info("Test ctype<wchar_t> 3...");
    const IOv2::ctype<wchar_t> obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));
    
    const wchar_t str[] =
        L"Is this the real life?\n"
        L"Is this just fantasy?\n"
        L"Caught in a landslide\n"
        L"No escape from reality\n"
        L"Open your eyes\n"
        L"Look up to the skies and see\n"
        L"I'm just a poor boy\n"
        L"I need no sympathy\n"
        L"Because I'm easy come, easy go\n"
        L"Little high, little low"
        L"Anyway the wind blows\n"
        L"Doesn't really matter to me\n"
        L"To me\n"
        L"                      -- Queen\n";
    
    const size_t len = sizeof(str) / sizeof(str[0]) - 1;
  
    const IOv2::base_ft<IOv2::ctype>::mask masks[] = {
          IOv2::base_ft<IOv2::ctype>::space, IOv2::base_ft<IOv2::ctype>::print, IOv2::base_ft<IOv2::ctype>::cntrl,
          IOv2::base_ft<IOv2::ctype>::upper, IOv2::base_ft<IOv2::ctype>::lower, IOv2::base_ft<IOv2::ctype>::alpha,
          IOv2::base_ft<IOv2::ctype>::digit, IOv2::base_ft<IOv2::ctype>::punct, IOv2::base_ft<IOv2::ctype>::xdigit,
          IOv2::base_ft<IOv2::ctype>::alnum, IOv2::base_ft<IOv2::ctype>::graph
    };

    const size_t num_masks = sizeof(masks) / sizeof(masks[0]);

    for (size_t i = 0; i < len; ++i)
    {
        for (size_t j = 0; j < num_masks; ++j)
        {
            for (size_t k = 0; k < num_masks; ++k)
            {
                bool r1 = obj.is_any(masks[j] | masks[k], str[i]);
                bool r2 = obj.is_any(masks[j], str[i]);
                bool r3 = obj.is_any(masks[k], str[i]);
                
                if (r1 != (r2 || r3)) throw std::runtime_error("ctype<wchar_t> is_any fails.");
            }
        }
    }
    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_4()
{
    dump_info("Test ctype<wchar_t> 4...");
    const IOv2::ctype<wchar_t> obj1(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));
    const IOv2::ctype<wchar_t> obj2(std::make_shared<IOv2::ctype_conf<wchar_t>>("de_DE.UTF-8"));
    
    std::vector<IOv2::base_ft<IOv2::ctype>::mask> v_c(256);
    std::vector<IOv2::base_ft<IOv2::ctype>::mask> v_de(256);
    
    for (int i = 0; i < 256; ++i)
    {
        v_c[i] = obj1.is(static_cast<wchar_t>(i));
        v_de[i] = obj2.is(static_cast<wchar_t>(i));
    }
    
    if (v_c == v_de) throw std::runtime_error("ctype<wchar_t> is fails.");
    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_5()
{
    dump_info("Test ctype<wchar_t> 5...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));
    
    constexpr char dfault = '?';
    std::basic_string<wchar_t>  wide(L"wibble");
    std::basic_string<char>     narrow("wibble");
    std::vector<char>           narrow_chars;
    narrow_chars.reserve(wide.size());
  
    // narrow(charT c, char dfault) const
    for (size_t i = 0; i < wide.length(); ++i)
    {
      char c = obj.narrow(wide[i], dfault);
      if (c != narrow[i]) throw std::runtime_error("ctype<wchar_t> narrow fails.");
    }

    obj.narrow_seq(wide.begin(), wide.end(), dfault, std::back_inserter(narrow_chars));
    if (narrow_chars.size() != wide.size()) throw std::runtime_error("ctype<wchar_t> narrow fails.");
    for (size_t i = 0; i < wide.size(); ++i)
        if (narrow_chars[i] != narrow[i]) throw std::runtime_error("ctype<wchar_t> narrow fails.");

    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_6()
{
    dump_info("Test ctype<wchar_t> 6...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));
    
    constexpr char dfault = '?';
    std::basic_string<wchar_t>  wide(L"wibble");
    wide += static_cast<wchar_t>(1240);
    wide += L"kibble";
    
    std::basic_string<char>     narrow("wibble");
    narrow += dfault;
    narrow += "kibble";
    
    std::vector<char>           narrow_chars;
    narrow_chars.reserve(wide.size());

    // narrow(charT c, char dfault) const
    for (size_t i = 0; i < wide.length(); ++i)
    {
        char c = obj.narrow(wide[i], dfault);
        if (c != narrow[i]) throw std::runtime_error("ctype<wchar_t> narrow fails.");
    }

    obj.narrow_seq(wide.begin(), wide.end(), dfault, std::back_inserter(narrow_chars));
    if (narrow_chars.size() != wide.size()) throw std::runtime_error("ctype<wchar_t> narrow fails.");
    for (size_t i = 0; i < wide.size(); ++i)
        if (narrow_chars[i] != narrow[i]) throw std::runtime_error("ctype<wchar_t> narrow fails.");
    
    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_7()
{
    dump_info("Test ctype<wchar_t> 7...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("se_NO.UTF-8"));
    
    const wchar_t* wstrlit = L"\x80";

    char buf[2];
    obj.narrow_seq(wstrlit, wstrlit + 2, ' ', buf);
    if (buf[0] != obj.narrow(wstrlit[0], ' ')) throw std::runtime_error("ctype<wchar_t> narrow fails.");
    if (buf[1] != obj.narrow(wstrlit[1], ' ')) throw std::runtime_error("ctype<wchar_t> narrow fails.");
    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_8()
{
    dump_info("Test ctype<wchar_t> 8...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));
    
    const auto *const ca = L"aaaaa";
    const auto *const cz = L"zzzzz";
    const auto *const cA = L"AAAAA";
    const auto *const cZ = L"ZZZZZ";
    const auto *const c0 = L"00000";
    const auto *const c9 = L"99999";
    const auto *const cs = L"     ";
    const auto *const xf = L"fffff";
    const auto *const xF = L"FFFFF";
    const auto *const p1 = L"!!!!!";
    const auto *const p2 = L"/////";
    
    auto _is = [&obj](IOv2::base_ft<IOv2::ctype>::mask m, const wchar_t *const b, const wchar_t *const e)
    {
        if (obj.scan_is_any(m, b, e) != b) throw std::runtime_error("scan_is_any fail.");
        if (obj.scan_not_any(m, b, e) != e) throw std::runtime_error("scan_not_any fail.");
    };
    
    auto _not = [&obj](IOv2::base_ft<IOv2::ctype>::mask m, const wchar_t *const b, const wchar_t *const e)
    {
        if (obj.scan_is_any(m, b, e) != e) throw std::runtime_error("scan_is_any fail.");
        if (obj.scan_not_any(m, b, e) != b) throw std::runtime_error("scan_not_any fail.");
    };
    
    // 'a'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, ca, ca + 5);
    _is(IOv2::base_ft<IOv2::ctype>::alpha, ca, ca + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, ca, ca + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, ca, ca + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, ca, ca + 5);
    _is(IOv2::base_ft<IOv2::ctype>::lower, ca, ca + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, ca, ca + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, ca, ca + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, ca, ca + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, ca, ca + 5);
    _is(IOv2::base_ft<IOv2::ctype>::xdigit, ca, ca + 5);

    // 'z'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, cz, cz + 5);
    _is(IOv2::base_ft<IOv2::ctype>::alpha, cz, cz + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, cz, cz + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, cz, cz + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, cz, cz + 5);
    _is(IOv2::base_ft<IOv2::ctype>::lower, cz, cz + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, cz, cz + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, cz, cz + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, cz, cz + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, cz, cz + 5);
    _not(IOv2::base_ft<IOv2::ctype>::xdigit, cz, cz + 5);
    
    // 'A'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, cA, cA + 5);
    _is(IOv2::base_ft<IOv2::ctype>::alpha, cA, cA + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, cA, cA + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, cA, cA + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, cA, cA + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, cA, cA + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, cA, cA + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, cA, cA + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, cA, cA + 5);
    _is(IOv2::base_ft<IOv2::ctype>::upper, cA, cA + 5);
    _is(IOv2::base_ft<IOv2::ctype>::xdigit, cA, cA + 5);

    // 'Z'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, cZ, cZ + 5);
    _is(IOv2::base_ft<IOv2::ctype>::alpha, cZ, cZ + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, cZ, cZ + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, cZ, cZ + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, cZ, cZ + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, cZ, cZ + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, cZ, cZ + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, cZ, cZ + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, cZ, cZ + 5);
    _is(IOv2::base_ft<IOv2::ctype>::upper, cZ, cZ + 5);
    _not(IOv2::base_ft<IOv2::ctype>::xdigit, cZ, cZ + 5);
    
    // '0'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, c0, c0 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::alpha, c0, c0 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, c0, c0 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::digit, c0, c0 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, c0, c0 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, c0, c0 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, c0, c0 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, c0, c0 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, c0, c0 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, c0, c0 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::xdigit, c0, c0 + 5);
    
    // '9'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, c9, c9 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::alpha, c9, c9 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, c9, c9 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::digit, c9, c9 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, c9, c9 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, c9, c9 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, c9, c9 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, c9, c9 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, c9, c9 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, c9, c9 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::xdigit, c9, c9 + 5);
    
    // ' '
    _not(IOv2::base_ft<IOv2::ctype>::alnum, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::alpha, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::graph, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, cs, cs + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, cs, cs + 5);
    _is(IOv2::base_ft<IOv2::ctype>::space, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, cs, cs + 5);
    _not(IOv2::base_ft<IOv2::ctype>::xdigit, cs, cs + 5);
    
    // 'f'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, xf, xf + 5);
    _is(IOv2::base_ft<IOv2::ctype>::alpha, xf, xf + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, xf, xf + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, xf, xf + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, xf, xf + 5);
    _is(IOv2::base_ft<IOv2::ctype>::lower, xf, xf + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, xf, xf + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, xf, xf + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, xf, xf + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, xf, xf + 5);
    _is(IOv2::base_ft<IOv2::ctype>::xdigit, xf, xf + 5);
    
    // 'F'
    _is(IOv2::base_ft<IOv2::ctype>::alnum, xF, xF + 5);
    _is(IOv2::base_ft<IOv2::ctype>::alpha, xF, xF + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, xF, xF + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, xF, xF + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, xF, xF + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, xF, xF + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, xF, xF + 5);
    _not(IOv2::base_ft<IOv2::ctype>::punct, xF, xF + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, xF, xF + 5);
    _is(IOv2::base_ft<IOv2::ctype>::upper, xF, xF + 5);
    _is(IOv2::base_ft<IOv2::ctype>::xdigit, xF, xF + 5);

    // '!'
    _not(IOv2::base_ft<IOv2::ctype>::alnum, p1, p1 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::alpha, p1, p1 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, p1, p1 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, p1, p1 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, p1, p1 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, p1, p1 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, p1, p1 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::punct, p1, p1 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, p1, p1 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, p1, p1 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::xdigit, p1, p1 + 5);

    // '/'
    _not(IOv2::base_ft<IOv2::ctype>::alnum, p2, p2 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::alpha, p2, p2 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::cntrl, p2, p2 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::digit, p2, p2 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::graph, p2, p2 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::lower, p2, p2 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::print, p2, p2 + 5);
    _is(IOv2::base_ft<IOv2::ctype>::punct, p2, p2 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::space, p2, p2 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::upper, p2, p2 + 5);
    _not(IOv2::base_ft<IOv2::ctype>::xdigit, p2, p2 + 5);

    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_9()
{
    dump_info("Test ctype<wchar_t> 9...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("C"));
    
    const wchar_t strlit00[] = L"manilla, cebu, tandag PHILIPPINES";
    const wchar_t strlit01[] = L"MANILLA, CEBU, TANDAG PHILIPPINES";
    const wchar_t strlit02[] = L"manilla, cebu, tandag philippines";
    const auto c00 = L'S';
    const auto c10 = L's';

    int len = std::char_traits<wchar_t>::length(strlit00);
    std::vector<wchar_t> c_array(len + 1);
    
    // sanity check ctype_base::mask members
    int i01 = IOv2::base_ft<IOv2::ctype>::space;
    int i02 = IOv2::base_ft<IOv2::ctype>::upper;
    int i03 = IOv2::base_ft<IOv2::ctype>::lower;
    int i04 = IOv2::base_ft<IOv2::ctype>::digit;
    int i05 = IOv2::base_ft<IOv2::ctype>::punct;
    int i06 = IOv2::base_ft<IOv2::ctype>::alpha;
    int i07 = IOv2::base_ft<IOv2::ctype>::xdigit;
    int i08 = IOv2::base_ft<IOv2::ctype>::alnum;
    int i09 = IOv2::base_ft<IOv2::ctype>::graph;
    int i10 = IOv2::base_ft<IOv2::ctype>::print;
    int i11 = IOv2::base_ft<IOv2::ctype>::cntrl;
    if (i01 == i02) throw std::runtime_error("mask duplicate");
    if (i02 == i03) throw std::runtime_error("mask duplicate");
    if (i03 == i04) throw std::runtime_error("mask duplicate");
    if (i04 == i05) throw std::runtime_error("mask duplicate");
    if (i05 == i06) throw std::runtime_error("mask duplicate");
    if (i06 == i07) throw std::runtime_error("mask duplicate");
    if (i07 == i08) throw std::runtime_error("mask duplicate");
    if (i08 == i09) throw std::runtime_error("mask duplicate");
    if (i09 == i10) throw std::runtime_error("mask duplicate");
    if (i10 == i11) throw std::runtime_error("mask duplicate");
    if (i11 == i01) throw std::runtime_error("mask duplicate");

    // char_type toupper(char_type c) const
    if (obj.toupper(c10) != c00) throw std::runtime_error("ctype<wchar_t> toupper fails");

    // char_type tolower(char_type c) const
    if (obj.tolower(c00) != c10) throw std::runtime_error("ctype<wchar_t> tolower fails");

    // char_type toupper_seq(char_type* low, const char_type* hi) const
    obj.toupper_seq(strlit02, strlit02 + len, c_array.data());
    if (std::char_traits<wchar_t>::compare(c_array.data(), strlit01, len - 1) != 0)
        throw std::runtime_error("ctype<wchar_t> toupper_seq fails");
        
    // char_type tolower_seq(char_type* low, const char_type* hi) const
    obj.tolower_seq(strlit01, strlit01 + len, c_array.data());
    if (std::char_traits<wchar_t>::compare(c_array.data(), strlit02, len - 1) != 0)
        throw std::runtime_error("ctype<wchar_t> tolower_seq fails");
        
    dump_info("Done\n");
}

void test_ctype_facet_wchar_t_10()
{
    dump_info("Test ctype<wchar_t> 10...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<wchar_t>>("zh_CN.UTF-8"));
    
    if (obj.tolower(L'Ａ') != L'ａ') throw std::runtime_error("ctype<wchar_t> tolower fails");
    if (obj.toupper(L'ａ') != L'Ａ') throw std::runtime_error("ctype<wchar_t> toupper fails");

    dump_info("Done\n");
}