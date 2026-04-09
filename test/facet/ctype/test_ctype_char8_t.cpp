#include <facet/ctype.h>
#include <ios>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <common/dump_info.h>
namespace
{
    constexpr auto charLen = 256;
}

void test_ctype_facet_char8_t_1()
{
    dump_info("Test ctype<char8_t> 1...");
    static_assert(std::is_same_v<IOv2::ctype<char8_t>::char_type, char8_t>);

    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<char8_t>>("en_US.UTF-8"));
    const IOv2::ctype<char> ref(std::make_shared<IOv2::ctype_conf<char>>("en_US.UTF-8"));
    
    char8_t chs[charLen];
    for (int i = 0; i < charLen; ++i) chs[i] = static_cast<char8_t>(i);
    
    std::ctype_base::mask mask_ref[charLen] = {static_cast<std::ctype_base::mask>(0)};
    ref.is_seq(chs + 0, chs + 256, mask_ref);
    
    for (int i = 0; i < charLen; ++i)
    {
        if (i < 128)
        {
            if (obj.is(i) != mask_ref[i])
                throw std::runtime_error("ctype::is result error.");

            if (obj.toupper(i) != ref.toupper(static_cast<char>(i)))
                throw std::runtime_error("ctype::toupper result error.");
            
            if (obj.tolower(i) != ref.tolower(static_cast<char>(i)))
                throw std::runtime_error("ctype::tolower result error.");
                
            if (obj.widen(i) != ref.widen(static_cast<char>(i)))
                throw std::runtime_error("ctype::widen result error.");
                
            if (obj.narrow(i, 0) != ref.narrow(static_cast<char>(i), 0))
                throw std::runtime_error("ctype::narrow result error.");
        }
        else
        {
            if (obj.is(i) != 0)
                throw std::runtime_error("ctype::is result error.");

            if (obj.toupper(i) != i)
                throw std::runtime_error("ctype::toupper result error.");

            if (obj.tolower(i) != i)
                throw std::runtime_error("ctype::tolower result error.");
                
            if (obj.widen(i) != i)
                throw std::runtime_error("ctype::widen result error.");
                
            if (obj.narrow(i, 0) != 0)
                throw std::runtime_error("ctype::narrow result error.");
        }
    }

    dump_info("Done\n");
}

void test_ctype_facet_char8_t_2()
{
    dump_info("Test ctype<char8_t> 2...");
    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<char8_t>>("en_US.UTF-8"));

    char8_t chs[charLen];
    for (int i = 0; i < charLen; ++i) chs[i] = static_cast<char8_t>(i);
    
    IOv2::base_ft<IOv2::ctype>::mask mask_res[charLen];
    auto mask_ptr = obj.is_seq(chs, chs + charLen, mask_res);
    if (mask_ptr != mask_res + charLen)
        throw std::runtime_error("ctype<char8_t>::is_seq fail, result number mismatch.");
    for (int i = 0; i < charLen; ++i)
        if (obj.is(static_cast<char8_t>(i)) != mask_res[i])
            throw std::runtime_error("ctype<char8_t>::is_seq fail, incorrect result");
            
    char8_t uchs[charLen];
    auto uchs_ptr = obj.toupper_seq(chs, chs + charLen, uchs);
    if (uchs_ptr != uchs + charLen)
        throw std::runtime_error("ctype<char8_t>::toupper_seq fail, result number mismatch.");
    for (int i = 0; i < charLen; ++i)
        if (obj.toupper(static_cast<char8_t>(i)) != uchs[i])
            throw std::runtime_error("ctype<char8_t>::toupper_seq fail, incorrect result");
            
    char8_t lchs[charLen];
    auto lchs_ptr = obj.tolower_seq(chs, chs + charLen, lchs);
    if (lchs_ptr != lchs + charLen)
        throw std::runtime_error("ctype<char8_t>::tolower_seq fail, result number mismatch.");
    for (int i = 0; i < charLen; ++i)
        if (obj.tolower(static_cast<char8_t>(i)) != lchs[i])
            throw std::runtime_error("ctype<char8_t>::tolower_seq fail, incorrect result");
            
    char8_t wchs[charLen];
    auto wchs_ptr = obj.widen_seq(chs, chs + charLen, wchs);
    if (wchs_ptr != wchs + charLen)
        throw std::runtime_error("ctype<char8_t>::widen_seq fail, result number mismatch.");
    for (int i = 0; i < charLen; ++i)
        if (obj.widen(static_cast<char8_t>(i)) != wchs[i])
            throw std::runtime_error("ctype<char8_t>::widen_seq fail, incorrect result");

    char8_t nchs[charLen];
    auto nchs_ptr = obj.narrow_seq(chs, chs + charLen, 0, nchs);
    if (nchs_ptr != nchs + charLen)
        throw std::runtime_error("ctype<char8_t>::narrow_seq fail, result number mismatch.");
    for (int i = 0; i < charLen; ++i)
    {
        if (i < 128)
        {
            if (obj.widen(static_cast<char8_t>(i)) != nchs[i])
                throw std::runtime_error("ctype<char8_t>::narrow_seq fail, incorrect result");
        }
        else
        {
            if (0 != nchs[i])
                throw std::runtime_error("ctype<char8_t>::narrow_seq fail, incorrect result");
        }
    }
    dump_info("Done\n");
}

void test_ctype_facet_char8_t_3()
{
    dump_info("Test ctype<char8_t> 3...");
    const char8_t *const ca = u8"aaaaa";
    const char8_t *const cz = u8"zzzzz";
    const char8_t *const cA = u8"AAAAA";
    const char8_t *const cZ = u8"ZZZZZ";
    const char8_t *const c0 = u8"00000";
    const char8_t *const c9 = u8"99999";
    const char8_t *const cs = u8"     ";
    const char8_t *const xf = u8"fffff";
    const char8_t *const xF = u8"FFFFF";
    const char8_t *const p1 = u8"!!!!!";
    const char8_t *const p2 = u8"/////";

    const IOv2::ctype obj(std::make_shared<IOv2::ctype_conf<char8_t>>("C"));

    auto _is = [&obj](IOv2::base_ft<IOv2::ctype>::mask m, const char8_t *const b, const char8_t *const e)
    {
        if (obj.scan_is_any(m, b, e) != b) throw std::runtime_error("scan_is_any fail.");
        if (obj.scan_not_any(m, b, e) != e) throw std::runtime_error("scan_not_any fail.");
    };
    
    auto _not = [&obj](IOv2::base_ft<IOv2::ctype>::mask m, const char8_t *const b, const char8_t *const e)
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
