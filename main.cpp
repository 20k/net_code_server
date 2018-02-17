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

#include <algorithm>
#include "script_util.hpp"
#include "seccallers.hpp"
#include <string_view>

#include "mongo.hpp"
#include "user.hpp"

#include <assert.h>
#include "item.hpp"

#include "http_beast_server.hpp"
#include "command_handler.hpp"


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

    script_info script;
    script.load_from_disk_with_db_metadata("i20k.parse");

    std::cout << script.parsed_source << std::endl;
}

std::string run_script_as(const std::string& script, const std::string& user)
{
    stack_duk sd;
    init_js_interop(sd, std::string());
    register_funcs(sd.ctx);

    script_info script_inf;
    script_inf.load_from_disk_with_db_metadata(script);

    ///need to check we have permission
    std::string data = script_inf.parsed_source;

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
    mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context();

    user test_user;
    test_user.construct_new_user(mongo_user_info, "test_user");

    //test_user.load_from_db("test_user");

    user t2_user;
    t2_user.load_from_db(mongo_user_info, "test_user");

    for(int i=0; i < 10; i++)
    {
        item test_item;

        std::cout << test_item.get_new_id() << std::endl;
    }

    /*item insert_item;
    insert_item.generate_set_id();

    insert_item.set_prop("Test Key", 23);

    insert_item.create_in_db("what");*/

    item update_item;
    update_item.set_prop("item_id", 32);
    update_item.set_prop("Potato", "ostrich");

    update_item.update_in_db();

    item test_load;
    test_load.set_prop("item_id", 32);
    test_load.load_from_db();

    std::cout << test_load.get_prop("Potato") << std::endl;

    //std::cout << "found user " << t2_user.name << " cash " << t2_user.cash << std::endl;

    //test_user.cash = 1;

    //test_user.overwrite_user_in_db();

    //std::cout << test_user.exists("test_user2");
}

void debug_terminal()
{
    user current_user;

    while(1)
    {
        std::string command;

        std::getline(std::cin, command);

        std::string user_str = "user ";
        std::string exit_str = "exit ";
        std::string up_str = "#up ";

        if(command.substr(0, user_str.length()) == user_str)
        {
            std::vector<std::string> found = no_ss_split(command, " ");

            if(found.size() != 2)
                continue;

            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context();

            std::string username = found[1];

            if(current_user.exists(mongo_user_info, username))
            {
                current_user.load_from_db(mongo_user_info, username);

                std::cout << "logged in as " << username << std::endl;
            }
            else
            {
                current_user.construct_new_user(mongo_user_info, username);
                current_user.overwrite_user_in_db(mongo_user_info);

                std::cout << "created new user " << username << std::endl;
            }
        }
        else if(command.substr(0, exit_str.length()) == exit_str)
        {
            break;
        }
        else if(command.substr(0, up_str.length()) == up_str)
        {
            std::vector<std::string> found = no_ss_split(command, " ");

            if(found.size() != 2)
                continue;

            std::string script = found[1];

            script_info script_inf;

            std::string data_source = get_script_from_name_string(base_scripts_string, script);

            stack_duk csd;
            init_js_interop(csd, std::string());
            register_funcs(csd.ctx);

            script_inf.load_from_unparsed_source(csd.ctx, data_source, script);
            script_inf.overwrite_in_db();

            js_interop_shutdown(csd.ctx);

            std::cout << "uploaded " << script << std::endl;
        }
        else
        {
            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context();

            if(current_user.exists(mongo_user_info, current_user.name))
            {
                std::string ret = run_in_user_context(current_user, command);

                std::cout << ret << std::endl;
            }
        }
    }
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

    http_test_run();

    printf("post\n");

    while(1)
    {


    }

    return 0;

    debug_terminal();

    //user test_user;
    //test_user.construct_new_user("test_user2");
    //test_user.overwrite_user_in_db();

    #if 0
    user test_user3;
    test_user3.construct_new_user("test_user3");
    test_user3.cash = 1000.;
    test_user3.overwrite_user_in_db();

    user to_run_as;
    to_run_as.load_from_db("i20k");

    if(!to_run_as.exists("i20k"))
    {
        to_run_as.construct_new_user("i20k");
    }

    std::string str = run_in_user_context(to_run_as, "i20k.trustcheck");

    //std::string str = run_in_user_context(to_run_as, "test_user3.xfer_to_caller");

    std::cout << str << std::endl;
    #endif // 0

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

    //user_tests();

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
