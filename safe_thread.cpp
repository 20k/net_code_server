#include "safe_thread.hpp"
#include <windows.h>
#include "mongo.hpp"

void sthread::this_yield()
{
    Sleep(0);

    #ifndef __WIN32__
    #error("doesn't work on linux");
    #endif // __WIN32__
}

void sthread::low_yield()
{
    std::this_thread::yield();
}

void sthread::this_sleep(int milliseconds)
{
    for(int i=0; i < milliseconds; i++)
        Sleep(1);

    //Sleep(milliseconds);
}

lock_counter::lock_counter()
{
    (*tls_get_holds_lock())++;
}

lock_counter::~lock_counter()
{
    (*tls_get_holds_lock())--;
}
