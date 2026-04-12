#include <common/lru_cache.h>
#include <common/verify.h>
#include <common/dump_info.h>
#include <string>

using namespace IOv2;

void test_lru_cache_basic()
{
    dump_info("Test lru_cache basic...");
    lru_cache<int, std::string, 3> cache;
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    auto r1 = cache.get(1);
    VERIFY(r1.has_value() && *r1 == "one");
    auto r2 = cache.get(2);
    VERIFY(r2.has_value() && *r2 == "two");
    auto r3 = cache.get(3);
    VERIFY(r3.has_value() && *r3 == "three");
    dump_info("Done\n");
}

void test_lru_cache_update()
{
    dump_info("Test lru_cache update...");
    lru_cache<int, std::string, 3> cache;
    cache.put(1, "hello");
    cache.put(1, "world"); // Should update existing key

    auto result = cache.get(1);
    VERIFY(result.has_value() && *result == "world");
    dump_info("Done\n");
}

void test_lru_cache_try_put()
{
    dump_info("Test lru_cache try_put...");
    lru_cache<int, std::string, 3> cache;
    VERIFY(cache.try_put(1, "hello") == true);
    VERIFY(cache.try_put(1, "world") == false); // Should NOT update

    auto result = cache.get(1);
    VERIFY(result.has_value() && *result == "hello");
    dump_info("Done\n");
}

void test_lru_cache_eviction()
{
    dump_info("Test lru_cache eviction...");
    lru_cache<int, std::string, 2> cache;
    cache.put(1, "one");
    cache.put(2, "two");
    
    // 1 is MRU, 2 is LRU
    cache.get(1); 
    
    // 2 should be evicted
    cache.put(3, "three"); 

    auto r1 = cache.get(1);
    VERIFY(r1.has_value() && *r1 == "one");
    
    auto r2 = cache.get(2);
    VERIFY(!r2.has_value());
    
    auto r3 = cache.get(3);
    VERIFY(r3.has_value() && *r3 == "three");
    dump_info("Done\n");
}

void test_lru_cache()
{
    test_lru_cache_basic();
    test_lru_cache_update();
    test_lru_cache_try_put();
    test_lru_cache_eviction();
}
