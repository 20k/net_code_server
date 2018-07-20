#include <iostream>
#include "scripting_api.hpp"
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
#include <secret/one_shot_core.hpp>

#include <libncclient/nc_util.hpp>
#include <secret/secret.hpp>
#include <secret/low_level_structure.hpp>

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

            duk_context* ctx = duk_create_heap_default();
            register_funcs(ctx, 0, "core");

            script_inf.load_from_unparsed_source(ctx, data_source, script, true, false);

            std::cout << script_inf.parsed_source << std::endl;

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);
            script_inf.overwrite_in_db(mongo_ctx);

            duk_destroy_heap(ctx);

            std::cout << "uploaded " << script << std::endl;
        }
        else
        {
            {
                mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

                if(!current_user.exists(mongo_user_info, current_user.name))
                    continue;
            }

            std::string ret = run_in_user_context(current_user.name, command, std::nullopt);

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

    std::cout << handle_autocompletes_json(usr.name, "server_scriptargs_json cash.steal");
}

void test_deadlock_detection()
{
    printf("predl\n");

    mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
    ctx.change_collection("i20k");
    mongo_lock_proxy ctx2 = get_global_mongo_user_accessible_context(-2);
    mongo_lock_proxy ctx3 = get_global_mongo_user_info_context(-3);
    ctx3.change_collection("i20k");

    printf("postdl\n");
}

void test_correct_collection_locking()
{
    mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
    ctx.change_collection("i20k");
    mongo_lock_proxy ctx2 = get_global_mongo_user_accessible_context(-2);
    mongo_lock_proxy ctx3 = get_global_mongo_user_info_context(-3);
    ctx3.change_collection("i20k8");
}

void test_locking()
{
    mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
    ctx.change_collection("i20k");

    std::mutex mut;

    safe_lock_guard guard(mut);
}

///making sure this ends up in the right repo
int main()
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::cout << std::hash<std::string>{}("aaaaaaaa") << std::endl;

    stack_on_start();

    lg::set_logfile("./log.txt");

    #ifdef TESTING
    parse_source();
    #endif // TESTING

    get_global_structure();

    sample_game_structure();

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

    low_level_structure_manager& manage = get_global_low_level_structure_manager();
    manage.erase_intersystem_specials();
    manage.for_each(low_level_structure::cleanup_invalid_users);

    //manage.erase_all();
    //manage.generate_up_to(150);

    #define NEW_GEN
    #ifdef NEW_GEN
    manage.harvest_existing_npcs();
    #endif // NEW_GEN

    manage.connect_systems_together();

    //manage.erase_intersystem_specials();
    //manage.for_each(low_level_structure::ensure_intersystem_npcs, 3);
    //manage.for_each(low_level_structure::layout_internal_users);

    //#define REGEN_SCRIPTS
    #ifdef REGEN_SCRIPTS
    int nid = 0;
    for_each_item([&](item& it)
                  {
                        if(!it.has("unparsed_source"))
                            return;

                        std::string unparsed_source = it.get_prop("unparsed_source");

                        //std::string parsed_source = parse_script(, ).parsed_source;

                        script_data data = parse_script("temp" + std::to_string(nid++), attach_unparsed_wrapper(unparsed_source), true);

                        if(!data.valid)
                            return;

                        it.set_prop("parsed_source", data.parsed_source);

                        {
                            mongo_lock_proxy ctx = get_global_mongo_user_items_context(-2);

                            it.overwrite_in_db(ctx);
                        }
                  });
    #endif // REGEN_SCRIPTS

    //test_correct_collection_locking();

    //test_deadlock_detection();


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
    //test_locking();

    boot_connection_handlers();


    //#define PROVOKE_CRASH
    #ifdef PROVOKE_CRASH


    for_each_item([&](item& it)
                  {
                        mongo_lock_proxy ctx = get_global_mongo_user_items_context(-2);

                        it.overwrite_in_db(ctx);
                  });

    #endif

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

    for_each_user([](user& u1)
                  {
                        mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
                        ctx.change_collection(u1.get_call_stack().back());

                        strip_old_msg_or_notif(ctx);
                  });
    #endif // TESTING

    //#ifndef TESTING
    start_npc_thread();
    init_purple_whale();
    init_blue_mantis();
    //#endif // TESTING

    printf("post\n");

    while(1)
    {
        std::string command;


        std::getline(std::cin, command);

        if(starts_with(command, "#rename"))
        {
            std::cout << "enter user start name " << std::endl;

            std::string start;

            std::getline(std::cin, start);

            std::cout << "enter user to name" << std::endl;

            std::string fin;
            std::getline(std::cin, fin);

            std::cout << "rename " << start << " " << fin << "? y/n" << std::endl;

            std::string sure;
            std::getline(std::cin, sure);

            if(starts_with(sure, "y"))
            {
                std::cout << rename_user_force(start, fin) << std::endl;
            }
            else
            {
                std::cout << "did nothing " << std::endl;
            }

            continue;
        }

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
