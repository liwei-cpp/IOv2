#pragma once

namespace IOv2
{
template <typename T>
class sing_temp
{
public:
    // Note: This implementation assumes init objects are only created during
    // static initialization **before** main(). Static initialization is single-threaded,
    // so atomic operations are not needed.
    // If init objects need to be created dynamically after main(), thread
    // synchronization mechanisms must be added.
    struct init
    {
        init()
        {
            auto& count = RefCount();
            if (count++ == 0)
            {
                T* ptr = sing_temp::ptr();
                new (ptr) T();
            }            
        }

        ~init()
        {
            auto& count = RefCount();
            if (--count == 0)
            {
                T* ptr = sing_temp::ptr();
                ptr->~T();
            }
        }

        static auto& RefCount()
        {
            static unsigned count{0};
            return count;
        }
        
        init(const init&) = delete;
        init& operator=(const init&) = delete;
    };
    
protected:
    sing_temp() = default;
    ~sing_temp() = default;
    sing_temp(const sing_temp&) = delete;
    sing_temp& operator=(const sing_temp&) = delete;
    
public:
    static T* ptr()
    {
        alignas(T) static char sing_buf[sizeof(T)];
        return reinterpret_cast<T*>(sing_buf);
    }
};
}