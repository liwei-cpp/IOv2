#include <common/lru_cache.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>

using namespace IOv2;

namespace
{
    // Fault injection: the copy constructor throws on the throw_at-th copy.
    // sizeof is 4, so this key is a small type and lru_cache takes it by value.
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

    // 132 bytes, so get() returns const LargeValue* rather than an optional.
    struct LargeValue
    {
        int val;
        char padding[128];
    };
}

template <>
struct std::hash<ThrowingKey>
{
    std::size_t operator()(const ThrowingKey& k) const { return hash<int>{}(k.val); }
};

TEST(LruCache, Basic)
{
    lru_cache<int, std::string, 3> cache;
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    // std::string is 32 bytes, so get() hands back a pointer, not an optional.
    const std::string* r1 = cache.get(1);
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(*r1, "one");

    const std::string* r2 = cache.get(2);
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(*r2, "two");

    const std::string* r3 = cache.get(3);
    ASSERT_NE(r3, nullptr);
    EXPECT_EQ(*r3, "three");
}

TEST(LruCache, PutOverwritesExistingKey)
{
    lru_cache<int, std::string, 3> cache;
    cache.put(1, "hello");
    cache.put(1, "world");

    const std::string* result = cache.get(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "world");
}

TEST(LruCache, TryPutKeepsExistingValue)
{
    lru_cache<int, std::string, 3> cache;
    EXPECT_TRUE(cache.try_put(1, "hello"));
    EXPECT_FALSE(cache.try_put(1, "world"));

    const std::string* result = cache.get(1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "hello");
}

TEST(LruCache, PutEvictsLeastRecentlyUsed)
{
    lru_cache<int, std::string, 2> cache;
    cache.put(1, "one");
    cache.put(2, "two");

    // Reading 1 makes it the most recent, which leaves 2 as the eviction victim.
    (void)cache.get(1);
    cache.put(3, "three");

    const std::string* r1 = cache.get(1);
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(*r1, "one");

    EXPECT_EQ(cache.get(2), nullptr);

    const std::string* r3 = cache.get(3);
    ASSERT_NE(r3, nullptr);
    EXPECT_EQ(*r3, "three");
}

TEST(LruCache, TryPutEvictsLeastRecentlyUsed)
{
    lru_cache<int, std::string, 2> cache;
    EXPECT_TRUE(cache.try_put(1, "one"));
    EXPECT_TRUE(cache.try_put(2, "two"));

    (void)cache.get(1);
    EXPECT_TRUE(cache.try_put(3, "three"));

    const std::string* r1 = cache.get(1);
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(*r1, "one");

    EXPECT_EQ(cache.get(2), nullptr);

    const std::string* r3 = cache.get(3);
    ASSERT_NE(r3, nullptr);
    EXPECT_EQ(*r3, "three");
}

TEST(LruCache, SmallValueTypeYieldsOptional)
{
    lru_cache<int, int, 2> cache;
    cache.put(1, 100);
    cache.put(2, 200);

    auto r1 = cache.get(1);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(*r1, 100);

    auto r2 = cache.get(2);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(*r2, 200);

    // 2 was read last, so 1 goes.
    cache.put(3, 300);
    EXPECT_FALSE(cache.get(1).has_value());
}

TEST(LruCache, CapacityOne)
{
    lru_cache<int, int, 1> cache;
    cache.put(1, 100);
    auto r1 = cache.get(1);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(*r1, 100);

    cache.put(2, 200);
    EXPECT_FALSE(cache.get(1).has_value());
    auto r2 = cache.get(2);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(*r2, 200);

    EXPECT_TRUE(cache.try_put(3, 300));
    EXPECT_FALSE(cache.get(2).has_value());
    auto r3 = cache.get(3);
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(*r3, 300);
}

TEST(LruCache, LargeValueTypeYieldsPointer)
{
    lru_cache<int, LargeValue, 2> cache;
    LargeValue lv{42, {0}};
    cache.put(1, lv);

    const LargeValue* ptr = cache.get(1);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->val, 42);

    EXPECT_EQ(cache.get(2), nullptr);
}

TEST(LruCache, MoveCarriesContents)
{
    lru_cache<int, int, 2> cache1;
    cache1.put(1, 100);

    lru_cache<int, int, 2> cache2(std::move(cache1));
    auto moved = cache2.get(1);
    ASSERT_TRUE(moved.has_value());
    EXPECT_EQ(*moved, 100);

    lru_cache<int, int, 2> cache3;
    cache3 = std::move(cache2);
    auto assigned = cache3.get(1);
    ASSERT_TRUE(assigned.has_value());
    EXPECT_EQ(*assigned, 100);
}

// ThrowingKey's trip wire is static, so a case that left it armed would derail
// whichever case ran next. The fixture disarms it on both sides instead of
// trusting each case to clean up after itself.
class LruCacheFaultInjection : public ::testing::Test
{
protected:
    void SetUp() override { arm(-1); }
    void TearDown() override { arm(-1); }

    static void arm(int throw_at)
    {
        ThrowingKey::copy_count = 0;
        ThrowingKey::throw_at = throw_at;
    }
};

// put() copies the key twice: once into the list, once into the map. Only the
// second is inside the try block, so the two throw points reach different code.
TEST_F(LruCacheFaultInjection, FailureBeforeTheTryBlockLeavesCacheIntact)
{
    lru_cache<ThrowingKey, std::string, 2> cache;
    cache.put(ThrowingKey(1), "one");

    arm(1);
    EXPECT_THROW(cache.put(ThrowingKey(2), "two"), std::runtime_error);
    arm(-1);

    const std::string* r1 = cache.get(ThrowingKey(1));
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(*r1, "one");
    EXPECT_EQ(cache.get(ThrowingKey(2)), nullptr);

    // The cache is not merely unchanged, it is still usable.
    cache.put(ThrowingKey(3), "three");
    const std::string* r3 = cache.get(ThrowingKey(3));
    ASSERT_NE(r3, nullptr);
    EXPECT_EQ(*r3, "three");
}

TEST_F(LruCacheFaultInjection, PutRollsBackWhenTheMapInsertThrows)
{
    lru_cache<ThrowingKey, int, 2> cache;

    arm(2);
    EXPECT_THROW(cache.put(ThrowingKey(1), 100), std::runtime_error);
    arm(-1);

    EXPECT_FALSE(cache.get(ThrowingKey(1)).has_value());
}

TEST_F(LruCacheFaultInjection, TryPutRollsBackWhenTheMapInsertThrows)
{
    lru_cache<ThrowingKey, int, 2> cache;

    arm(2);
    EXPECT_THROW((void)cache.try_put(ThrowingKey(2), 200), std::runtime_error);
    arm(-1);

    EXPECT_FALSE(cache.get(ThrowingKey(2)).has_value());
}
