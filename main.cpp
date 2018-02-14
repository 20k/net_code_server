#include <iostream>
#include "duktape.h"

#include <iostream>
#include <SFML/Graphics.hpp>
//#include <imgui/imgui.h>
//#include <imgui/imgui-SFML.h>
//#include <imgui/imgui_internal.h>
#include <vec/vec.hpp>
//#include <serialise/serialise.hpp>
//#include <js/manager.hpp>
#include <math.h>
#include <js/font_renderer.hpp>
//#include <4space_server/networking.hpp>

#include <js/js_interop.hpp>
#include <js/ui_util.hpp>

#include <algorithm>
#include "script_util.hpp"
#include "seccallers.hpp"
#include <string_view>

#include "mongo.hpp"
#include "user.hpp"

#include <assert.h>
#include "item.hpp"

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

std::string run_script_as(const std::string& script, const std::string& user)
{
    stack_duk sd;
    init_js_interop(sd, std::string());
    register_funcs(sd.ctx);


    ///need to check we have permission
    std::string data = parse_script(get_script_from_name_string("./scripts", script));

    std::cout << data << std::endl;

    //std::cout << data.size() << std::endl;

    if(data == "")
    {
        return "Invalid Script";
    }

    std::vector<std::string> parts = no_ss_split(script, ".");

    startup_state(sd.ctx, user, parts[0], parts[1]);

    std::string ret = compile_and_call(sd, data, false, get_caller(sd.ctx));

    return ret;
}

void user_tests()
{
    user test_user;
    test_user.construct_new_user("test_user");

    //test_user.load_from_db("test_user");

    user t2_user;
    t2_user.load_from_db("test_user");

    for(int i=0; i < 10; i++)
    {
        item test_item;

        std::cout << test_item.get_new_id() << std::endl;
    }

    item insert_item;
    insert_item.generate_set_id();

    insert_item.set_prop("Test Key", 23);

    insert_item.create_in_db("what");

    //std::cout << "found user " << t2_user.name << " cash " << t2_user.cash << std::endl;

    //test_user.cash = 1;

    //test_user.overwrite_user_in_db();

    //std::cout << test_user.exists("test_user2");
}

int main()
{
    //mongo_tests("i20k_FDFDFDF_IMPOSSIBLE");

    //bool is_bot = false;

    /*if(is_bot)
    {
        std::string jsfile = read_file(bot_js);

        register_function(sd, jsfile, "botjs");
        bot_id = call_global_function(sd, "botjs");
    }*/

    //tests();

    /*std::string base_scripts_directory = "./scripts";

    std::string data = read_file("test.js");

    stack_duk sd;
    init_js_interop(sd, data);
    register_funcs(sd.ctx);

    startup_state(sd.ctx, "i20k");

    std::string data_2 = read_file("test.js");

    compile_and_call(sd, data_2, false, get_caller(sd.ctx));

    std::string data_3 = parse_script(get_script_from_name_string(base_scripts_directory, "i20k.parse"));

    std::string exec = compile_and_call(sd, data_3, false, get_caller(sd.ctx));

    std::cout << exec << std::endl;*/

    user_tests();

    //std::string ret = run_script_as("i20k.parse", "i20k");
    //std::cout << ret << std::endl;

    //tests();


    //arg_idx global_object = sd.push_global_object();

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
