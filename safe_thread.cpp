#include "safe_thread.hpp"
#ifdef __WIN32__
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "mongo.hpp"
#include <SFML/System/Sleep.hpp>

void sthread::this_yield()
{
    #ifdef __WIN32__
    Sleep(0);
    #else
    sleep(0);
    #endif
}

void sthread::low_yield()
{
    std::this_thread::yield();
}

void sthread::this_sleep(int milliseconds)
{
    sf::sleep(sf::milliseconds(milliseconds));
}

void thread_priority_handler::enable()
{
    #ifdef __WIN32__
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    #else
    nice(-20);
    #endif
}

thread_priority_handler::~thread_priority_handler()
{
    #ifdef __WIN32__
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    #else
    nice(0);
    #endif
}

lock_counter::lock_counter()
{
    (*tls_get_holds_lock())++;
}

lock_counter::~lock_counter()
{
    (*tls_get_holds_lock())--;
}
