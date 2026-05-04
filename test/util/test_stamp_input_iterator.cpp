#include <vector>
#include <cassert>
#include <common/stamp_input_iterator.h>
#include <common/verify.h>
#include <io/streambuf_iterator.h>
#include <device/mem_device.h>
#include <io/streambuf.h>
#include <common/dump_info.h>

using namespace IOv2;

struct TestPoint {
    int x;
    int y;
};

void test_stamp_input_iterator() {
    dump_info("Test stamp_input_iterator basic...");
    {
        std::vector<TestPoint> points = {{1, 2}, {3, 4}};
        IOv2::stamp_input_iterator it(points.begin());
        
        VERIFY(it->x == 1);
        VERIFY(it->y == 2);
        
        ++it;
        VERIFY(it->x == 3);
        VERIFY(it->y == 4);
        
        it.rollback();
        VERIFY(it->x == 1);
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator istreambuf_iterator...");
    {
        IOv2::mem_device dev("abc");
        IOv2::istreambuf buf(dev);
        IOv2::istreambuf_iterator is_it(buf);
        IOv2::stamp_input_iterator s_it(is_it);
        
        char c = *s_it;
        VERIFY(c == 'a');
        
        // Testing operator-> if possible (istreambuf_iterator usually points to char)
        // Here we just ensure it compiles and behaves correctly
        VERIFY(*(s_it.operator->()) == 'a');
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator raw pointer...");
    {
        TestPoint points[] = {{10, 20}, {30, 40}};
        IOv2::stamp_input_iterator it(&points[0]);
        
        VERIFY(it->x == 10);
        VERIFY(it->y == 20);
        
        ++it;
        VERIFY(it->x == 30);
        VERIFY(it->y == 40);
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator move constructor...");
    {
        std::vector<int> vec = {1, 2, 3, 4};
        IOv2::stamp_input_iterator it1(vec.begin());
        ++it1;
        ++it1; // m_pos = 2
        
        // Move construction
        auto it2 = std::move(it1);
        // it1.m_pos should be 0 now
        it1.rollback(); // Should be no-op
        
        it2.rollback();
        VERIFY(*it2 == 1);
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator move assignment...");
    {
        std::vector<int> vec = {1, 2, 3, 4};
        IOv2::stamp_input_iterator it1(vec.begin());
        ++it1;
        ++it1; // m_pos = 2
        
        // Move assignment
        auto it3 = IOv2::stamp_input_iterator(vec.begin());
        it3 = std::move(it1);
        
        it1.rollback(); // Should be no-op
        it3.rollback();
        VERIFY(*it3 == 1);
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator self assignment...");
    {
        std::vector<int> vec = {1, 2, 3, 4};
        IOv2::stamp_input_iterator it1(vec.begin());
        ++it1; // m_pos = 1

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
        it1 = std::move(it1);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
        VERIFY(*it1 == 2);
        it1.rollback();
        VERIFY(*it1 == 1);
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator move semantics (istreambuf)...");
    {
        IOv2::mem_device dev("abc");
        IOv2::istreambuf buf(dev);
        IOv2::istreambuf_iterator is_it(buf);
        IOv2::stamp_input_iterator s_it1(is_it);
        
        ++s_it1;
        ++s_it1; // m_rec has 2 elements
        
        // Move construction
        auto s_it2 = std::move(s_it1);
        s_it1.rollback(); // Should be no-op as m_rec is empty
        
        s_it2.rollback();
        VERIFY(*s_it2 == 'a');
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator arithmetic and comparisons...");
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        IOv2::stamp_input_iterator it1(vec.begin());
        IOv2::stamp_input_iterator it2(vec.begin());
        
        VERIFY(it1 == it2);
        VERIFY(!(it1 != it2));
        
        (void)it1++;
        VERIFY(*it1 == 2);
        VERIFY(it1 != it2);
        VERIFY(it1 > it2);
        VERIFY(it1 >= it2);
        VERIFY(it2 < it1);
        VERIFY(it2 <= it1);
        
        it1 += 2;
        VERIFY(*it1 == 4);
        
        it1 -= 1;
        VERIFY(*it1 == 3);
        
        auto it3 = it1 + 1;
        VERIFY(*it3 == 4);
        
        auto it4 = 1 + it1;
        VERIFY(*it4 == 4);
        
        auto it5 = it1 - 1;
        VERIFY(*it5 == 2);
        
        VERIFY(it1 - it2 == 2);
        
        VERIFY(it1[1] == 4);
        
        (void)it1--;
        VERIFY(*it1 == 2);
        
        it1.rollback();
        VERIFY(*it1 == 1);
        
        VERIFY(it1.internal() == vec.begin());
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator istreambuf_iterator backward...");
    {
        IOv2::mem_device dev("abc");
        IOv2::istreambuf buf(dev);
        IOv2::istreambuf_iterator is_it(buf);
        IOv2::stamp_input_iterator s_it(is_it);
        
        (void)s_it++;
        (void)s_it++;
        VERIFY(*s_it == 'c');
        
        --s_it;
        VERIFY(*s_it == 'b');
        
        (void)s_it--;
        VERIFY(*s_it == 'a');
        
        try {
            --s_it;
            VERIFY(false); // Should throw
        } catch (const std::runtime_error&) {
            // OK
        }
        
        (void)s_it++;
        s_it.rollback();
        VERIFY(*s_it == 'a');
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator constructors...");
    {
        IOv2::stamp_input_iterator<std::vector<int>::iterator> it_default;
        // it_default is default initialized, internal iterator is value initialized.
        
        IOv2::stamp_input_iterator<std::vector<int>::iterator> it_sentinel{std::default_sentinel};
        // Similar to default
        
        std::vector<int> vec = {1};
        IOv2::stamp_input_iterator it(vec.begin());
        IOv2::stamp_input_iterator it_copy(it);
        VERIFY(it_copy == it);
        
        IOv2::stamp_input_iterator it_assign = it;
        VERIFY(it_assign == it);

        // Test traits
        VERIFY(is_stamp_input_iterator_v<decltype(it)>);
        VERIFY(!is_stamp_input_iterator_v<int>);
    }
    dump_info("Done\n");

    dump_info("Test stamp_input_iterator istreambuf_iterator additional...");
    {
        IOv2::mem_device dev("abc");
        IOv2::istreambuf buf(dev);
        IOv2::istreambuf_iterator is_it(buf);
        IOv2::stamp_input_iterator s_it(is_it);
        
        // rollback when empty
        s_it.rollback();
        VERIFY(*s_it == 'a');
        
        // internal()
        VERIFY(s_it.internal() == is_it);

        // move assignment
        IOv2::stamp_input_iterator s_it2(is_it);
        s_it2 = std::move(s_it);
        VERIFY(*s_it2 == 'a');
    }
    dump_info("Done\n");
}
