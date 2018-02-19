#ifndef MEMORY_SANDBOX_HPP_INCLUDED
#define MEMORY_SANDBOX_HPP_INCLUDED

#include "duktape.h"

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
static size_t max_allocated = 256 * 1024;  /* 256kB sandbox */

static void sandbox_dump_memstate(void)
{
#if 0
    fprintf(stderr, "Total allocated: %ld\n", (long) total_allocated);
    fflush(stderr);
#endif
}

struct sandbox_data
{
    size_t total_allocated = 0;
};

static void *sandbox_alloc(void *udata, duk_size_t size)
{
    alloc_hdr *hdr;

    sandbox_data* data = (sandbox_data*)udata;

    if (size == 0)
    {
        return NULL;
    }

    if(data->total_allocated + size > max_allocated)
    {
        fprintf(stderr, "Sandbox maximum allocation size reached, %ld requested in sandbox_alloc\n",
                (long) size);
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

static void *sandbox_realloc(void *udata, void *ptr, duk_size_t size)
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
                fprintf(stderr, "Sandbox maximum allocation size reached, %ld requested in sandbox_realloc\n",
                        (long) size);
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
                fprintf(stderr, "Sandbox maximum allocation size reached, %ld requested in sandbox_realloc\n",
                        (long) size);
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

static void sandbox_free(void *udata, void *ptr)
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

static void sandbox_fatal(void *udata, const char *msg)
{
    (void) udata;  /* Suppress warning. */
    fprintf(stderr, "FATAL: %s\n", (msg ? msg : "no message"));
    fflush(stderr);
    exit(1);  /* must not return */
}

#endif // MEMORY_SANDBOX_HPP_INCLUDED
