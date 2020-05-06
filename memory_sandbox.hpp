#ifndef MEMORY_SANDBOX_HPP_INCLUDED
#define MEMORY_SANDBOX_HPP_INCLUDED

#include "safe_thread.hpp"
#include <atomic>
#include <SFML/System.hpp>
#include <memory>

typedef struct
{
    /* The double value in the union is there to ensure alignment is
     * good for IEEE doubles too.  In many 32-bit environments 4 bytes
     * would be sufficiently aligned and the double value is unnecessary.
     */
    union
    {
        size_t sz;
        double d;
    } u;
} alloc_hdr;

//static size_t total_allocated = 0;
static size_t max_allocated = 64 * 1024 * 1024;  /* 64MB sandbox */

static void sandbox_dump_memstate(void)
{
#if 0
    fprintf(stderr, "Total allocated: %ld\n", (long) total_allocated);
    fflush(stderr);
#endif
}

struct command_handler_state;
struct shared_command_handler_state;

struct sandbox_data
{
    std::shared_ptr<shared_command_handler_state> all_shared = nullptr;
    int realtime_script_id = -1;
    sf::Clock full_run_clock;
    std::atomic_bool is_static{false};
    float max_elapsed_time_ms = 0;
    double ms_awake_elapsed_static = 0;
    double framerate_limit = 30;

    size_t total_allocated = 0;
    std::atomic_bool terminate_semi_gracefully{false};
    std::atomic_bool terminate_realtime_gracefully{false};
    std::atomic_int sleep_for{0};

    std::atomic_bool is_realtime{false};
    double realtime_ms_awake_elapsed{0};
    sf::Clock clk;
};

inline void *sandbox_alloc(void *udata, size_t size)
{
    alloc_hdr *hdr;

    sandbox_data* data = (sandbox_data*)udata;

    if (size == 0)
    {
        return NULL;
    }

    if(data->total_allocated + size > max_allocated)
    {
        //fprintf(stderr, "Sandbox maximum allocation size reached, %ld requested in sandbox_alloc\n",
        //        (long) size);

        std::cout << "sandbox max size reached" << std::endl;

        fflush(stderr);
        return NULL;
    }

    hdr = (alloc_hdr *) malloc(size + sizeof(alloc_hdr));
    if (!hdr)
    {
        return NULL;
    }
    hdr->u.sz = size;
    data->total_allocated += size;
    sandbox_dump_memstate();
    return (void *) (hdr + 1);
}

inline void *sandbox_realloc(void *udata, void *ptr, size_t size)
{
    alloc_hdr *hdr;
    size_t old_size;
    void *t;

    /* Handle the ptr-NULL vs. size-zero cases explicitly to minimize
     * platform assumptions.  You can get away with much less in specific
     * well-behaving environments.
     */

    sandbox_data* data = (sandbox_data*)udata;

    if (ptr)
    {
        hdr = (alloc_hdr *) (((char *) ptr) - sizeof(alloc_hdr));
        old_size = hdr->u.sz;

        if (size == 0)
        {
            data->total_allocated -= old_size;
            free((void *) hdr);
            sandbox_dump_memstate();
            return NULL;
        }
        else
        {
            if (data->total_allocated - old_size + size > max_allocated)
            {
                //fprintf(stderr, "Sandbox maximum allocation size reached, %ld requested in sandbox_realloc\n",
                //        (long) size);
                fflush(stderr);
                return NULL;
            }

            t = realloc((void *) hdr, size + sizeof(alloc_hdr));
            if (!t)
            {
                return NULL;
            }
            hdr = (alloc_hdr *) t;
            data->total_allocated -= old_size;
            data->total_allocated += size;
            hdr->u.sz = size;
            sandbox_dump_memstate();
            return (void *) (hdr + 1);
        }
    }
    else
    {
        if (size == 0)
        {
            return NULL;
        }
        else
        {
            if (data->total_allocated + size > max_allocated)
            {
                //fprintf(stderr, "Sandbox maximum allocation size reached, %ld requested in sandbox_realloc\n",
                //        (long) size);
                fflush(stderr);
                return NULL;
            }

            hdr = (alloc_hdr *) malloc(size + sizeof(alloc_hdr));
            if (!hdr)
            {
                return NULL;
            }
            hdr->u.sz = size;
            data->total_allocated += size;
            sandbox_dump_memstate();
            return (void *) (hdr + 1);
        }
    }
}

inline void sandbox_free(void *udata, void *ptr)
{
    alloc_hdr *hdr;

    sandbox_data* data = (sandbox_data*)udata;

    if (!ptr)
    {
        return;
    }
    hdr = (alloc_hdr *) (((char *) ptr) - sizeof(alloc_hdr));
    data->total_allocated -= hdr->u.sz;
    free((void *) hdr);
    sandbox_dump_memstate();
}

///so. What we really want to do is terminate the thread we're running in here
inline void sandbox_fatal(void *udata, const char *msg)
{
    (void) udata;  /* Suppress warning. */
    fprintf(stderr, "FATAL: %s\n", (msg ? msg : "no message"));
    fflush(stderr);

    while(1){throw std::runtime_error("Sandbox fatal");}

    //exit(1);  /* must not return */
}

#endif // MEMORY_SANDBOX_HPP_INCLUDED
