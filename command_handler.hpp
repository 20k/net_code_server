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

    /*register_function(sd, js_data, "mainfunc");

    call_global_function(sd, "mainfunc");

    sd.save_function_call_point();*/
}

inline
std::string add_freeze(const std::string& name)
{
    /*std::string freezy = "Object.defineProperty( global, \"" + name + "\", {\n"
                          "value: global." + name + ",\n"
                          "writable: false,\n"
                          "enumerable: true,\n"
                          "configurable: true\n"
                        "});";*/

    return " global." + name + " = deepFreeze(global." + name + ");\n";// + " " + freezy;
}

inline
void do_freeze(duk_context* ctx, const std::string& name, std::string& script_accumulate)
{
    /*duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, name.c_str());
    duk_freeze(ctx, -1);
    duk_pop(ctx);
    duk_pop(ctx);*/

    duk_push_global_object(ctx);
    //duk_get_prop_string(ctx, -1, name.c_str());

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

    /*duk_push_global_stash(sd.ctx);
    duk_push_global_object(sd.ctx);
    duk_put_prop_string(sd.ctx, -2, "global_object");
    duk_pop(sd.ctx);*/

    #if 1
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

    //freeze_script += add_freeze("JSON");
    /*freeze_script += add_freeze("Array");
    freeze_script += add_freeze("parseInt");
    freeze_script += add_freeze("parseFloat");
    freeze_script += add_freeze("Math");
    freeze_script += add_freeze("Date");*/
    //freeze_script += add_freeze("Error");
    //freeze_script += add_freeze("Number");

    do_freeze(sd.ctx, "JSON", freeze_script);
    do_freeze(sd.ctx, "Array", freeze_script);
    do_freeze(sd.ctx, "parseInt", freeze_script);
    do_freeze(sd.ctx, "parseFloat", freeze_script);
    do_freeze(sd.ctx, "Math", freeze_script);
    do_freeze(sd.ctx, "Date", freeze_script);
    do_freeze(sd.ctx, "Error", freeze_script);
    do_freeze(sd.ctx, "Number", freeze_script);

    std::cout << freeze_script << std::endl;

    duk_int_t res = duk_peval_string(sd.ctx, freeze_script.c_str());

    if(res != 0)
    {
        std::string err = duk_safe_to_string(sd.ctx, -1);

        printf("eval failed: %s\n", err.c_str());
    }
    else
    {
        printf("evaled\n");

        duk_pop(sd.ctx);
    }
    #endif

    register_funcs(sd.ctx);

    #if 1
    ///not actually working
    ///may need to do this from JS
    printf("prefreeze\n");

    //do_freeze(sd.ctx, "JSON");
    /*do_freeze(sd.ctx, "ARRAY");
    do_freeze(sd.ctx, "Array");
    do_freeze(sd.ctx, "parseInt");
    do_freeze(sd.ctx, "parseFloat");
    do_freeze(sd.ctx, "MATH");
    do_freeze(sd.ctx, "Math");
    do_freeze(sd.ctx, "DATE");
    do_freeze(sd.ctx, "Date");
    do_freeze(sd.ctx, "Error");
    do_freeze(sd.ctx, "Number");*/

    #endif // 0

    #if 0
    std::string script = command;

    script_info script_inf;
    script_inf.name = script; ///critical

    ///these two lines would be for a regular run, and the else condition
    ///would be "script doesn't exist"
    /*if(script_inf.exists_in_db())
        script_inf.load_from_db();*/

    if(script_inf.exists_in_db())
        script_inf.load_from_db();
    #endif

    /*std::string data_source = get_script_from_name_string(base_scripts_string, script);

    ///#UP
    script_inf.load_from_unparsed_source(sd.ctx, data_source, script);

    ///#UP
    script_inf.overwrite_in_db();


    ///need to check we have permission
    std::string data = script_inf.parsed_source;

    std::cout << "data\n" << data << std::endl;

    if(data == "")
    {
        return "Invalid Script";
    }*/

    //std::vector<std::string> parts = no_ss_split(script, ".");

    startup_state(sd.ctx, usr.name, usr.name, "invoke");

    std::string ret = js_unified_force_call_data(sd.ctx, command, usr.name);

    //std::string ret = js_unified_force_call(sd.ctx, script);

    //std::string ret = compile_and_call(sd, data, false, get_caller(sd.ctx));

    return ret;
}


struct command_handler_state
{
    user current_user;
};

///context?
std::string handle_command(command_handler_state& state, const std::string& str);

#endif // COMMAND_HANDLER_HPP_INCLUDED
