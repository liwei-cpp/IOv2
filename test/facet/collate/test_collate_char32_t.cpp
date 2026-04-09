#include <deque>
#include <list>
#include <stdexcept>
#include <vector>
#include <facet/collate.h>

#include <common/dump_info.h>

void test_collate_char32_t_compare_1()
{
    dump_info("Test collate<char32_t> compare 1...");
    const IOv2::collate obj(std::make_shared<IOv2::collate_conf<char32_t>>("C"));
    
    char32_t strlit1[] = U"monkey picked tikuanyin oolong";
    char32_t strlit2[] = U"imperial tea court green oolong";
    const size_t size1 = std::size(strlit1);
    const size_t size2 = std::size(strlit2);

    { // compare with const char as input
        auto i1 = obj.compare(strlit1, strlit1 + size1, strlit1, strlit1 + 7);
        if (i1 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(strlit1, strlit1 + 7, strlit1, strlit1 + size1);
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(strlit1, strlit1 + 7, strlit1, strlit1 + 7);
        if (i1 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
        auto i2 = obj.compare(strlit2, strlit2 + size2, strlit2, strlit2 + 13);
        if (i2 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(strlit2, strlit2 + 13, strlit2, strlit2 + size2);
        if (i2 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(strlit2, strlit2 + size2, strlit2, strlit2 + size2);
        if (i2 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
    }
    
    { // compare with const char & list as input
        std::list<char32_t> lstLit1{strlit1, strlit1 + 7};
        std::list<char32_t> lstLit2{strlit2, strlit2 + size2};
        
        auto i1 = obj.compare(strlit1, strlit1 + size1, lstLit1.begin(), lstLit1.end());
        if (i1 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1.end(), strlit1, strlit1 + size1);
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1.end(), lstLit1.begin(), lstLit1.end());
        if (i1 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
        auto i2 = obj.compare(lstLit2.begin(), lstLit2.end(), strlit2, strlit2 + 13);
        if (i2 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(strlit2, strlit2 + 13, lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(lstLit2.begin(), lstLit2.end(), lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
    }
    
    { // compare with const list as input
        std::list<char32_t> lstLit1{strlit1, strlit1 + size1};
        auto lstLit1Ptr = lstLit1.begin(); std::advance(lstLit1Ptr, 7);
        std::list<char32_t> lstLit2{strlit2, strlit2 + size2};
        auto lstLit2Ptr = lstLit2.begin(); std::advance(lstLit2Ptr, 13);
        auto i1 = obj.compare(lstLit1.begin(), lstLit1.end(), lstLit1.begin(), lstLit1Ptr);
        if (i1 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1Ptr, lstLit1.begin(), lstLit1.end());
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1Ptr, lstLit1.begin(), lstLit1Ptr);
        if (i1 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
        auto i2 = obj.compare(lstLit2.begin(), lstLit2.end(), lstLit2.begin(), lstLit2Ptr);
        if (i2 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(lstLit2.begin(), lstLit2Ptr, lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(lstLit2.begin(), lstLit2.end(), lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
    }
    dump_info("Done\n");
}

void test_collate_char32_t_compare_2()
{
    dump_info("Test collate<char32_t> compare 2...");
    const IOv2::collate obj(std::make_shared<IOv2::collate_conf<char32_t>>("de_DE.UTF-8"));
    
    const char32_t strlit3[] = U"Äuglein Augment"; // "C" == "Augment Äuglein"
    const char32_t strlit4[] = U"Base baß Baß Bast"; // "C" == "Base baß Baß Bast"
    size_t size3 = std::size(strlit3);
    size_t size4 = std::size(strlit4);
    {  // compare with const char as input
        auto i1 = obj.compare(strlit3, strlit3 + size3, strlit3, strlit3 + 7);
        if (i1 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(strlit3, strlit3 + 7, strlit3, strlit3 + size3);
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(strlit3, strlit3 + 7, strlit3, strlit3 + 7);
        if (i1 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(strlit3, strlit3 + 6, strlit3 + 8, strlit3 + 14);
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        auto i2 = obj.compare(strlit4, strlit4 + size4, strlit4, strlit4 + 13);
        if (i2 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(strlit4, strlit4 + 13, strlit4, strlit4 + size4);
        if (i2 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(strlit4, strlit4 + size4, strlit4, strlit4 + size4);
        if (i2 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
    }
    
    {  // compare with const char & list as input
        std::list<char32_t> lstLit1{strlit3, strlit3 + 7};
        std::list<char32_t> lstLit2{strlit4, strlit4 + size4};
        auto i1 = obj.compare(strlit3, strlit3 + size3, lstLit1.begin(), lstLit1.end());
        if (i1 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1.end(), strlit3, strlit3 + size3);
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1.end(), lstLit1.begin(), lstLit1.end());
        if (i1 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
        auto i2 = obj.compare(lstLit2.begin(), lstLit2.end(), strlit4, strlit4 + 13);
        if (i2 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(strlit4, strlit4 + 13, lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(lstLit2.begin(), lstLit2.end(), lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
    }
    
    {  // compare with list as input
        std::list<char32_t> lstLit1{strlit3, strlit3 + size3};
        std::list<char32_t> lstLit2{strlit4, strlit4 + size4};
        auto lstLit1_6 = lstLit1.begin(); std::advance(lstLit1_6, 6);
        auto lstLit1_7 = lstLit1.begin(); std::advance(lstLit1_7, 7);
        auto lstLit1_8 = lstLit1.begin(); std::advance(lstLit1_8, 8);
        auto lstLit1_E = lstLit1.begin(); std::advance(lstLit1_E, 14);
        auto lstLit2_D = lstLit2.begin(); std::advance(lstLit2_D, 13);
        
        auto i1 = obj.compare(lstLit1.begin(), lstLit1.end(), lstLit1.begin(), lstLit1_7);
        if (i1 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1_7, lstLit1.begin(), lstLit1.end());
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1_7, lstLit1.begin(), lstLit1_7);
        if (i1 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
        i1 = obj.compare(lstLit1.begin(), lstLit1_6, lstLit1_8, lstLit1_E);
        if (i1 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        auto i2 = obj.compare(lstLit2.begin(), lstLit2.end(), lstLit2.begin(), lstLit2_D);
        if (i2 != std::strong_ordering::greater) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(lstLit2.begin(), lstLit2_D, lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::less) throw std::runtime_error("collate::compare error");
        i2 = obj.compare(lstLit2.begin(), lstLit2.end(), lstLit2.begin(), lstLit2.end());
        if (i2 != std::strong_ordering::equal) throw std::runtime_error("collate::compare error");
    }

    dump_info("Done\n");
}

void test_collate_char32_t_compare_3()
{
    dump_info("Test collate<char32_t> compare 3...");
    const IOv2::collate obj_c(std::make_shared<IOv2::collate_conf<char32_t>>("C"));
    const IOv2::collate obj_de(std::make_shared<IOv2::collate_conf<char32_t>>("de_DE.UTF-8"));
    
    const char32_t strlit1[] = U"a\0a\0";
    const char32_t strlit2[] = U"a\0b\0";
    const char32_t strlit3[] = U"a\0\xc4\0";
    const char32_t strlit4[] = U"a\0B\0";
    const char32_t strlit5[] = U"aa\0";
    const char32_t strlit6[] = U"b\0a\0";
    
    {  // compare with const char as input
        if (obj_c.compare(strlit1, strlit1 + 3, strlit2, strlit2 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit1, strlit1 + 3, strlit2, strlit2 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit3, strlit3 + 3, strlit4, strlit4 + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit3, strlit3 + 3, strlit4, strlit4 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit1, strlit1 + 3, strlit1, strlit1 + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit3, strlit3 + 4, strlit3, strlit3 + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit1, strlit1 + 4, strlit4, strlit4 + 1) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit3, strlit3 + 3, strlit3, strlit3 + 3) != std::strong_ordering::equal)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit1, strlit1 + 2, strlit1, strlit1 + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit1, strlit1 + 3, strlit5, strlit5 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit6, strlit6 + 3, strlit1, strlit1 + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
    }

    {  // compare with deque as input
        std::deque<char32_t> deqlit1{std::begin(strlit1), std::end(strlit1)};
        std::deque<char32_t> deqlit2{std::begin(strlit2), std::end(strlit2)};
        std::deque<char32_t> deqlit3{std::begin(strlit3), std::end(strlit3)};
        std::deque<char32_t> deqlit4{std::begin(strlit4), std::end(strlit4)};
        std::deque<char32_t> deqlit5{std::begin(strlit5), std::end(strlit5)};
        std::deque<char32_t> deqlit6{std::begin(strlit6), std::end(strlit6)};
        
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 3, deqlit2.begin(), deqlit2.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit1.begin(), deqlit1.begin() + 3, deqlit2.begin(), deqlit2.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit3.begin(), deqlit3.begin() + 3, deqlit4.begin(), deqlit4.begin() + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit3.begin(), deqlit3.begin() + 3, deqlit4.begin(), deqlit4.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 3, deqlit1.begin(), deqlit1.begin() + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit3.begin(), deqlit3.begin() + 4, deqlit3.begin(), deqlit3.begin() + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 4, deqlit4.begin(), deqlit4.begin() + 1) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit3.begin(), deqlit3.begin() + 3, deqlit3.begin(), deqlit3.begin() + 3) != std::strong_ordering::equal)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 2, deqlit1.begin(), deqlit1.begin() + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit1.begin(), deqlit1.begin() + 3, deqlit5.begin(), deqlit5.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit6.begin(), deqlit6.begin() + 3, deqlit1.begin(), deqlit1.begin() + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
    }

    {  // compare with const char & deque as input
        std::deque<char32_t> deqlit1{std::begin(strlit1), std::end(strlit1)};
        std::deque<char32_t> deqlit2{std::begin(strlit2), std::end(strlit2)};
        std::deque<char32_t> deqlit3{std::begin(strlit3), std::end(strlit3)};
        std::deque<char32_t> deqlit4{std::begin(strlit4), std::end(strlit4)};
        std::deque<char32_t> deqlit5{std::begin(strlit5), std::end(strlit5)};
        std::deque<char32_t> deqlit6{std::begin(strlit6), std::end(strlit6)};
        
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 3, strlit2, strlit2 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit1, strlit1 + 3, deqlit2.begin(), deqlit2.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit1.begin(), deqlit1.begin() + 3, strlit2, strlit2 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit1, strlit1 + 3, deqlit2.begin(), deqlit2.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit3.begin(), deqlit3.begin() + 3, strlit4, strlit4 + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit3, strlit3 + 3, deqlit4.begin(), deqlit4.begin() + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit3.begin(), deqlit3.begin() + 3, strlit4, strlit4 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit3, strlit3 + 3, deqlit4.begin(), deqlit4.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 3, strlit1, strlit1 + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit1, strlit1 + 3, deqlit1.begin(), deqlit1.begin() + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit3.begin(), deqlit3.begin() + 4, strlit3, strlit3 + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit3, strlit3 + 4, deqlit3.begin(), deqlit3.begin() + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 4, strlit4, strlit4 + 1) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit1, strlit1 + 4, deqlit4.begin(), deqlit4.begin() + 1) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit3.begin(), deqlit3.begin() + 3, strlit3, strlit3 + 3) != std::strong_ordering::equal)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit3, strlit3 + 3, deqlit3.begin(), deqlit3.begin() + 3) != std::strong_ordering::equal)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit1.begin(), deqlit1.begin() + 2, strlit1, strlit1 + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit1, strlit1 + 2, deqlit1.begin(), deqlit1.begin() + 4) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(deqlit1.begin(), deqlit1.begin() + 3, strlit5, strlit5 + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_de.compare(strlit1, strlit1 + 3, deqlit5.begin(), deqlit5.begin() + 3) != std::strong_ordering::less)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(deqlit6.begin(), deqlit6.begin() + 3, strlit1, strlit1 + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
        if (obj_c.compare(strlit6, strlit6 + 3, deqlit1.begin(), deqlit1.begin() + 3) != std::strong_ordering::greater)
            throw std::runtime_error("collate::compare error");
    }
    
    dump_info("Done\n");
}

void test_collate_char32_t_transform_1()
{
    dump_info("Test collate<char32_t> transform 1...");
    const IOv2::collate obj(std::make_shared<IOv2::collate_conf<char32_t>>("de_DE.UTF-8"));
    
    const char32_t strlit3[] = U"Äuglein Augment"; // "C" == "Augment Äuglein"
    const char32_t strlit4[] = U"Base baß Baß Bast"; // "C" == "Base baß Baß Bast"
    
    auto len1 = obj.transform_length(std::begin(strlit3), std::end(strlit3));
    auto len2 = obj.transform_length(std::begin(strlit4), std::end(strlit4));
    
    {  // transform_length with list iterator as input
        std::list<char32_t> s1{std::begin(strlit3), std::end(strlit3)};
        std::list<char32_t> s2{std::begin(strlit4), std::end(strlit4)};
        
        if (obj.transform_length(s1.begin(), s1.end()) != len1)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj.transform_length(s2.begin(), s2.end()) != len2)
            throw std::runtime_error("collate::transform_length fail.");
    }
    
    std::u32string str_trans1, str_trans2;
    str_trans1.resize(len1); str_trans2.resize(len2);
    
    {   // transform using char array as input / output
        auto [it, s] = obj.transform(std::begin(strlit3), std::end(strlit3), str_trans1.data());
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (it != str_trans1.data() + len1) throw std::runtime_error("collate::transform fail.");
        
        std::tie(it, s) = obj.transform(std::begin(strlit4), std::end(strlit4), str_trans2.data());
        if (s != len2) throw std::runtime_error("collate::transform fail.");
        if (it != str_trans2.data() + len2) throw std::runtime_error("collate::transform fail.");
        
        if (str_trans1 >= str_trans2) throw std::runtime_error("collate::transform fail.");
    }

    {   // transform using list as input and char array as output
        std::list<char32_t> lstlit3{std::begin(strlit3), std::end(strlit3)};
        std::list<char32_t> lstlit4{std::begin(strlit4), std::end(strlit4)};
        std::u32string str_trans2_1, str_trans2_2;
        str_trans2_1.resize(len1); str_trans2_2.resize(len2);
        auto [it, s] = obj.transform(lstlit3.begin(), lstlit3.end(), str_trans2_1.data());
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (it != str_trans2_1.data() + len1) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_1 != str_trans1) throw std::runtime_error("collate::transform fail.");
            
        std::tie(it, s) = obj.transform(lstlit4.begin(), lstlit4.end(), str_trans2_2.data());
        if (s != len2) throw std::runtime_error("collate::transform fail.");
        if (it != str_trans2_2.data() + len2) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_2 != str_trans2) throw std::runtime_error("collate::transform fail.");
    }
    
    {   // transform char array as input and back_inserter as output
        std::u32string str_trans2_1, str_trans2_2;
        auto b1 = std::back_inserter(str_trans2_1);
        auto b2 = std::back_inserter(str_trans2_2);
        auto [it, s] = obj.transform(std::begin(strlit3), std::end(strlit3), b1);
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_1 != str_trans1) throw std::runtime_error("collate::transform fail.");
            
        std::tie(it, s) = obj.transform(std::begin(strlit4), std::end(strlit4), b2);
        if (s != len2) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_2 != str_trans2) throw std::runtime_error("collate::transform fail.");
    }
    
    {   // transform list as input and back_inserter as output
        std::list<char32_t> lstlit3{std::begin(strlit3), std::end(strlit3)};
        std::list<char32_t> lstlit4{std::begin(strlit4), std::end(strlit4)};
        std::u32string str_trans2_1, str_trans2_2;
        auto b1 = std::back_inserter(str_trans2_1);
        auto b2 = std::back_inserter(str_trans2_2);
        auto [it, s] = obj.transform(lstlit3.begin(), lstlit3.end(), b1);
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_1 != str_trans1) throw std::runtime_error("collate::transform fail.");
            
        std::tie(it, s) = obj.transform(lstlit4.begin(), lstlit4.end(), b2);
        if (s != len2) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_2 != str_trans2) throw std::runtime_error("collate::transform fail.");
    }
    
    dump_info("Done\n");
}

void test_collate_char32_t_transform_2()
{
    dump_info("Test collate<char32_t> transform 2...");
    const IOv2::collate obj(std::make_shared<IOv2::collate_conf<char32_t>>("C"));
    
    constexpr size_t MAX_SIZE = 10000000;
    const std::u32string sstr(MAX_SIZE, L'a');
    auto len1 = obj.transform_length(sstr.begin(), sstr.end());
    if (len1 != MAX_SIZE)
        throw std::runtime_error("collate::transform_length fail.");
    if (obj.transform_length(sstr.data(), sstr.data() + MAX_SIZE) != len1)
        throw std::runtime_error("collate::transform_length fail.");

    std::u32string str_trans1;
    str_trans1.resize(len1);
    {   // transform using char array as input / output
        auto [it, s] = obj.transform(sstr.data(), sstr.data() + MAX_SIZE, str_trans1.data());
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (it != str_trans1.data() + len1) throw std::runtime_error("collate::transform fail.");
        
        for (size_t i = 0; i < MAX_SIZE; ++i)
            if (str_trans1[i] != 'a') throw std::runtime_error("collate::transform_length fail.");
    }

    {   // transform using list as input and char array as output
        std::list<char32_t> lstlit3{sstr.begin(), sstr.end()};
        std::u32string str_trans2_1; str_trans2_1.resize(len1);
        auto [it, s] = obj.transform(lstlit3.begin(), lstlit3.end(), str_trans2_1.data());
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (it != str_trans2_1.data() + len1) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_1 != str_trans1) throw std::runtime_error("collate::transform fail.");
    }
    
    {   // transform char array as input and back_inserter as output
        std::u32string str_trans2_1;
        auto b1 = std::back_inserter(str_trans2_1);
        auto [it, s] = obj.transform(sstr.data(), sstr.data() + MAX_SIZE, b1);
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_1 != str_trans1) throw std::runtime_error("collate::transform fail.");
    }

    {   // transform list as input and back_inserter as output
        std::list<char32_t> lstlit3{sstr.begin(), sstr.end()};
        std::u32string str_trans2_1;
        auto b1 = std::back_inserter(str_trans2_1);
        auto [it, s] = obj.transform(lstlit3.begin(), lstlit3.end(), b1);
        if (s != len1) throw std::runtime_error("collate::transform fail.");
        if (str_trans2_1 != str_trans1) throw std::runtime_error("collate::transform fail.");
    }
    dump_info("Done\n");
}

void test_collate_char32_t_transform_3()
{
    dump_info("Test collate<char32_t> transform 3...");
    const IOv2::collate obj_c(std::make_shared<IOv2::collate_conf<char32_t>>("C"));
    const IOv2::collate obj_de(std::make_shared<IOv2::collate_conf<char32_t>>("de_DE.UTF-8"));
    
    const char32_t strlit1[] = U"a\0a\0";
    const char32_t strlit2[] = U"a\0b\0";
    const char32_t strlit3[] = U"a\0\xc4\0";
    const char32_t strlit4[] = U"a\0B\0";
    const char32_t strlit5[] = U"aa\0";
    const char32_t strlit6[] = U"b\0a\0";
    
    auto len1_c = obj_c.transform_length(std::begin(strlit1), std::end(strlit1));
    auto len2_c = obj_c.transform_length(std::begin(strlit2), std::end(strlit2));
    auto len3_c = obj_c.transform_length(std::begin(strlit3), std::end(strlit3));
    auto len4_c = obj_c.transform_length(std::begin(strlit4), std::end(strlit4));
    auto len5_c = obj_c.transform_length(std::begin(strlit5), std::end(strlit5));
    auto len6_c = obj_c.transform_length(std::begin(strlit6), std::end(strlit6));
    auto len1_de = obj_de.transform_length(std::begin(strlit1), std::end(strlit1));
    auto len2_de = obj_de.transform_length(std::begin(strlit2), std::end(strlit2));
    auto len3_de = obj_de.transform_length(std::begin(strlit3), std::end(strlit3));
    auto len4_de = obj_de.transform_length(std::begin(strlit4), std::end(strlit4));
    auto len5_de = obj_de.transform_length(std::begin(strlit5), std::end(strlit5));
    auto len6_de = obj_de.transform_length(std::begin(strlit6), std::end(strlit6));
    
    {  // transform_length with list iterator as input
        std::list<char32_t> s1{std::begin(strlit1), std::end(strlit1)};
        std::list<char32_t> s2{std::begin(strlit2), std::end(strlit2)};
        std::list<char32_t> s3{std::begin(strlit3), std::end(strlit3)};
        std::list<char32_t> s4{std::begin(strlit4), std::end(strlit4)};
        std::list<char32_t> s5{std::begin(strlit5), std::end(strlit5)};
        std::list<char32_t> s6{std::begin(strlit6), std::end(strlit6)};
        
        if (obj_c.transform_length(s1.begin(), s1.end()) != len1_c)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_c.transform_length(s2.begin(), s2.end()) != len2_c)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_c.transform_length(s3.begin(), s3.end()) != len3_c)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_c.transform_length(s4.begin(), s4.end()) != len4_c)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_c.transform_length(s5.begin(), s5.end()) != len5_c)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_c.transform_length(s6.begin(), s6.end()) != len6_c)
            throw std::runtime_error("collate::transform_length fail.");

        if (obj_de.transform_length(s1.begin(), s1.end()) != len1_de)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_de.transform_length(s2.begin(), s2.end()) != len2_de)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_de.transform_length(s3.begin(), s3.end()) != len3_de)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_de.transform_length(s4.begin(), s4.end()) != len4_de)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_de.transform_length(s5.begin(), s5.end()) != len5_de)
            throw std::runtime_error("collate::transform_length fail.");
        if (obj_de.transform_length(s6.begin(), s6.end()) != len6_de)
            throw std::runtime_error("collate::transform_length fail.");
    }

    {
        std::u32string str1, str2;
        obj_c.transform(strlit1, strlit1 + 3, std::back_inserter(str1));
        obj_c.transform(strlit2, strlit2 + 3, std::back_inserter(str2));
        if (str1 >= str2) throw std::runtime_error("collate::transform fail.");
    }

    {
        std::u32string str1, str2;
        obj_de.transform(strlit1, strlit1 + 3, std::back_inserter(str1));
        obj_de.transform(strlit2, strlit2 + 3, std::back_inserter(str2));
        if (str1 >= str2) throw std::runtime_error("collate::transform fail.");
    }

    {
        std::u32string str1, str2;
        obj_c.transform(strlit3, strlit3 + 3, std::back_inserter(str1));
        obj_c.transform(strlit4, strlit4 + 3, std::back_inserter(str2));
        if (str1 <= str2) throw std::runtime_error("collate::transform fail.");
    }

    {
        std::u32string str1, str2;
        obj_de.transform(strlit3, strlit3 + 3, std::back_inserter(str1));
        obj_de.transform(strlit4, strlit4 + 3, std::back_inserter(str2));
        if (str1 >= str2) throw std::runtime_error("collate::transform fail.");
    }

    {
        std::u32string str1, str2;
        obj_c.transform(strlit1, strlit1 + 1, std::back_inserter(str1));
        obj_c.transform(strlit5, strlit5 + 1, std::back_inserter(str2));
        if (str1 != str2) throw std::runtime_error("collate::transform fail.");
    }
    
    {
        std::u32string str1, str2;
        obj_de.transform(strlit6, strlit6 + 3, std::back_inserter(str1));
        obj_de.transform(strlit1, strlit1 + 3, std::back_inserter(str2));
        if (str1 <= str2) throw std::runtime_error("collate::transform fail.");
    }
    
    {
        std::u32string str1, str2;
        obj_c.transform(strlit1, strlit1 + 3, std::back_inserter(str1));
        obj_c.transform(strlit5, strlit5 + 3, std::back_inserter(str2));
        if (str1 >= str2) throw std::runtime_error("collate::transform fail.");
    }

    {
        std::u32string str1, str2;
        obj_c.transform(strlit2, strlit2 + 3, std::back_inserter(str1));
        obj_c.transform(strlit2, strlit2 + 4, std::back_inserter(str2));
        if (str1 >= str2) throw std::runtime_error("collate::transform fail.");
    }
    
    {
        std::u32string str1, str2;
        obj_de.transform(strlit2, strlit2 + 4, std::back_inserter(str1));
        obj_de.transform(strlit2, strlit2 + 3, std::back_inserter(str2));
        if (str1 <= str2) throw std::runtime_error("collate::transform fail.");
    }
    dump_info("Done\n");
}

void test_collate_char32_t_transform_4()
{
    dump_info("Test collate<char32_t> transform 4...");
    const IOv2::collate obj(std::make_shared<IOv2::collate_conf<char32_t>>("de_DE.UTF-8"));
    
    const char32_t strlit[] = U"Äuglein Augment\0Base baß Baß Bast";
    const char32_t strlit1[] = U"Äuglein Augment";
    const char32_t strlit2[] = U"Base baß Baß Bast";
    
    // Note: std::end of the above strings includes the eos (\0).
    size_t len = obj.transform_length(std::begin(strlit), std::end(strlit));
    size_t len1 = obj.transform_length(std::begin(strlit1), std::end(strlit1));
    size_t len2 = obj.transform_length(std::begin(strlit2), std::end(strlit2));
    
    if (len1 + len2 != len) throw std::runtime_error("collate::transform_length fail.");

    std::u32string t, t1, t2;
    obj.transform(std::begin(strlit), std::end(strlit), std::back_inserter(t));
    obj.transform(std::begin(strlit1), std::end(strlit1), std::back_inserter(t1));
    obj.transform(std::begin(strlit2), std::end(strlit2), std::back_inserter(t2));
    
    {
        // use char* as input and output
        std::u32string t3, t4;
        t3.resize(len1 - 3); t4.resize(len - 3);
        obj.transform(std::begin(strlit), std::end(strlit), t3.data(), len1 - 3);
        if (t3.size() != len1 - 3) throw std::runtime_error("collate::transform fail.");
        if (t3 != t1.substr(0, t1.size() - 3)) throw std::runtime_error("collate::transform fail.");
        
        obj.transform(std::begin(strlit), std::end(strlit), t4.data(), len - 3);
        if (t4.size() != len - 3) throw std::runtime_error("collate::transform fail.");
        if (t4 != t.substr(0, t.size() - 3)) throw std::runtime_error("collate::transform fail.");
    }
    
    {
        // use char* as input and back_inserter as output
        std::u32string t3, t4;
        obj.transform(std::begin(strlit), std::end(strlit), std::back_inserter(t3), len1 - 3);
        if (t3.size() != len1 - 3) throw std::runtime_error("collate::transform fail.");
        if (t3 != t1.substr(0, t1.size() - 3)) throw std::runtime_error("collate::transform fail.");
        
        obj.transform(std::begin(strlit), std::end(strlit), std::back_inserter(t4), len - 3);
        if (t4.size() != len - 3) throw std::runtime_error("collate::transform fail.");
        if (t4 != t.substr(0, t.size() - 3)) throw std::runtime_error("collate::transform fail.");
    }
    
    {
        // use list iterator as input and char* as output
        std::list<char32_t> ls_strlit(std::begin(strlit), std::end(strlit));
        std::u32string t3, t4;
        t3.resize(len1 - 3); t4.resize(len - 3);
        obj.transform(ls_strlit.begin(), ls_strlit.end(), t3.data(), len1 - 3);
        if (t3.size() != len1 - 3) throw std::runtime_error("collate::transform fail.");
        if (t3 != t1.substr(0, t1.size() - 3)) throw std::runtime_error("collate::transform fail.");
        
        obj.transform(ls_strlit.begin(), ls_strlit.end(), t4.data(), len - 3);
        if (t4.size() != len - 3) throw std::runtime_error("collate::transform fail.");
        if (t4 != t.substr(0, t.size() - 3)) throw std::runtime_error("collate::transform fail.");
    }
    
    {
        // use list iterator as input and back_inserter as output
        std::list<char32_t> ls_strlit(std::begin(strlit), std::end(strlit));
        std::u32string t3, t4;
        obj.transform(ls_strlit.begin(), ls_strlit.end(), std::back_inserter(t3), len1 - 3);
        if (t3.size() != len1 - 3) throw std::runtime_error("collate::transform fail.");
        if (t3 != t1.substr(0, t1.size() - 3)) throw std::runtime_error("collate::transform fail.");
        
        obj.transform(ls_strlit.begin(), ls_strlit.end(), std::back_inserter(t4), len - 3);
        if (t4.size() != len - 3) throw std::runtime_error("collate::transform fail.");
        if (t4 != t.substr(0, t.size() - 3)) throw std::runtime_error("collate::transform fail.");
    }

    dump_info("Done\n");
}