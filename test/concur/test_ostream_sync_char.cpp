// SPDX-FileCopyrightText: 2026 liwei <liweifriends@gmail.com>
// SPDX-License-Identifier: MIT

#include <device/mem_device.h>
#include <io/io_base.h>
#include <io/iostream.h>
#include <io/ostream.h>
#include <io/traits/arithmetic.h>
#include <io/traits/char_and_str.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    // Every thread writes the same three-token record, so the interleaving cannot
    // be observed in the order of the records -- only in whether a record was torn
    // apart. IOv2::sync is what keeps the three insertions together; without it the
    // device ends up holding something other than N copies of "123 456\n".
    template <template <typename, typename> class T>
    void insert_records_concurrently()
    {
        constexpr std::size_t thread_num = 10;
        constexpr std::size_t loop_num = 1024;

        T ostr{IOv2::mem_device{""}};
        std::vector<std::thread> tr_vec;
        tr_vec.reserve(thread_num);

        for (std::size_t thread_ID = 0; thread_ID < thread_num; ++thread_ID)
        {
            std::thread tr([&ostr]()
            {
                for (std::size_t i = 0; i < loop_num; ++i)
                    IOv2::sync(ostr).stream << 123 << ' ' << 456 << '\n';
            });
            tr_vec.push_back(std::move(tr));
        }

        for (auto& tr : tr_vec)
            tr.join();

        std::string ref;
        ref.reserve(loop_num * thread_num * 8);
        for (std::size_t i = 0; i < loop_num * thread_num; ++i)
            ref += "123 456\n";

        auto [dev, err] = ostr.detach();
        EXPECT_EQ(dev.str(), ref);
    }
}

TEST(OstreamSyncChar, TenThreadsInsertWholeRecords)
{
    insert_records_concurrently<IOv2::ostream>();
}

TEST(OstreamSyncChar, TenThreadsInsertWholeRecordsThroughAnIostream)
{
    insert_records_concurrently<IOv2::iostream>();
}
