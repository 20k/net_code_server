#include "virtual_memory.hpp"

#ifdef __WIN32__
#include <windows.h>
#include <memoryapi.h>
#endif // __WIN32__

namespace virtual_memory
{
    void* allocate_at(void* address, size_t size)
    {
        #ifdef __WIN32__
        return VirtualAlloc(address, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        #endif // __WIN32__
    }

    void free_at(void* address, size_t size)
    {
        #ifdef __WIN32__
        VirtualFree(address, 0, MEM_RELEASE);
        #endif // __WIN32__
    }
}
