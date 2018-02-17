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

inline
std::string add_freeze(const std::string& name)
{
    return " global." + name + " = deepFreeze(global." + name + ");\n";
}

inline
void do_freeze(duk_context* ctx, const std::string& name, std::string& script_accumulate)
{
    duk_push_global_object(ctx);

    duk_push_string(ctx, name.c_str());

    duk_def_prop(ctx, -2,
             DUK_DEFPROP_HAVE_WRITABLE |
             DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_ENUMERABLE |
             DUK_DEFPROP_HAVE_CONFIGURABLE);

    duk_pop(ctx);

    script_accumulate += add_freeze(name);
}

inline
std::string run_in_user_context(user& usr, const std::string& command)
{
    stack_duk sd;
    init_js_interop(sd, std::string());

    std::string get_global = "var global = new Function(\'return this;\')();";

    std::string freeze_script = get_global + "\nfunction deepFreeze(o) {\n"
          "Object.freeze(o);\n"

          "Object.getOwnPropertyNames(o).forEach(function(prop) {\n"
            "if (o.hasOwnProperty(prop)\n"
            "&& o[prop] !== null\n"
            "&& (typeof o[prop] === \"object\" || typeof o[prop] === \"function\")\n"
            "&& !Object.isFrozen(o[prop])) {\n"
                "deepFreeze(o[prop]);\n"
              "}\n"
          "});\n"

         "return o;\n"
        "};\n";

    do_freeze(sd.ctx, "JSON", freeze_script);
    do_freeze(sd.ctx, "Array", freeze_script);
    do_freeze(sd.ctx, "parseInt", freeze_script);
    do_freeze(sd.ctx, "parseFloat", freeze_script);
    do_freeze(sd.ctx, "Math", freeze_script);
    do_freeze(sd.ctx, "Date", freeze_script);
    do_freeze(sd.ctx, "Error", freeze_script);
    do_freeze(sd.ctx, "Number", freeze_script);

    duk_int_t res = duk_peval_string(sd.ctx, freeze_script.c_str());

    if(res != 0)
    {
        std::string err = duk_safe_to_string(sd.ctx, -1);

        printf("eval failed: %s\n", err.c_str());
    }
    else
    {
        duk_pop(sd.ctx);
    }

    register_funcs(sd.ctx);

    startup_state(sd.ctx, usr.name, usr.name, "invoke");

    std::string ret = js_unified_force_call_data(sd.ctx, command, usr.name);

    return ret;
}


struct command_handler_state
{
    user current_user;
};

///context?
std::string handle_command(command_handler_state& state, const std::string& str);

#endif // COMMAND_HANDLER_HPP_INCLUDED
