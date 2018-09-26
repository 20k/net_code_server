#include <iostream>
#include "scripting_api.hpp"
#include "stacktrace.hpp"

#include <iostream>
//#include <imgui/imgui.h>
//#include <imgui/imgui-SFML.h>
//#include <imgui/imgui_internal.h>
#include <vec/vec.hpp>
//#include <serialise/serialise.hpp>
//#include <js/manager.hpp>
#include <math.h>
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
#include <windows.h>
#include "db_storage_backend.hpp"

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

void dump_test()
{
    timestamp_move_queue q;

    nlohmann::json fun;
    fun = q;

    std::string str = fun.dump();

    std::cout << "test dump " << str << " sep dump " << nlohmann::json(q).dump() << " incorrect dump " << nlohmann::json{q} << std::endl;

    exit(0);
}

void tickle_cache()
{
    for_each_npc([](npc_user& usr)
    {
        npc_prop_list props;

        mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

        props.load_from_db(ctx, usr.name);

        get_user_and_nodes(usr.name, -2);
    });
}

///making sure this ends up in the right repo
int main()
{
    startup_tls_state();

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::cout << std::hash<std::string>{}("aaaaaaaa") << std::endl;

    stack_on_start();

    lg::set_logfile("./log.txt");

    #ifdef TESTING
    parse_source();
    #endif // TESTING

    get_global_structure();
    get_global_structure_info();

    sample_game_structure();

    /*http://www.karldiab.com/3DPointPlotter/
    for(int i=0; i < 2000; i++)
    {
        vec3f pos = sample_game_structure();

        printf("%.0f,%.0f,%.0f,", pos.x(), pos.y(), pos.z());
    }*/

    printf("\n");

    test_hexbin();
    initialse_mongo_all();

    #ifdef TESTING
    init_db_storage_backend();
    #endif // TESTING

    db_storage_backend::run_tests();

    //test_json();

    //#define SERVER_FIRST_TIME_EVER_RELEASE
    #ifdef SERVER_FIRST_TIME_EVER_RELEASE
    #define FIXJOIN_CHANNELS
    #define REGENERATE_LINKS_AND_USERS
    #endif // SERVER_FIRST_TIME_EVER_RELEASE

    //#define FIXJOIN_CHANNELS
    #ifdef FIXJOIN_CHANNELS
    run_in_user_context("core", "#msg.manage({create:\"local\"})", std::nullopt);
    run_in_user_context("core", "#msg.manage({create:\"global\"})", std::nullopt);
    run_in_user_context("core", "#msg.manage({create:\"help\"})", std::nullopt);
    run_in_user_context("core", "#msg.manage({create:\"memes\"})", std::nullopt);

    for_each_user([](user& usr)
                  {
                        run_in_user_context(usr.name, "#msg.manage({leave:\"0000\"})", std::nullopt);
                        //run_in_user_context(usr.name, "#msg.manage({leave:\"memes\"})", std::nullopt);
                        run_in_user_context(usr.name, "#msg.manage({leave:\"7001\"})", std::nullopt);
                        run_in_user_context(usr.name, "#msg.manage({join:\"local\"})", std::nullopt);
                        run_in_user_context(usr.name, "#msg.manage({join:\"global\"})", std::nullopt);
                        run_in_user_context(usr.name, "#msg.manage({join:\"help\"})", std::nullopt);
                        run_in_user_context(usr.name, "#msg.manage({join:\"memes\"})", std::nullopt);
                  });
    #endif // FIXJOIN_CHANNELS

    //dump_test();

    /*for_each_npc([](user& usr)
                 {
                    mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

                    usr.overwrite_user_in_db(ctx);
                 });*/

    npc_manager& npc_manage = get_global_npc_manager();


    /*{
        mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);
        std::cout << "num_npcs " << npc_manage.npc_number(ctx) << std::endl;
    }*/

    //npc_manage.delete_npcs_over(MAX_NPC_COUNT);

    low_level_structure_manager& manage = get_global_low_level_structure_manager();
    //manage.erase_intersystem_specials();
    //manage.for_each(low_level_structure::cleanup_invalid_users);


    #ifdef DELETE_DISCONNECTED_USERS
    for_each_npc([&](npc_user& usr)
                 {
                        auto sys_opt = manage.get_system_of(usr.name);

                        if(!sys_opt.has_value())
                        {
                            command_handler_state state;

                            std::cout << "deleting " << usr.name << std::endl;

                            delete_user(state, usr.name, true);
                        }
                 });


    npc_manage.create_npcs_up_to(MAX_NPC_COUNT);
    #endif // DELETE_DISCONNECTED_USERS

    //manage.erase_all();
    //manage.generate_up_to(150);

    //#define FIXY_FIX_NPCS
    #ifdef FIXY_FIX_NPCS

    std::vector<std::string> all_users_and_npcs;

    for_each_user([&](user& usr)
                  {
                    mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);

                    all_users_and_npcs.push_back(usr.name);

                    usr.users_i_have_access_to.clear();

                    usr.overwrite_user_in_db(ctx);
                  });

    for_each_npc([&](npc_user& usr)
                 {
                    mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);

                    all_users_and_npcs.push_back(usr.name);

                    usr.users_i_have_access_to.clear();

                    usr.overwrite_user_in_db(ctx);
                 });

    for(std::string& user_name : all_users_and_npcs)
    {
        mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);

        user usr;

        if(!usr.load_from_db(ctx, user_name))
            continue;

        std::vector<std::string> owner_list = usr.owner_list;

        for(std::string& owner : owner_list)
        {
            user owned_user;

            if(!owned_user.load_from_db(ctx, owner))
                continue;

            owned_user.users_i_have_access_to.push_back(user_name);

            owned_user.overwrite_user_in_db(ctx);
        }
    }

    #endif // FIXY_FIX_NPCS

    /*#ifndef TESTING
    tickle_cache();
    #endif // TESTING*/

    //#define NEW_GEN
    #ifdef NEW_GEN
    manage.harvest_existing_npcs();
    #endif // NEW_GEN

    //manage.connect_systems_together();

    //manage.erase_intersystem_specials();
    //manage.for_each(low_level_structure::ensure_intersystem_npcs, 3);

    //#define REGENERATE_LINKS_AND_USERS
    #ifdef REGENERATE_LINKS_AND_USERS
    manage.erase_intersystem_specials();
    manage.for_each(low_level_structure::cleanup_invalid_users);

    manage.harvest_existing_npcs();

    manage.for_each(low_level_structure::layout_internal_users);

    manage.connect_systems_together();
    #endif // REGENERATE_LINKS_AND_USERS

    //#define DELETE_ANY_W
    #ifdef DELETE_ANY_W
    for_each_npc([](npc_user& npc)
    {
        std::string name = npc.name;

        if(name.size() < 2)
            return;

        if(name.back() == 'w' && name[(int)name.size() - 2] == '_')
        {
            std::cout << "name " << name << std::endl;

            command_handler_state state;
            delete_user(state, name, true);
        }
    });
    #endif // DELETE_ANY_W

    //#define REGEN_W
    #ifdef REGEN_W
    manage.erase_intersystem_specials();
    manage.connect_systems_together();
    #endif // REGEN_W

    //#define FIX_BAD_ITEMS
    #ifdef FIX_BAD_ITEMS

    for_each_npc([](npc_user& usr)
                 {
                    std::vector<std::string> items = usr.get_all_items();

                    bool any = false;

                    for(int i=0; i < (int)items.size(); i++)
                    {
                        mongo_lock_proxy ctx = get_global_mongo_user_items_context(-2);

                        if(!item().load_from_db(ctx, items[i]))
                        {
                            std::cout << "bad item " << items[i] << " on " << usr.name << std::endl;

                            any = true;

                            items.erase(items.begin() + i);
                            i--;
                            continue;
                        }
                    }

                    if(any)
                    {
                        usr.upgr_idx = array_to_str(items);
                        usr.loaded_upgr_idx = "";

                        {
                            mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

                            usr.overwrite_user_in_db(ctx);
                        }
                    }
                 });

    #endif // FIX_BAD_ITEMS

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


    #ifdef TESTING
    system("start test_launch.bat");
    #endif // TESTING

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

                        try
                        {
                            props.get_as<std::vector<float>>("vals");
                            props.get_as<std::vector<int>>("props");
                        }
                        catch(...)
                        {
                            std::cout << "npc " << npc.name << " broken " << std::endl;

                            props.set_as<std::vector<int>>("props", {});
                            props.set_as<std::vector<float>>("vals", {});

                            {
                                mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

                                props.overwrite_in_db(ctx);
                            }

                            command_handler_state temp;
                            delete_user(temp, npc.name, true);
                        }

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
    user::launch_pump_events_thread();
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

        if(starts_with(command, "#down"))
        {
            std::string item_id;

            std::cout << "enter full item id, eg phobetor.hack\n";

            std::getline(std::cin, item_id);

            item id;

            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                if(!id.load_from_db(mongo_ctx, item_id))
                {
                    std::cout << "no such item with id " << item_id << std::endl;
                    continue;
                }
            }

            write_all_bin(item_id, id.get_prop("unparsed_source"));
            continue;
        }

        if(starts_with(command, "#exfil"))
        {
            std::string exp = run_in_user_context("i20k", "#i20k.ast_debug({suffix:\"s\"})", std::nullopt);

            write_all_bin("db_exfil", exp);
            continue;
        }

        if(starts_with(command, "#shutdown"))
        {
            break;
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

        if(!usr.load_from_db(lock, command))
            continue;

        std::cout << "found" << std::endl;

        std::string key = usr.auth;

        write_all_bin(command + ".key", key);

        Sleep(50);
    }

    return 0;
    #endif

    //debug_terminal();

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
