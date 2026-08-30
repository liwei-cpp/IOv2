#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/iostream.h>
#include <io/istream.h>
#include <io/traits/arithmetic.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    // Ten threads pull from one device, so which thread reads which pair is not
    // fixed -- but every pair is "123 456", so each extraction still has an exact
    // expectation. IOv2::sync is what makes the two extractions one critical
    // section; without it a thread can read another's 456 as its own 123.
    template <template <typename, typename> class T>
    void extract_pairs_concurrently()
    {
        constexpr std::size_t thread_num = 10;
        constexpr std::size_t loop_num = 1024;

        std::wstring ref;
        ref.reserve(loop_num * thread_num * 8);
        for (std::size_t i = 0; i < loop_num * thread_num; ++i)
            ref += L"123 456\n";

        T istr{IOv2::mem_device{ref}};
        std::vector<std::thread> tr_vec;
        tr_vec.reserve(thread_num);

        for (std::size_t thread_ID = 0; thread_ID < thread_num; ++thread_ID)
        {
            std::thread tr([&istr]()
            {
                for (std::size_t i = 0; i < loop_num; ++i)
                {
                    int v1 = 0;
                    int v2 = 0;
                    IOv2::sync(istr).stream >> v1 >> v2;
                    EXPECT_EQ(v1, 123);
                    EXPECT_EQ(v2, 456);
                }
            });
            tr_vec.push_back(std::move(tr));
        }

        for (auto& tr : tr_vec)
            tr.join();
    }
}

TEST(IstreamSyncWchar, TenThreadsExtractWholePairs)
{
    extract_pairs_concurrently<IOv2::istream>();
}

TEST(IstreamSyncWchar, TenThreadsExtractWholePairsThroughAnIostream)
{
    extract_pairs_concurrently<IOv2::iostream>();
}
