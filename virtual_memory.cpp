#include "virtual_memory.hpp"
#include "mongo.hpp"
#include "serialisables.hpp"

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

    void decommit_at(void* address, size_t size)
    {
        #ifdef __WIN32__
        VirtualFree(address, size, MEM_DECOMMIT);
        #endif // __WIN32__
    }
}

namespace virtual_memory_manager
{
    void* allocate_for(size_t unique_id, size_t size)
    {
        db::read_write_tx rtx;

        user_page page;

        if(db_disk_load(rtx, page, unique_id))
        {
            if(size != page.size)
                throw std::runtime_error("Tried in change page size on a reload. Might be caused by changing db limits. There's nothing wrong with this explicit, this is just for debugging");

            return virtual_memory::allocate_at((void*)page.address, size);
        }
        else
        {
            void* fptr = virtual_memory::allocate_at(nullptr, size);

            if(fptr == nullptr)
                throw std::runtime_error("Failed virtual memory allocation");

            page.unique_id = unique_id;
            page.size = size;
            page.address = (uint64_t)fptr;

            db_disk_overwrite(rtx, page);

            return fptr;
        }
    }

    void decommit_for(size_t unique_id, size_t size)
    {
        db::read_tx rtx;

        user_page page;

        if(!db_disk_load(rtx, page, unique_id))
            throw std::runtime_error("Something horrible happened with virtual memory allocation");

        virtual_memory::decommit_at((void*)page.address, page.size);
    }
}
