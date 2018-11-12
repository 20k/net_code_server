#include "exec_context.hpp"
#include "memory_sandbox.hpp"
#include "seccallers.hpp"

void exec_context::create_as_sandbox()
{
    ctx = (void*)create_sandbox_heap();

    duk_context* dctx = (duk_context*)ctx;

    duk_push_heap_stash(dctx);
    duk_push_pointer(dctx, this);

    duk_put_prop_string(dctx, -2, DUK_HIDDEN_SYMBOL("HPOINTER"));

    duk_pop(dctx);
}

void exec_context::destroy()
{
    duk_destroy_heap((duk_context*)ctx);
}

void* exec_context::get_ctx()
{
    return ctx;
}

void exec_context::stash_context(const std::string& host, int seclevel, int stack_offset, void* new_context)
{
    duk_context* dctx = (duk_context*)get_ctx();

    duk_push_heap_stash(dctx);

    duk_dup(dctx, -1 + stack_offset);

    std::string key = host + "/" + std::to_string(seclevel);
    duk_put_prop_string(dctx, -2, key.c_str());

    duk_pop(dctx);

    stashed_contexts[host][seclevel] = new_context;
}

void* exec_context::get_new_context_for(const std::string& host, int seclevel)
{
    //void* ptr = stashed_contexts[host][seclevel];

    //if(ptr == nullptr)
    {
        duk_context* dctx = (duk_context*)get_ctx();

        duk_idx_t thr_idx = duk_push_thread_new_globalenv(dctx);
        duk_context* new_ctx = duk_get_context(dctx, thr_idx);
        //stash_context(host, seclevel, -1, new_ctx);

        register_funcs(new_ctx, seclevel, host, true);
        return (void*)new_ctx;
    }
    /*else
    {
        duk_context* dctx = (duk_context*)get_ctx();

        duk_push_heap_stash(dctx);

        std::string key = host + "/" + std::to_string(seclevel);

        duk_get_prop_string(dctx, -1, key.c_str());

        duk_remove(dctx, -2);

        register_funcs((duk_context*)ptr, seclevel, host, false);
        return ptr;
    }*/
}

exec_context* exec_from_ctx(duk_context* ctx)
{
    duk_context* dctx = (duk_context*)ctx;

    duk_push_heap_stash(dctx);

    duk_get_prop_string(dctx, -1, DUK_HIDDEN_SYMBOL("HPOINTER"));

    void* ptr = duk_get_pointer(dctx, -1);

    duk_pop(dctx);
    duk_pop(dctx);

    return (exec_context*)ptr;
}

exec_stack::exec_stack(exec_context& pctx, void* fresh) : ctx(pctx)
{
    backup = ctx.get_ctx();
    ctx.ctx = fresh;
}

exec_stack::~exec_stack()
{
    early_out();
}

void exec_stack::early_out()
{
    if(escaped)
        return;

    ctx.ctx = backup;

    escaped = true;
}
