#pragma once
#include <atomic>

namespace IOv2
{
template <typename T>
class sing_temp
{
public:
    struct init
    {
        init()
        {
            auto& count = RefCount();
            auto ori = count.fetch_add(1);
            if (ori == 0)
            {
                T* ptr = sing_temp::ptr();
                new (ptr) T();
            }            
        }

        ~init()
        {
            auto& count = RefCount();
            auto ori = count.fetch_sub(1);
            if (ori == 1)
            {
                T* ptr = sing_temp::ptr();
                ptr->~T();
            }
        }

        static auto& RefCount()
        {
            static std::atomic<unsigned> count{0};
            return count;
        }
        
        init(const init&) = delete;
        init& operator= (const init&) = delete;
    };
    
protected:
    sing_temp() = default;
    ~sing_temp() = default;
    sing_temp(const sing_temp&) = delete;
    sing_temp& operator= (const sing_temp&) = delete;
    
public:
    static T* ptr()
    {
        alignas(T) static char sing_buf[sizeof(T)];
        return reinterpret_cast<T*>(sing_buf);
    }
};
}