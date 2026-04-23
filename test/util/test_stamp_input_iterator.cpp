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
}
