#include <common/lru_cache.h>
#include <common/verify.h>
#include <common/dump_info.h>
#include <string>
#include <stdexcept>

using namespace IOv2;

struct ThrowingKey
{
    int val;
    static inline int copy_count = 0;
    static inline int throw_at = -1;

    ThrowingKey(int v) : val(v) {}
    ThrowingKey(const ThrowingKey& other) : val(other.val)
    {
        if (throw_at != -1 && ++copy_count >= throw_at)
        {
            throw std::runtime_error("Simulated copy failure");
        }
    }
    ThrowingKey& operator=(const ThrowingKey&) = default;
    bool operator==(const ThrowingKey& other) const { return val == other.val; }
};

namespace std
{
    template <>
    struct hash<ThrowingKey>
    {
        size_t operator()(const ThrowingKey& k) const { return hash<int>{}(k.val); }
    };
}

// Large type to trigger pointer return branch in get()
struct LargeValue {
    int val;
    char padding[128]; 
};

void test_lru_cache_exception_safety()
{
    dump_info("Test lru_cache exception safety...");
    lru_cache<ThrowingKey, std::string, 2> cache;
    cache.put(ThrowingKey(1), "one");
    
    ThrowingKey::copy_count = 0;
    ThrowingKey::throw_at = 1; // Throw on first copy (likely during map insertion)
    
    try
    {
        cache.put(ThrowingKey(2), "two");
        VERIFY(false); // Should have thrown
    }
    catch (const std::runtime_error&)
    {
        // Expected
    }

    ThrowingKey::throw_at = -1; // Stop throwing
    
    // Verify 1 is still there and 2 is NOT there
    auto r1 = cache.get(ThrowingKey(1));
    VERIFY(r1 && *r1 == "one");
    
    auto r2 = cache.get(ThrowingKey(2));
    VERIFY(!r2);

    // Verify cache still works
    cache.put(ThrowingKey(3), "three");
    auto r3 = cache.get(ThrowingKey(3));
    VERIFY(r3 && *r3 == "three");
    
    dump_info("Done\n");
}

void test_lru_cache_basic()
{
    dump_info("Test lru_cache basic...");
    lru_cache<int, std::string, 3> cache;
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    auto r1 = cache.get(1);
    VERIFY(r1 && *r1 == "one");
    auto r2 = cache.get(2);
    VERIFY(r2 && *r2 == "two");
    auto r3 = cache.get(3);
    VERIFY(r3 && *r3 == "three");
    dump_info("Done\n");
}

void test_lru_cache_update()
{
    dump_info("Test lru_cache update...");
    lru_cache<int, std::string, 3> cache;
    cache.put(1, "hello");
    cache.put(1, "world"); // Should update existing key

    auto result = cache.get(1);
    VERIFY(result && *result == "world");
    dump_info("Done\n");
}

void test_lru_cache_try_put()
{
    dump_info("Test lru_cache try_put...");
    lru_cache<int, std::string, 3> cache;
    VERIFY(cache.try_put(1, "hello") == true);
    VERIFY(cache.try_put(1, "world") == false); // Should NOT update

    auto result = cache.get(1);
    VERIFY(result && *result == "hello");
    dump_info("Done\n");
}

void test_lru_cache_eviction()
{
    dump_info("Test lru_cache eviction...");
    lru_cache<int, std::string, 2> cache;
    cache.put(1, "one");
    cache.put(2, "two");
    
    // 1 is MRU, 2 is LRU
    (void)cache.get(1); 
    
    // 2 should be evicted
    cache.put(3, "three"); 

    auto r1 = cache.get(1);
    VERIFY(r1 && *r1 == "one");
    
    auto r2 = cache.get(2);
    VERIFY(!r2);
    
    auto r3 = cache.get(3);
    VERIFY(r3 && *r3 == "three");
    dump_info("Done\n");
}

void test_lru_cache_small_type()
{
    dump_info("Test lru_cache small type...");
    lru_cache<int, int, 2> cache;
    cache.put(1, 100);
    cache.put(2, 200);

    auto r1 = cache.get(1);
    VERIFY(r1 && *r1 == 100);
    
    auto r2 = cache.get(2);
    VERIFY(r2 && *r2 == 200);
    
    cache.put(3, 300);
    VERIFY(!cache.get(1)); // 1 should be evicted
    dump_info("Done\n");
}

void test_lru_cache_try_put_eviction()
{
    dump_info("Test lru_cache try_put eviction...");
    lru_cache<int, std::string, 2> cache;
    VERIFY(cache.try_put(1, "one") == true);
    VERIFY(cache.try_put(2, "two") == true);
    
    // 1 is MRU, 2 is LRU
    (void)cache.get(1); 
    
    // 2 should be evicted
    VERIFY(cache.try_put(3, "three") == true); 

    auto r1 = cache.get(1);
    VERIFY(r1 && *r1 == "one");
    
    auto r2 = cache.get(2);
    VERIFY(!r2);
    
    auto r3 = cache.get(3);
    VERIFY(r3 && *r3 == "three");
    dump_info("Done\n");
}

void test_lru_cache_capacity_one()
{
    dump_info("Test lru_cache capacity one...");
    lru_cache<int, int, 1> cache;
    cache.put(1, 100);
    VERIFY(cache.get(1) && *cache.get(1) == 100);
    
    cache.put(2, 200);
    VERIFY(!cache.get(1));
    VERIFY(cache.get(2) && *cache.get(2) == 200);
    
    VERIFY(cache.try_put(3, 300) == true);
    VERIFY(!cache.get(2));
    VERIFY(cache.get(3) && *cache.get(3) == 300);
    dump_info("Done\n");
}

void test_lru_cache_remaining_coverage() {
    dump_info("Test lru_cache remaining coverage...");

    // 1. Large type branch coverage
    {
        lru_cache<int, LargeValue, 2> cache;
        LargeValue lv{42, {0}};
        cache.put(1, lv);
        const LargeValue* ptr = cache.get(1);
        VERIFY(ptr != nullptr && ptr->val == 42);
        VERIFY(cache.get(2) == nullptr);
    }

    // 2. Move semantics coverage
    {
        lru_cache<int, int, 2> cache1;
        cache1.put(1, 100);
        lru_cache<int, int, 2> cache2(std::move(cache1));
        VERIFY(cache2.get(1) && *cache2.get(1) == 100);

        lru_cache<int, int, 2> cache3;
        cache3 = std::move(cache2);
        VERIFY(cache3.get(1) && *cache3.get(1) == 100);
    }

    // 3. Precise catch block coverage
    {
        lru_cache<ThrowingKey, int, 2> cache;
        ThrowingKey::copy_count = 0;
        ThrowingKey::throw_at = 2; // Success for list, throw for map
        try {
            cache.put(ThrowingKey(1), 100);
            VERIFY(false);
        } catch (const std::runtime_error&) {}
        
        ThrowingKey::throw_at = -1;
        VERIFY(!cache.get(ThrowingKey(1)));

        // Repeat for try_put
        ThrowingKey::copy_count = 0;
        ThrowingKey::throw_at = 2;
        try {
            (void)cache.try_put(ThrowingKey(2), 200);
            VERIFY(false);
        } catch (const std::runtime_error&) {}
        
        ThrowingKey::throw_at = -1;
        VERIFY(!cache.get(ThrowingKey(2)));
    }

    dump_info("Done\n");
}

void test_lru_cache()
{
    test_lru_cache_basic();
    test_lru_cache_update();
    test_lru_cache_try_put();
    test_lru_cache_eviction();
    test_lru_cache_try_put_eviction();
    test_lru_cache_capacity_one();
    test_lru_cache_remaining_coverage();
    test_lru_cache_small_type();
    test_lru_cache_exception_safety();
}
