#include <common/clocale_wrapper.h>
#include <common/verify.h>
#include <common/dump_info.h>
#include <type_traits>
#include <utility>

using namespace IOv2;

void test_clocale_wrapper_traits()
{
    dump_info("Test clocale_wrapper traits...");
    static_assert(std::is_nothrow_destructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_constructible_v<clocale_wrapper>);
    static_assert(std::is_nothrow_move_assignable_v<clocale_wrapper>);
    dump_info("Done\n");
}

void test_clocale_wrapper_move()
{
    dump_info("Test clocale_wrapper move semantics...");
    clocale_wrapper loc1("C");
    
    // Move construction
    clocale_wrapper loc2(std::move(loc1));
    // loc1 should now be empty (c_locale == nullptr)
    // We can't access c_locale directly as it's private, but we can test safety of destruction
    
    // Move assignment
    clocale_wrapper loc3("C");
    loc3 = std::move(loc2);
    
    dump_info("Done\n");
}

void test_clocale_wrapper_copy()
{
    dump_info("Test clocale_wrapper copy semantics...");
    clocale_wrapper loc1("C");
    
    // Copy construction
    clocale_wrapper loc2(loc1);
    
    // Copy assignment
    clocale_wrapper loc3("C");
    loc3 = loc2;
    
    dump_info("Done\n");
}

void test_clocale_wrapper_self_assignment()
{
    dump_info("Test clocale_wrapper self assignment...");
    clocale_wrapper loc1("C");
    
    // Self copy assignment
    loc1 = loc1;

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    // Self move assignment
    loc1 = std::move(loc1);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    dump_info("Done\n");
}

void test_clocale_wrapper_safety()
{
    dump_info("Test clocale_wrapper safety (moved-from state)...");
    {
        clocale_wrapper loc1("C");
        clocale_wrapper loc2(std::move(loc1));
        // loc1 is now in a moved-from state. 
        // Its destructor should be safe (it is marked noexcept and we added a null check).
    }
    dump_info("Done\n");
}

void test_clocale_wrapper()
{
    test_clocale_wrapper_traits();
    test_clocale_wrapper_move();
    test_clocale_wrapper_copy();
    test_clocale_wrapper_self_assignment();
    test_clocale_wrapper_safety();
}
