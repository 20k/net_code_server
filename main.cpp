#include <iostream>
#include "stacktrace.hpp"

#include <iostream>
#include <vec/vec.hpp>
#include <math.h>

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
#include "db_storage_backend.hpp"
#include "auth.hpp"
#include "safe_thread.hpp"
#include "source_maps.hpp"
#include <secret/special_user_scripts.hpp>
#include "reoccurring_task_handler.hpp"
#include "serialisables.hpp"
#include <secret/structure_generation_2.hpp>
#include "command_handler_fiber_backend.hpp"
#include "db_storage_backend_lmdb.hpp"

void test_hexbin()
{
    std::string test_string = "012344981pioamj;slj;00\0dfdf\n";

    std::string hex = binary_to_hex(test_string);

    assert(hex_to_binary(hex) == test_string);
}

extern nlohmann::json handle_client_poll_json(user& usr);
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

    lock_type_t mut;

    safe_lock_guard guard(mut);
}

void tickle_cache()
{
    for_each_npc([](npc_user& usr)
    {
        npc_prop_list props;

        mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

        db_disk_load(ctx, props, usr.name);

        get_user_and_nodes(usr.name, -2);
    });
}

void tickle_item_cache()
{
    mongo_nolock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

    item it;
    db_disk_load(mongo_ctx, it, "0");
}

void termination_func()
{
    //if(std::uncaught_exceptions() > 0)

    FILE* pFile = fopen("crash.txt", "w");

    fclose(pFile);

    if(std::current_exception())
    {
        try
        {
            if(std::current_exception())
            {
                std::rethrow_exception(std::current_exception());
            }
        }
        catch(const std::exception& e)
        {
            std::cout << "Caught exception \"" << e.what() << "\"\n";
        }
    }
    else
        std::cout << "unexpected terminate\n";

    std::cout << "stacktracing\n";

    std::cout << get_stacktrace() << std::endl;
    system("pause");

    while(1){}
}

void handle_terminate()
{
    std::set_terminate(termination_func);
    std::set_unexpected(termination_func);
}

void pathfind_stresstest()
{
    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    sf::Clock clk;

    for(int i=0; i < 1000; i++)
    {
        auto plen = playspace_network_manage.get_accessible_path_to("i20k", "ast", "i20k", (path_info::path_info)(path_info::ALLOW_WARP_BOUNDARY | path_info::NONE));

        if(i == 0)
            std::cout << "plsize " << plen.size() << std::endl;
    }

    std::cout << "elapsed " << clk.getElapsedTime().asMilliseconds() << std::endl;

    while(1){}

    //exit(0);
}

void cleanup_notifs()
{
    get_noncritical_fiber_queue().add([]()
    {
        for_each_user([](user& u1)
                      {
                            mongo_nolock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
                            ctx.change_collection(u1.name);

                            strip_old_msg_or_notif(ctx);

                            fiber_sleep(1);
                      });

        printf("Finished stripping users\n");

        for_each_npc([](npc_user& u1)
                      {
                            mongo_nolock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
                            ctx.change_collection(u1.name);

                            strip_old_msg_or_notif(ctx);

                            fiber_sleep(1);
                      });

        printf("Finished all strip\n");
    });
}

void fix_users()
{
    for_each_user([](user& u1)
    {
        mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);
        ctx.change_collection(u1.name);

        u1.overwrite_user_in_db(ctx);
    });

    for_each_npc([](npc_user& u1)
    {
        mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);
        ctx.change_collection(u1.name);

        u1.overwrite_user_in_db(ctx);
    });
}

template<typename T>
void dedup_container(T& in)
{
    std::sort(in.begin(), in.end());
    auto last = std::unique(in.begin(), in.end());
    in.erase(last, in.end());
}

template<typename T, typename U>
auto find_by_val(const T& c, const U& val)
{
    return std::find(c.begin(), c.end(), val);
}

void undupe_items()
{
    for_each_user([](user& u1)
    {
        /*for_each_user([&u1](user& u2)
        {
            if(u1.name == u2.name)
                return;

            dedup_container(u2.loaded_upgr_idx);
            dedup_container(u2.upgr_idx);

            for(auto& i : u1.upgr_idx)
            {
                u2.remove_item(i);
            }

            mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);
            u2.overwrite_user_in_db(ctx);
        });*/

        u1.upgr_idx = {};
        u1.loaded_upgr_idx = {};

        mongo_nolock_proxy ctx = get_global_mongo_user_info_context(-2);
        u1.overwrite_user_in_db(ctx);
    });
}

///making sure this ends up in the right repo
int main()
{
    handle_terminate();

    std::cout << std::hash<std::string>{}("aaaaaaaa") << std::endl;

    stack_on_start();


    #ifdef USE_FIBERS

    printf("IN IFDEF\n");

    boot_fiber_manager();
    #endif // USE_FIBERS


    /*{
        nlohmann::json test;
        test["hi"] = 76561197983690027;

        std::vector<unsigned char> vdumped = nlohmann::json::to_cbor(test);

        std::string dumped(vdumped.begin(), vdumped.end());

        nlohmann::json dpp = nlohmann::json::from_cbor(dumped);

        std::cout << "DPP " << dpp["hi"] << std::endl;
    }*/

    /*sthread([]()
                {
                    //Sleep(5000);
                    raise(SIGSEGV);
                }).detach();*/

    //raise(SIGSEGV);

    ///we don't talk about this
    ///don't delete this line of code
    ///yes its looks stupid
    ///yes its 100% necessary
    ///I think this will force everything into high res clock mode
    sf::sleep(sf::milliseconds(1));

    lg::set_logfile("./log.txt");

    source_map_init();

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
    initialse_db_all();
    db_tests();

    //#ifdef TESTING
    init_db_storage_backend();
    //#endif // TESTING

    //undupe_items();

    //fix_users();

    sthread([]()
           {
            tickle_item_cache();
           }).detach();

    {
        mongo_lock_proxy usr = get_global_mongo_user_info_context(-2);

        user me;
        me.load_from_db(usr, "i20k");
        me.cash = 10000;
        me.overwrite_user_in_db(usr);
    }

    get_global_task_handler().register_task(cleanup_notifs, 60*60*24);

    #ifndef TESTING
    //convert_all_nodes();
    #endif // TESTING

    //test_json();

    #define INIT_TEST_BACK
    #ifdef INIT_TEST_BACK
    //dungeon_generator dgen;
    //dgen.make(40, 10, 0);

    //city_generator cgen;
    //cgen.make(50, 0, "mcmurdo");
    #endif // INIT_TEST_BACK

    //#define SERVER_FIRST_TIME_EVER_RELEASE
    #ifdef SERVER_FIRST_TIME_EVER_RELEASE
    //#define FIXJOIN_CHANNELS
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
    #endif // DELETE_DISCONNECTED_USERS]

    #ifdef CHECK_DISCO_NPCS
    int num_broken = 0;

    for_each_npc([&](npc_user& usr)
    {
        auto sys_opt = manage.get_system_of(usr.name);

        if(!sys_opt.has_value())
        {
            /*command_handler_state state;

            std::cout << "deleting " << usr.name << std::endl;

            delete_user(state, usr.name, true);*/

            num_broken++;
        }
    });

    std::cout << "num broken " << num_broken << std::endl;
    #endif // FIX_BROKEN_NPCS

    #ifdef SERVER_FIRST_TIME_EVER_RELEASE
    manage.erase_all();
    manage.generate_up_to(150);

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_chat_channel_propeties_context(-2);

        std::vector<std::string> chans{"global", "local", "help", "memes"};

        for(auto& i : chans)
        {
            mongo_requester to_insert;
            to_insert.set_prop("channel_name", i);

            if(to_insert.fetch_from_db(mongo_ctx).size() == 0)
            {
                to_insert.set_prop("password", "");
                to_insert.set_prop("user_list", std::vector<std::string>());

                to_insert.insert_in_db(mongo_ctx);
            }
        }
    }

    #endif // SERVER_FIRST_TIME_EVER_RELEASE

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
    //manage.erase_intersystem_specials();
    //manage.for_each(low_level_structure::cleanup_invalid_users);

    manage.harvest_existing_npcs();

    manage.for_each(&low_level_structure::layout_internal_users);

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
                        usr.upgr_idx = items;
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

                        printf("Did %i\n", nid);
                  });
    #endif // REGEN_SCRIPTS

    //test_correct_collection_locking();

    //test_deadlock_detection();

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

    #ifdef PATHFIND_STRESS
    pathfind_stresstest();
    #endif // PATHFIND_STRESS

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
    /*for_each_npc([](npc_user& npc)
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
                     });*/

    #endif // TESTING

    //#ifndef TES TING
    start_npc_thread();
    init_purple_whale();
    init_blue_mantis();
    init_special_user_scripts();
    user::launch_pump_events_thread();
    //#endif // TESTING

    printf("post\n");

    while(1)
    {
        std::string command;

        std::getline(std::cin, command);

        if(command.size() == 0)
        {
            sf::sleep(sf::milliseconds(1000));
            continue;
        }

        if(command == "#exit")
            exit(0);

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
                //std::cout << rename_user_force(start, fin) << std::endl;
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

                if(!db_disk_load(mongo_ctx, id, item_id))
                {
                    std::cout << "no such item with id " << item_id << std::endl;
                    continue;
                }
            }

            write_all_bin(item_id, id.get_prop("unparsed_source"));
            continue;
        }

        if(starts_with(command, "#pdown"))
        {
            std::string item_id;

            std::cout << "enter full item id, eg phobetor.hack\n";

            std::getline(std::cin, item_id);

            item id;

            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                if(db_disk_load(mongo_ctx, id, item_id))
                {
                    std::cout << "no such item with id " << item_id << std::endl;
                    continue;
                }
            }

            write_all_bin(item_id, id.get_prop("parsed_source"));
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
            exit(0);
            break;
        }

        /*if(starts_with(command, "#dbg"))
        {
            std::cout << get_all_thread_stacktraces() << std::endl;
            continue;
        }*/

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

        std::string key = usr.get_auth_token_hex();

        write_all_bin(command + "_hex.key", key);

        sf::sleep(sf::milliseconds(50));
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

    //CoUninitialize();

    //tests();

    return 0;
}
