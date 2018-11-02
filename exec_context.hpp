#ifndef EXEC_CONTEXT_HPP_INCLUDED
#define EXEC_CONTEXT_HPP_INCLUDED

#include <tuple>
#include "scripting_api.hpp"

template<typename F, typename... T>
inline
auto test_tuple(const F& f, const T&... hi) -> decltype(f(hi...))
{
    return f(hi...);
}

template<typename... T>
inline
duk_int_t function_fwrd(duk_context* ctx, void* args)
{
    const std::tuple<T...>& hi = *(std::tuple<T...>*)args;

    return std::apply(test_tuple<T...>, hi);
}

///eventually this will be the full scripting api neutral wrapper
struct exec_context
{
    void* ctx;

    void create_as_sandbox();
    void destroy();

    void* get_ctx();

    template<typename... T>
    int safe_exec(T&&... t)
    {
        std::tuple<T...> args(t...);

        int rval = duk_safe_call((duk_context*)get_ctx(), function_fwrd<T...>, &args, 0, 1);

        if(rval != 0)
        {
            duk_dup((duk_context*)get_ctx(), -1);

            printf("Err in safe wrapper %s\n", duk_safe_to_string((duk_context*)get_ctx(), -1));

            duk_pop((duk_context*)get_ctx());
        }

        return rval;
    }
};

exec_context* exec_from_ctx(duk_context* ctx);

#endif // EXEC_CONTEXT_HPP_INCLUDED
