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
std::string run_in_user_context(user& usr, const std::string& command)
{
    stack_duk sd;
    init_js_interop(sd, std::string());
    register_funcs(sd.ctx);

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
