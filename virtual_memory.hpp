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
}

#endif // VIRTUAL_MEMORY_HPP_INCLUDED
