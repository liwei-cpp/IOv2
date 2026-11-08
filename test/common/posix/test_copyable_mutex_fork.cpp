// SPDX-FileCopyrightText: 2026 liwei <liwei.cpp@gmail.com>
// SPDX-License-Identifier: MIT

#include <IOv2/common/copyable_mutex.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace IOv2;

// ---------------------------------------------------------------------------
// 8. 跨 fork()：fork 时另一线程持有的锁，在子进程里仍能加锁。fork() 只把调用它的线程
//    带进子进程，裸 std::mutex 在子进程里会永久阻塞（多线程程序 fork 后 exec 失败、
//    往 cerr 报错即是这一形态）。子进程是裸 fork（检查的正是 fork 本身），只看它是否
//    在时限内经管道回报；卡住的子进程被杀掉，不会拖住整个测试。子进程以 exec
//    /bin/true 结束，不 _exit：valgrind 在 _exit 时把 fork 时别的线程拥有的内存报成
//    泄漏，exec 则不做退出检查，而子进程里的访问错误在发生时就已报出。
// ---------------------------------------------------------------------------
namespace
{
    // Forks; the child runs `in_child` and, if it returns true, writes one byte back.
    // True when that byte arrived within 5 s.
    template <typename F>
    bool child_gets_through(F in_child)
    {
        int fds[2];
        if (::pipe(fds) != 0)
            return false;
        const pid_t pid = ::fork();
        if (pid == 0)
        {
            ::close(fds[0]);
            const char ok = 'k';
            if (in_child())
                (void)!::write(fds[1], &ok, 1);
            ::execl("/bin/true", "true", static_cast<char*>(nullptr));
            ::_exit(127);
        }
        ::close(fds[1]);
        pollfd p{fds[0], POLLIN, 0};
        char c = 0;
        const bool got = pid > 0 && ::poll(&p, 1, 5000) == 1 && ::read(fds[0], &c, 1) == 1;
        if (pid > 0 && !got)
            ::kill(pid, SIGKILL);
        if (pid > 0)
            ::waitpid(pid, nullptr, 0);
        ::close(fds[0]);
        return got;
    }

    // Runs `body` while another thread holds `m` through `lock` / `unlock`.
    template <typename M, typename Lock, typename Unlock, typename F>
    void while_another_thread_holds(M& m, Lock lock, Unlock unlock, F body)
    {
        std::atomic<bool> held{false}, release{false};
        std::thread t([&] {
            lock(m);
            held = true;
            while (!release)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            unlock(m);
        });
        while (!held)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        body();
        release = true;
        t.join();
    }

    template <typename M>
    void check_exclusive_held_across_fork()
    {
        M m;
        while_another_thread_holds(m, [](M& x) { x.lock(); }, [](M& x) { x.unlock(); }, [&] {
            EXPECT_TRUE(child_gets_through([&] {
                m.lock();
                m.unlock();
                if (!m.try_lock())
                    return false;
                m.unlock();
                return true;
            }));
        });
    }
}

TEST(CopyableMutex, ALockAnotherThreadHeldAtForkIsUsableInTheChild)
{
    {
        SCOPED_TRACE("std::mutex");
        check_exclusive_held_across_fork<copyable_mutex<std::mutex>>();
    }
    {
        SCOPED_TRACE("std::recursive_mutex");
        check_exclusive_held_across_fork<copyable_mutex<std::recursive_mutex>>();
    }
    {
        SCOPED_TRACE("std::shared_mutex");
        check_exclusive_held_across_fork<copyable_mutex<std::shared_mutex>>();
    }
}

TEST(CopyableMutex, ASharedLockAnotherThreadHeldAtForkLetsTheChildWrite)
{
    copyable_mutex<std::shared_mutex> m;
    while_another_thread_holds(m, [](auto& x) { x.lock_shared(); }, [](auto& x) { x.unlock_shared(); }, [&] {
        EXPECT_TRUE(child_gets_through([&] {
            std::lock_guard g(m);
            return true;
        }));
    });
}

// The parent's own lock is untouched by the fork: still held by the other thread, free
// once it lets go.
TEST(CopyableMutex, AForkLeavesTheParentsLockAlone)
{
    copyable_mutex<std::mutex> m;
    while_another_thread_holds(m, [](auto& x) { x.lock(); }, [](auto& x) { x.unlock(); }, [&] {
        EXPECT_TRUE(child_gets_through([] { return true; }));
        EXPECT_FALSE(m.try_lock());
    });
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}
