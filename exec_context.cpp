#include "exec_context.hpp"
#include "memory_sandbox.hpp"

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
