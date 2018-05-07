#include <iostream>
#include "duktape.h"
#include "stacktrace.hpp"

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
#include "logging.hpp"

#include <secret/npc_manager.hpp>
#include <secret/structure.hpp>
#include <secret/one_shots.hpp>

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

            script_inf.load_from_unparsed_source(csd.ctx, data_source, script, true);

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

            std::string ret = run_in_user_context(current_user.name, command);

            std::cout << ret << std::endl;
        }
    }
}

void test_hexbin()
{
    std::string test_string = "012344981pioamj;slj;00\0dfdf\n";

    std::string hex = binary_to_hex(test_string);

    assert(hex_to_binary(hex) == test_string);
}

extern std::string handle_client_poll_json(user& usr);
//extern std::string handle_client_poll_json_old(user& usr);

void test_json()
{
    user usr;

    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

        usr.load_from_db(ctx, "i20k");
    }

    std::cout << handle_client_poll_json(usr) << std::endl;
    //std::cout << handle_client_poll_json_old(usr) << std::endl;

    std::cout << handle_autocompletes_json(usr, "server_scriptargs_json cash.steal");
}

///making sure this ends up in the right repo
int main()
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    stack_on_start();

    get_global_structure();

    /*http://www.karldiab.com/3DPointPlotter/
    for(int i=0; i < 2000; i++)
    {
        vec3f pos = sample_game_structure();

        printf("%.0f,%.0f,%.0f,", pos.x(), pos.y(), pos.z());
    }*/

    printf("\n");

    #ifdef TESTING
    system("start test_launch.bat");
    #endif // TESTING

    test_hexbin();
    initialse_mongo_all();
    test_json();

    lg::set_logfile("./log.txt");

    /*{
        std::vector<mongo_requester> all;

        {
            mongo_lock_proxy all_auth = get_global_mongo_global_properties_context(-2);

            mongo_requester request;

            request.exists_check["account_token"] = 1;

            all = request.fetch_from_db(all_auth);
        }

        for(auto& i : all)
        {
            auto users = str_to_array(i.get_prop("users"));

            for(std::string& usrname : users)
            {
                ///throwaway doesn't work here due to
                ///multi auth protection
                //run_in_user_context(usrname, "#msg.manage({join:\"0000\"})");
                //run_in_user_context(usrname, "#msg.manage({join:\"7001\"})");
                //run_in_user_context(usrname, "#msg.manage({join:\"memes\"})");

                {
                    mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

                    user usr;
                    usr.load_from_db(ctx, usrname);

                    usr.overwrite_user_in_db(ctx);
                }

                //std::cout << "proc username " << usrname << std::endl;
            }
        }
    }*/

    //#define DELETE_BANNED
    #ifdef DELETE_BANNED
    std::set<std::string> banned;

    for(auto& i : privileged_args)
    {
        std::string script_name = i.first;

        std::string str = no_ss_split(script_name, ".")[0];

        banned.insert(str);
    }

    banned.insert("db");

    command_handler_state temp;

    for(auto& i : banned)
    {
        std::cout << delete_user(temp, i, true) << std::endl;
        std::cout << "deleted " << i << std::endl;
    }

    #endif // DELETE_BANNED

    #if 1
    http_test_run();

    ///fix db screwup
    #ifndef TESTING
    for_each_npc([](npc_user& npc)
                     {
                        npc_prop_list props;

                        {
                            mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

                            props.load_from_db(ctx, npc.name);
                        }

                        props.force_conversion<std::vector<std::string>>("vals", from_string<float>);
                        props.force_conversion<std::vector<std::string>>("props", from_string<int>);

                        {
                            mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

                            props.overwrite_in_db(ctx);
                        }
                     });
    #endif // TESTING

    start_npc_thread();
    init_purple_whale();

    printf("post\n");

    while(1)
    {
        std::string command;

        std::getline(std::cin, command);

        std::cout << "Are you sure? y/n" << std::endl;

        std::string sure;

        std::getline(std::cin, sure);

        if(!starts_with(sure, "y"))
            continue;

        mongo_lock_proxy lock = get_global_mongo_user_info_context(-2);

        /*mongo_requester req;
        req.set_prop("name", command);

        auto res = req.fetch_from_db(lock);

        if(res.size() == 1)
        {
            mongo_requester found = res[0];

            std::string key = found.get_prop("auth");

            write_all_bin(command + ".key", key);

            std::cout << "found" << std::endl;
        }*/

        user usr;
        usr.load_from_db(lock, command);

        if(!usr.valid)
            continue;

        std::cout << "found" << std::endl;

        std::string key = usr.auth;

        write_all_bin(command + ".key", key);

        Sleep(50);
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

    CoUninitialize();

    //tests();

    return 0;
}
