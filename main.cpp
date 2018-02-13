#include <iostream>
#include "duktape.h"

#include <iostream>
#include <SFML/Graphics.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui-SFML.h>
#include <imgui/imgui_internal.h>
#include <vec/vec.hpp>
//#include <serialise/serialise.hpp>
//#include <js/manager.hpp>
#include <math.h>
#include <js/font_renderer.hpp>
#include <4space_server/networking.hpp>

#include <js/js_interop.hpp>
#include <js/ui_util.hpp>

#include <algorithm>
#include "seccallers.hpp"

///i think something is broken with 7.2s stringstream implementation
///i dont know why the stringstream version crashes
std::vector<std::string> no_ss_split(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

void init_js_interop(stack_duk& sd, const std::string& js_data)
{
    sd.ctx = js_interop_startup();

    /*register_function(sd, js_data, "mainfunc");

    call_global_function(sd, "mainfunc");

    sd.save_function_call_point();*/
}

bool is_valid_string(const std::string& to_parse)
{
    bool check_digit = true;

    for(char c : to_parse)
    {
        if(check_digit && isdigit(c))
        {
            return false;
        }

        check_digit = false;

        if(!isalnum(c))
        {
            return false;
        }
    }

    return true;
}

std::string get_script_from_name_string(const std::string& base_dir, const std::string& name_string)
{
    std::string to_parse = strip_whitespace(name_string);

    int num_dots = std::count(to_parse.begin(), to_parse.end(), '.');

    if(num_dots != 1)
    {
        return "";
    }

    std::vector<std::string> strings = no_ss_split(to_parse, ".");

    if(strings.size() != 2)
        return "";

    bool all_valid = true;

    for(auto& str : strings)
    {
        if(!is_valid_string(str))
            all_valid = false;
    }

    if(!all_valid)
        return "";

    std::replace(to_parse.begin(), to_parse.end(), '.', '/');

    return read_file(base_dir + "/" + to_parse + ".js");
}

void tests()
{
    std::vector<std::string> strings = no_ss_split("test.hello", ".");

    std::string base = "./scripts";

    std::string s1_data = get_script_from_name_string(base, "i20k.test");
    std::string s2_data = read_file(base + "/i20k/test.js");

    std::cout << s1_data << " s1\n\n" << s2_data << std::endl;

    std::string s3_data = get_script_from_name_string(base, "i20k.2test");
    std::string s4_data = read_file(base + "/i20k/2test.js");

    assert(s1_data == s2_data);

    assert(s3_data == "" && s3_data != s4_data);
}

void test_compile(stack_duk& sd, const std::string& data)
{
    std::string prologue = "function INTERNAL_TEST()\n{'use strict'\nvar IVAR = ";
    std::string endlogue = "\n\nreturn IVAR();\n\n}\n";

    std::string wrapper = prologue + data + endlogue;

    std::cout << wrapper << std::endl;

    duk_push_string(sd.ctx, wrapper.c_str());
    duk_push_string(sd.ctx, "test-name");

    //DUK_COMPILE_FUNCTION
    if (duk_pcompile(sd.ctx, DUK_COMPILE_FUNCTION | DUK_COMPILE_STRICT) != 0)
    {
        printf("compile failed: %s\n", duk_safe_to_string(sd.ctx, -1));
    }
    else
    {
        duk_pcall(sd.ctx, 0);      /* [ func ] -> [ result ] */
        printf("program result: %s\n", duk_safe_to_string(sd.ctx, -1));
    }

    duk_pop(sd.ctx);
}

int main()
{
    bool is_bot = false;

    /*if(is_bot)
    {
        std::string jsfile = read_file(bot_js);

        register_function(sd, jsfile, "botjs");
        bot_id = call_global_function(sd, "botjs");
    }*/

    tests();

    std::string base_scripts_directory = "./scripts";

    std::string data = read_file("test.js");

    stack_duk sd;
    init_js_interop(sd, data);
    register_funcs(sd.ctx);

    std::string data_2 = read_file("test.js");

    test_compile(sd, data_2);

    //tests();


    arg_idx global_object = sd.push_global_object();

    //arg_idx gs_id = call_function_from_absolute(sd, "game_state_make");
    //arg_idx cm_id = call_function_from_absolute(sd, "card_manager_make");

    /*printf("gs cm %i %i\n", gs_id.val, cm_id.val);

    call_function_from_absolute(sd, "game_state_generate_new_game", gs_id, cm_id);
    ///does not return
    ///so we can get it off the stack
    sd.pop_n(1);*/

    /*call_function_from_absolute(sd, "debug", gs_id);


    arg_idx mainfunc = call_function_from(sd, "mainfunc", global_object);

    command c = command::invoke_bot_command(sd, bot_id, gs_id, mainfunc, current_player);

    commands.add(c);

    sd.pop_n(1);*/


    return 0;
}
