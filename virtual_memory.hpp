#ifndef VIRTUAL_MEMORY_HPP_INCLUDED
#define VIRTUAL_MEMORY_HPP_INCLUDED

#include <cstdint>
#include <networking/serialisable_fwd.hpp>

struct user_page : serialisable, free_function
{
    size_t unique_id = -1;
    uint64_t address = 0;
    uint64_t size = 0;
};

namespace virtual_memory
{
    void* allocate_at(void* address, size_t size);
    void free_at(void* address, size_t size);
    void decommit_at(void* address, size_t size); ///removes physical backing, keeps address
}

namespace virtual_memory_manager
{
    void* allocate_for(size_t unique_id, size_t size);
    void decommit_for(size_t unique_id, size_t size);
    //void free_for(void* address, size_t unique_id, size_t size);
}

#define JS_FUNCTION(x) __attribute__((section(".static_func.S" #x)))

#endif // VIRTUAL_MEMORY_HPP_INCLUDED
