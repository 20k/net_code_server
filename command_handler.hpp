#ifndef COMMAND_HANDLER_HPP_INCLUDED
#define COMMAND_HANDLER_HPP_INCLUDED

#include <string>

#include "user.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"

///will need account here as well

inline
void init_js_interop(stack_duk& sd, const std::string& js_data)
{
    sd.ctx = js_interop_startup();
}

struct unsafe_info
{
    user* usr;
    std::string command;
    duk_context* ctx;

    std::string ret;
};

inline
duk_ret_t unsafe_wrapper(duk_context* ctx, void* udata)
{
    unsafe_info* info = (unsafe_info*)udata;

    std::string ret = js_unified_force_call_data(info->ctx, info->command, info->usr->name);

    info->ret = ret;

    return 0;
}

inline
std::string run_in_user_context(user& usr, const std::string& command)
{
    stack_duk sd;
    init_js_interop(sd, std::string());

    fully_freeze(sd.ctx, "JSON", "Array", "parseInt", "parseFloat", "Math", "Date", "Error", "Number");

    startup_state(sd.ctx, usr.name, usr.name, "invoke");

    unsafe_info inf;
    inf.usr = &usr;
    inf.command = command;
    inf.ctx = sd.ctx;

    if(duk_safe_call(sd.ctx, unsafe_wrapper, (void*)&inf, 0, 1) != 0)
    {
        printf("Err in safe wrapper %s\n", duk_safe_to_string(sd.ctx, -1));
    }

    duk_pop(sd.ctx);

    std::string ret = inf.ret;

    return ret;
}

struct command_handler_state
{
    user current_user;
};

///context?
std::string handle_command(command_handler_state& state, const std::string& str);

#endif // COMMAND_HANDLER_HPP_INCLUDED
