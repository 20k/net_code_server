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
#include "script_util.hpp"
#include "seccallers.hpp"
#include <string_view>

void init_js_interop(stack_duk& sd, const std::string& js_data)
{
    sd.ctx = js_interop_startup();

    /*register_function(sd, js_data, "mainfunc");

    call_global_function(sd, "mainfunc");

    sd.save_function_call_point();*/
}


void tests()
{
    std::vector<std::string> strings = no_ss_split("test.hello", ".");

    std::string base = "./scripts";

    std::string s1_data = get_script_from_name_string(base, "i20k.test");
    std::string s2_data = read_file(base + "/i20k/test.js");

    //std::cout << s1_data << " s1\n\n" << s2_data << std::endl;

    std::string s3_data = get_script_from_name_string(base, "i20k.2test");
    std::string s4_data = read_file(base + "/i20k/2test.js");

    assert(s1_data == s2_data);

    assert(s3_data == "" && s3_data != s4_data);


    std::string parsed = parse_script(get_script_from_name_string(base, "i20k.parse"));

    std::cout << parsed << std::endl;
}

void test_compile(stack_duk& sd, const std::string& data)
{
    std::string prologue = "function INTERNAL_TEST()\n{'use strict'\nvar IVAR = ";
    std::string endlogue = "\n\nreturn IVAR();\n\n}\n";

    std::string wrapper = prologue + data + endlogue;

    //std::cout << wrapper << std::endl;

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

    std::string data_3 = parse_script(get_script_from_name_string(base_scripts_directory, "i20k.parse"));

    test_compile(sd, data_3);

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
