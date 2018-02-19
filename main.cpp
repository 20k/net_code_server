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

#if 0
void user_tests()
{
    mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

    user test_user;
    test_user.construct_new_user(mongo_user_info, "test_user");

    //test_user.load_from_db("test_user");

    user t2_user;
    t2_user.load_from_db(mongo_user_info, "test_user");

    for(int i=0; i < 10; i++)
    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(-2);

        item test_item;

        std::cout << test_item.get_new_id(mongo_ctx) << std::endl;
    }

    /*item insert_item;
    insert_item.generate_set_id();

    insert_item.set_prop("Test Key", 23);

    insert_item.create_in_db("what");*/

    mongo_lock_proxy mongo_user_items = get_global_mongo_user_items_context(-2);

    item update_item;
    update_item.set_prop("item_id", 32);
    update_item.set_prop("Potato", "ostrich");

    update_item.update_in_db(mongo_user_items);

    item test_load;
    test_load.set_prop("item_id", 32);
    test_load.load_from_db(mongo_user_items);

    std::cout << test_load.get_prop("Potato") << std::endl;

    //std::cout << "found user " << t2_user.name << " cash " << t2_user.cash << std::endl;

    //test_user.cash = 1;

    //test_user.overwrite_user_in_db();

    //std::cout << test_user.exists("test_user2");
}
#endif // 0

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

            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

            std::string username = found[1];

            if(current_user.exists(mongo_user_info, username))
            {
                current_user.load_from_db(mongo_user_info, username);

                std::cout << "logged in as " << username << std::endl;
            }
            else
            {
                current_user.construct_new_user(mongo_user_info, username, "DUMMY_AUTH");
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

            //std::string fullname = current_user.name + "." + script;

            std::string fullname = script;

            std::cout << "loading " << fullname << std::endl;

            std::string data_source = get_script_from_name_string(base_scripts_string, strip_whitespace(fullname));

            stack_duk csd;
            init_js_interop(csd, std::string());
            register_funcs(csd.ctx, 0);

            script_inf.load_from_unparsed_source(csd.ctx, data_source, script);

            std::cout << script_inf.parsed_source << std::endl;

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);
            script_inf.overwrite_in_db(mongo_ctx);

            js_interop_shutdown(csd.ctx);

            std::cout << "uploaded " << script << std::endl;
        }
        else
        {
            {
                mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

                if(!current_user.exists(mongo_user_info, current_user.name))
                    continue;
            }

            std::string ret = run_in_user_context(current_user, command);

            std::cout << ret << std::endl;
        }
    }
}

int main()
{
    #if 1
    http_test_run();

    printf("post\n");

    while(1)
    {


    }

    return 0;
    #endif

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

    return 0;
}
