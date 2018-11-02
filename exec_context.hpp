#ifndef EXEC_CONTEXT_HPP_INCLUDED
#define EXEC_CONTEXT_HPP_INCLUDED

///eventually this will be the full scripting api neutral wrapper
struct exec_context
{
    void* ctx;

    void create_as_sandbox();

    void destroy();

    void* get_ctx();
};

#endif // EXEC_CONTEXT_HPP_INCLUDED
