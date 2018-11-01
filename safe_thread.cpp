#include "safe_thread.hpp"
#include <windows.h>

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
    Sleep(milliseconds);
}
