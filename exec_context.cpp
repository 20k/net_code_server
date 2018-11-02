#include "exec_context.hpp"
#include "memory_sandbox.hpp"

void exec_context::create_as_sandbox()
{
    ctx = (void*)create_sandbox_heap();

    duk_context* dctx = (duk_context*)ctx;

    duk_push_global_object(dctx);
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

    duk_push_global_object(dctx);

    duk_get_prop_string(dctx, -2, DUK_HIDDEN_SYMBOL("HPOINTER"));

    void* ptr = duk_get_pointer(dctx, -1);

    duk_pop(dctx);
    duk_pop(dctx);

    return (exec_context*)ptr;
}
