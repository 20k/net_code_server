#include "exec_context.hpp"
#include "memory_sandbox.hpp"

void exec_context::create_as_sandbox()
{
    ctx = (void*)create_sandbox_heap();
}

void exec_context::destroy()
{
    duk_destroy_heap((duk_context*)ctx);
}

void* exec_context::get_ctx()
{
    return ctx;
}
