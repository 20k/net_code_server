#include "command_handler.hpp"
#include "seccallers.hpp"
#include <thread>
#include <chrono>
#include <set>
#include "http_beast_server.hpp"
#include "memory_sandbox.hpp"
#include "auth.hpp"
#include "logging.hpp"
#include <iomanip>
#include "rng.hpp"
#include <secret/npc_manager.hpp>
#include <nlohmann/json.hpp>
#include <libncclient/nc_util.hpp>
#include "rate_limiting.hpp"
#include "privileged_core_scripts.hpp"
#include "shared_duk_worker_state.hpp"
#include "shared_data.hpp"
#include <atomic>
#include <SFML/System.hpp>
#include "shared_command_handler_state.hpp"
#include "duk_object_functions.hpp"
#include <secret/low_level_structure.hpp>
#include "safe_thread.hpp"
#include "mongo.hpp"
#include "quest_manager.hpp"
#include <secret/tutorial.hpp>
#include "steam_auth.hpp"
#include <networking/serialisable.hpp>
#include "serialisables.hpp"
#include "argument_object.hpp"
#include "command_handler_fiber_backend.hpp"
#include "chat_channels.hpp"
#include "event_manager.hpp"
#include "js_ui.hpp"
#include <toolkit/clock.hpp>

#ifndef __WIN32__
#include <unistd.h>
#endif // __WIN32__

struct unsafe_info
{
    std::string execute_as;
    std::string command;
    int finished = 0;
    js::value_context heap;
    js::value returned_val;

    std::string ret;

    #ifndef USE_QUICKJS
    unsafe_info() : returned_val(heap)
    #else
    unsafe_info() : heap(interrupt_handler, new sandbox_data), returned_val(heap)
    #endif
    {

    }
};

void unsafe_wrapper(unsafe_info& info)
{
    auto [val, msg] = js_unified_force_call_data(info.heap, info.command, info.execute_as);

    info.ret = msg;
    info.returned_val = std::move(val);
}

void managed_duktape_thread(unsafe_info* info, size_t tid)
{
    ///set thread storage hack
    ///convert from int to size_t
    *tls_get_thread_id_storage_hack() = (size_t)tid;

    try
    {
        unsafe_wrapper(*info);
    }
    catch(std::runtime_error& err)
    {
        std::cout << "GOT ERR " << err.what() << std::endl;
        info->ret = err.what();
    }
    catch(...)
    {
        std::cout << "Misc error" << std::endl;
        info->ret = "Server threw an exception of unknown type";
    }

    info->finished = 1;
}

struct cleanup_auth_at_exit
{
    lock_type_t& to_lock;
    std::map<std::string, int>& to_cleanup;
    std::string auth;
    bool blocked = true;

    cleanup_auth_at_exit(lock_type_t& lk, std::map<std::string, int>& cleanup, std::string ath) : to_lock(lk), to_cleanup(cleanup), auth(ath) {}

    void unblock()
    {
        safe_lock_guard lk(to_lock);

        to_cleanup[auth] = 0;
        blocked = false;
    }

    ~cleanup_auth_at_exit()
    {
        if(!blocked)
            return;

        safe_lock_guard lk(to_lock);

        to_cleanup[auth] = 0;
    }
};

namespace script_management_mode
{
    enum mode
    {
        DEFAULT,
        REALTIME,
    };
}

template<typename T>
void async_realtime_script_handler(js::value_context& nvctx, js::value in_arg, command_handler_state& state, std::string& ret,
                                   int current_id, T& callback, std::shared_ptr<shared_command_handler_state>& all_shared)
{
    double current_framerate = js::get_heap_stash(nvctx).get("framerate_limit");

    steady_timer clk;

    js::value_context vctx(nvctx);

    sandbox_data* sand = js::get_sandbox_data<sandbox_data>(nvctx);
    sand->framerate_limit = current_framerate;

    /*MAKE_PERF_COUNTER();
    mongo_diagnostics diagnostic_scope;*/

    js::value args = js::xfer_between_contexts(vctx, in_arg);

    uint64_t last_sequence_id = 0;

    while(1)
    {
        try
        {
            {
                std::lock_guard guard(state.script_data_lock);

                last_sequence_id = state.script_data[current_id].client_seq_id;
            }

            sandbox_data* sand_data = js::get_sandbox_data<sandbox_data>(vctx);
            sand_data->sleep_realtime.reset();

            steady_timer elapsed;

            {
                std::lock_guard guard(state.script_data_lock);

                realtime_script_data& dat = state.script_data[current_id];

                for(auto& i : dat.realtime_ui.element_states)
                {
                    ui_element_state& st = i.second;

                    st.processed = true;
                }
            }

            bool any = false;

            if(args.has("on_wheelmoved"))
            {
                if(state.has_mousewheel_state(current_id))
                {
                    vec2f wheel = state.consume_mousewheel_state(current_id);

                    js::value local_args(vctx);
                    local_args["x"] = wheel.x();
                    local_args["y"] = wheel.y();

                    auto [success, result] = js::call_prop(args, "on_wheelmoved", local_args);

                    if(!success)
                    {
                        ret = (std::string)result;
                        break;
                    }
                }

                any = true;
            }

            if(args.has("on_resize"))
            {
                if(state.has_new_width_height(current_id))
                {
                    std::pair<int, int> width_height = state.consume_width_height(current_id);

                    width_height.first = clamp(width_height.first, 5, 400);
                    width_height.second = clamp(width_height.second, 5, 400);

                    js::value local_args(vctx);
                    local_args["width"] = width_height.first;
                    local_args["height"] = width_height.second;

                    auto [success, result] = js::call_prop(args, "on_resize", local_args);

                    if(!success)
                    {
                        ret = (std::string)result;
                        break;
                    }
                }

                any = true;
            }

            std::vector<unprocessed_key_info> unprocessed_text_input;

            {
                safe_lock_guard guard(state.script_data_lock);

                unprocessed_text_input = state.script_data[current_id].unprocessed_text_input;

                state.script_data[current_id].unprocessed_text_input.clear();
            }

            if(args.has("on_textinput"))
            {
                for(auto i : unprocessed_text_input)
                {
                    std::string c = i.key;

                    js::value local_args1(vctx);
                    local_args1 = c;

                    auto [success, result] = js::call_prop(args, "on_textinput", local_args1);

                    if(!success)
                    {
                        ret = (std::string)result;
                        break;
                    }
                }

                any = true;
            }

            std::vector<unprocessed_key_info> unprocessed_key_input;

            {
                safe_lock_guard guard(state.script_data_lock);

                unprocessed_key_input = state.script_data[current_id].unprocessed_key_input;

                state.script_data[current_id].unprocessed_key_input.clear();
            }

            if(args.has("on_input"))
            {
                for(auto i : unprocessed_key_input)
                {
                    std::string c = i.key;
                    bool is_repeat = i.is_repeat;

                    js::value local_args1(vctx);
                    local_args1 = c;

                    js::value local_args2(vctx);
                    local_args2 = is_repeat;

                    auto [success, result] = js::call_prop(args, "on_input", local_args1, local_args2);

                    if(!success)
                    {
                        ret = (std::string)result;
                        break;
                    }
                }

                any = true;
            }

            if(args.has("on_update"))
            {
                double current_dt = clk.restart() * 1000;

                js::value local_args(vctx);
                local_args = current_dt;

                auto [success, result] = js::call_prop(args, "on_update", local_args);

                if(!success)
                {
                    ret = (std::string)result;
                    break;
                }

                any = true;
            }

            if(args.has("on_draw"))
            {
                auto [success, result] = js::call_prop(args, "on_draw");

                if(!success)
                {
                    ret = (std::string)result;
                    break;
                }

                async_pipe(&vctx, (std::string)result);

                any = true;
            }

            {
                std::lock_guard guard(state.script_data_lock);

                realtime_script_data& dat = state.script_data[current_id];

                for(auto& i : dat.realtime_ui.element_states)
                {
                    ui_element_state& st = i.second;

                    if(!st.processed)
                        continue;

                    for(int kk=0; kk < (int)st.value.size(); kk++)
                    {
                        if(js_ui::is_edge_event(st.value[kk]))
                        {
                            st.value.erase(st.value.begin() + kk);
                            kk--;
                            continue;
                        }
                    }
                }
            }

            if(!any)
            {
                break;
            }

            ///handles promises
            #ifdef USE_QUICKJS
            vctx.execute_jobs();
            #endif // USE_QUICKJS

            ///need to set work units based on how much of elapsed frametime is used
            int sleep_mult = all_shared->live_work_units();

            {
                double frametime = (1/current_framerate) * 1000;

                double fiber_load = fiber_overload_factor();

                double allowed_executable_time = (1/4.f) * frametime;
                double sleep_time = (1 - (1/4.f)) * frametime;

                sleep_time += frametime * (sleep_mult - 1);
                sleep_time += frametime * (fiber_load - 1);

                ///did not forcibly rate limit
                if(!sand_data->sleep_realtime.exceeded_awake && allowed_executable_time > sand_data->sleep_realtime.awake_ms)
                {
                    sand_data->sleep_realtime.consume_remaining_time(allowed_executable_time, sleep_time);
                }
                else
                {
                    sand_data->sleep_realtime.consume_remaining_time(1, 3 + 4 * (sleep_mult - 1) + 4 * (fiber_load - 1));
                    sand_data->sleep_realtime.consume_remaining_time(0, sleep_time);
                }
            }

            if(callback(vctx, last_sequence_id))
                break;

            //float drdrdr = elapsed.get_elapsed_time_s() * 1000.;

            //printf("ELAPSED %f\n", drdrdr);
        }
        catch(...)
        {
            printf("Caught exception in async\n");
            break;
        }
    }
}

struct execution_blocker_guard
{
    std::optional<std::shared_ptr<shared_command_handler_state>>& shared;
    bool blocked = true;

    execution_blocker_guard(std::optional<std::shared_ptr<shared_command_handler_state>>& all_shared) : shared(all_shared)
    {
        if(all_shared)
            all_shared.value()->execution_is_blocked = true;
    }

    void unblock()
    {
        if(shared)
        {
            shared.value()->execution_is_blocked = false;
            shared.value()->execution_requested = false;
        }

        blocked = false;
    }

    ~execution_blocker_guard()
    {
        if(shared && blocked)
        {
            shared.value()->execution_is_blocked = false;
            shared.value()->execution_requested = false;
        }
    }
};

struct fiber_work_manager
{
    fiber_work_manager()
    {
        fiber_work::notify(1);
    }

    ~fiber_work_manager()
    {
        fiber_work::unnotify(1);
    }
};

std::vector<std::string> get_user_call_stack(const std::string& username)
{
    user usr;

    {
        mongo_read_proxy ctx = get_global_mongo_user_info_context(-2);

        if(!usr.load_from_db(ctx, username))
            return {username};
    }

    usr.cleanup_call_stack(-2);

    return usr.get_call_stack();
}

std::optional<std::string> get_auth_token_hex(const std::string& username, std::optional<std::shared_ptr<shared_command_handler_state>> all_shared)
{
    {
        user usr;
        mongo_read_proxy ctx = get_global_mongo_user_info_context(-2);

        if(usr.load_from_db(ctx, username))
            return usr.get_auth_token_hex();
    }

    if(all_shared.has_value())
    {
        return all_shared.value()->state.get_auth_hex();
    }

    return std::nullopt;
}

std::string run_in_user_context(std::string username, std::string command, std::optional<std::shared_ptr<shared_command_handler_state>> all_shared, std::optional<float> custom_exec_time_s, bool force_exec)
{
    try
    {
        execution_blocker_guard exec_guard(all_shared);

        static lock_type_t id_mut;

        static std::map<std::string, int> auth_guard;
        static std::atomic_int gthread_id{1};
        int32_t local_thread_id = gthread_id++;

        auto auth_opt = get_auth_token_hex(username, all_shared);

        if(!auth_opt.has_value())
            return "No auth token found in shared state or user";

        std::string auth_token = auth_opt.value();

        #ifndef LOCAL_IP
        if(!force_exec)
        {
            safe_lock_guard lk(id_mut);

            if(auth_guard[auth_token] == 1)
                return make_error_col("Cannot run two scripts at once in different contexts!");

            auth_guard[auth_token] = 1;
        }
        #endif // LOCAL_IP

        cleanup_auth_at_exit cleanup(id_mut, auth_guard, auth_token);
        fiber_work_manager fbwork;

        if(force_exec)
        {
            cleanup.unblock();
            exec_guard.unblock();
        }

        std::vector<std::string> call_stack = get_user_call_stack(username);

        unsafe_info inf;

        sandbox_data* sand_data = js::get_sandbox_data<sandbox_data>(inf.heap);

        shared_duk_worker_state* shared_duk_state = new shared_duk_worker_state;

        startup_state(inf.heap, call_stack.back(), call_stack.back(), "invoke", call_stack, shared_duk_state);

        js::get_heap_stash(inf.heap).get("thread_id") = local_thread_id;

        if(all_shared.has_value())
        {
            shared_data& shared = all_shared.value()->shared;

            js::value heap_stash = js::get_heap_stash(inf.heap);

            heap_stash["shared_data_ptr"].set_ptr(&shared);
            heap_stash["command_handler_state_pointer"].set_ptr(&all_shared.value()->state);
            heap_stash["all_shared_data"].allocate_in_heap(all_shared.value());

            sand_data->all_shared = all_shared.value();
        }
        else
        {
            js::value heap_stash = js::get_heap_stash(inf.heap);

            heap_stash["shared_data_ptr"].set_ptr(nullptr);
            heap_stash["command_handler_state_pointer"].set_ptr(nullptr);
            heap_stash["all_shared_data"].set_ptr(nullptr);
        }

        inf.execute_as = call_stack.back();
        inf.command = command;

        sand_data->is_static = true;
        sand_data->max_elapsed_time_ms = 5000;

        if(all_shared.has_value())
        {
            all_shared.value()->state.number_of_oneshot_scripts++;
        }

        if(custom_exec_time_s.has_value())
        {
            sand_data->max_elapsed_time_ms = custom_exec_time_s.value() * 1000;
        }

        script_management_mode::mode current_mode = script_management_mode::DEFAULT;

        #ifdef PERF_DIAGNOSTICS
        steady_timer runtime;
        #endif // PERF_DIAGNOSTICS

        managed_duktape_thread(&inf, local_thread_id);

        #ifdef PERF_DIAGNOSTICS
        std::cout << "TOTAL RUNTIME " << runtime.get_elapsed_time_s() * 1000 << std::endl;
        #endif // PERF_DIAGNOSTICS

        *tls_get_should_throw() = 0;

        if(all_shared.has_value())
        {
            all_shared.value()->state.number_of_oneshot_scripts_terminated++;
        }

        if(shared_duk_state->is_realtime())
        {
            current_mode = script_management_mode::REALTIME;

            printf("scooted into realtime mode\n");
        }

        bool launched_realtime = false;
        int launched_realtime_id = 0;

        if(inf.finished)
        {
            if(current_mode == script_management_mode::REALTIME && all_shared.has_value() && !sand_data->terminate_semi_gracefully && !sand_data->terminate_realtime_gracefully)
            {
                /*MAKE_PERF_COUNTER();
                mongo_diagnostics diagnostic_scope;*/

                exec_guard.unblock();
                cleanup.unblock();

                launched_realtime = true;

                int current_id = -1;

                {
                    db::read_write_tx tx;
                    current_id = db::get_next_id(tx);
                }

                printf("cid %i\n", current_id);

                launched_realtime_id = current_id;

                all_shared.value()->state.add_realtime_script(current_id);

                bool is_valid = !inf.returned_val.is_undefined();

                if(is_valid)
                {
                    js::get_heap_stash(inf.heap)["realtime_id"] = current_id;

                    command_handler_state& cstate = all_shared.value()->state;

                    bool last_use_square_font = false;

                    ///pipe window size
                    {
                        auto [width, height] = shared_duk_state->get_width_height();

                        js::value stash = js::get_heap_stash(inf.heap);

                        bool is_square = (int)stash["square_font"] > 0;

                        nlohmann::json j;
                        j["id"] = current_id;
                        j["width"] = width;
                        j["height"] = height;
                        j["script_name"] = (std::string)stash["realtime_script_name"];
                        j["type"] = "command_realtime";
                        j["square_font"] = is_square;

                        all_shared.value()->shared.add_back_write(j.dump());

                        last_use_square_font = is_square;
                    }

                    sand_data->is_realtime = true;
                    sand_data->is_static = false;

                    uint64_t server_packet_sequence_id = 0;

                    auto update_check = [&](js::value_context& vctx, uint64_t current_ack_state)
                    {
                        while(shared_duk_state->has_output_data_available())
                        {
                            std::string str = shared_duk_state->consume_output_data();

                            if(str != "")
                            {
                                nlohmann::json j;
                                j["id"] = current_id;
                                j["msg"] = str;
                                j["type"] = "command_realtime";

                                all_shared.value()->shared.add_back_write(j.dump());
                            }
                        }

                        {
                            std::optional<js_ui::ui_stack> stk_opt = js_ui::consume(inf.heap);

                            if(stk_opt.has_value())
                            {
                                js_ui::ui_stack& stk = stk_opt.value();

                                nlohmann::json j;
                                j["id"] = current_id;
                                j["type"] = "command_realtime_ui";

                                std::set<std::string> typeidx;
                                std::vector<int> argument_counts;

                                //steady_timer process_time;

                                j["arguments"] =  nlohmann::json::array();
                                j["types"] = nlohmann::json::array();

                                nlohmann::json& arg = j["arguments"];
                                nlohmann::json& types = j["types"];

                                for(js_ui::ui_element& e : stk.elements)
                                {
                                    typeidx.insert(e.type);
                                }

                                argument_counts.resize(typeidx.size());

                                for(js_ui::ui_element& e : stk.elements)
                                {
                                    int idx = std::distance(typeidx.begin(), typeidx.find(e.type));

                                    types.push_back(idx);
                                    argument_counts[idx] = e.arguments.size();

                                    for(auto& found_arg : e.arguments)
                                    {
                                        arg.push_back(std::move(found_arg));
                                    }
                                }

                                j["typeidx"] = typeidx;
                                j["typeargc"] = argument_counts;
                                j["server_seq_id"] = server_packet_sequence_id;

                                //ui_stack* stk = js::get_heap_stash(vctx)["ui_stack"].get_ptr<ui_stack>();

                                j["client_seq_ack"] = current_ack_state;

                                /*std::cout << "TIDX SIZE " << typeidx.size() << " ARGSIZE " << types.size() << std::endl;
                                printf("Naive size %i\n", j.dump().size());
                                printf("msgpack size %i\n", nlohmann::json::to_msgpack(j).size());*/

                                all_shared.value()->shared.add_back_write(j.dump());
                                //float ftime = process_time.get_elapsed_time_s() * 1000;
                                //printf("Elapsed %f\n", ftime);
                            }
                        }

                        server_packet_sequence_id++;

                        if(all_shared.value()->state.should_terminate_any_realtime)
                            return true;

                        {
                            safe_lock_guard guard(all_shared.value()->state.script_data_lock);

                            if(all_shared.value()->state.script_data[current_id].should_terminate_realtime)
                                return true;
                        }

                        if(!shared_duk_state->is_realtime())
                            return true;

                        if(all_shared.value()->live_work_units() > 10)
                            return true;

                        shared_duk_state->set_key_state(all_shared.value()->state.get_key_state(current_id));
                        shared_duk_state->set_mouse_pos(all_shared.value()->state.get_mouse_pos(current_id));

                        bool is_square = (int)js::get_heap_stash(vctx).get("square_font") > 0;

                        if(is_square != last_use_square_font)
                        {
                            nlohmann::json j;
                            j["id"] = current_id;
                            j["type"] = "command_realtime";
                            j["square_font"] = is_square;

                            all_shared.value()->shared.add_back_write(j.dump());

                            last_use_square_font = is_square;
                        }

                        return false;
                    };

                    sand_data->realtime_script_id = current_id;

                    ///remember, need to also set work units and do other things!
                    async_realtime_script_handler(inf.heap, inf.returned_val, cstate, inf.ret, current_id, update_check, all_shared.value());

                    if(shared_duk_state->close_window_on_exit())
                    {
                        nlohmann::json j;
                        j["id"] = current_id;
                        j["close"] = true;
                        j["type"] = "command_realtime";

                        all_shared.value()->shared.add_back_write(j.dump());
                    }
                }

                printf("Ended realtime\n");
            }
        }

        js::get_heap_stash(inf.heap)["all_shared_data"].free_in_heap<std::shared_ptr<shared_command_handler_state>>();
        teardown_state(inf.heap);

        //printf("cleaned up resources\n");

        if(launched_realtime)
        {
            all_shared.value()->state.remove_realtime_script(launched_realtime_id);;
        }

        return inf.ret;
    }
    catch(...)
    {
        return "Caught exception in script execution";
    }
}

void throwaway_user_thread(const std::string& username, const std::string& command, std::optional<float> custom_exec_time_s, bool force_exec)
{
    #ifndef USE_FIBERS

    get_global_fiber_queue().add(run_in_user_context, username, command, std::nullopt, custom_exec_time_s, force_exec);

    #else

    get_global_fiber_queue().add([=]()
    {
        run_in_user_context(username, command, std::nullopt, custom_exec_time_s, force_exec);
    });

    #endif
}

std::string binary_to_hex(const std::string& in, bool swap_endianness)
{
    std::string ret;

    const char* LUT = "0123456789ABCDEF";

    for(auto& i : in)
    {
        int lower_bits = ((int)i) & 0xF;
        int upper_bits = (((int)i) >> 4) & 0xF;

        if(swap_endianness)
        {
            std::swap(lower_bits, upper_bits);
        }

        ret += std::string(1, LUT[lower_bits]) + std::string(1, LUT[upper_bits]);
    }

    return ret;
}

int char_to_val(uint8_t c)
{
    if(c == '0')
        return 0;
    if(c == '1')
        return 1;
    if(c == '2')
        return 2;
    if(c == '3')
        return 3;
    if(c == '4')
        return 4;
    if(c == '5')
        return 5;
    if(c == '6')
        return 6;
    if(c == '7')
        return 7;
    if(c == '8')
        return 8;
    if(c == '9')
        return 9;
    if(c == 'A' || c == 'a')
        return 10;
    if(c == 'B' || c == 'b')
        return 11;
    if(c == 'C' || c == 'c')
        return 12;
    if(c == 'D' || c == 'd')
        return 13;
    if(c == 'E' || c == 'e')
        return 14;
    if(c == 'F' || c == 'f')
        return 15;

    return 0;
}

std::string hex_to_binary(const std::string& in, bool swap_endianness)
{
    std::string ret;

    int len = in.size();

    if((len % 2) != 0)
        return "Invalid Hex (non even size)";

    for(int i=0; i < len; i+=2)
    {
        int next = i + 1;

        if(next >= len)
            return "Invalid Hex (non even size)";

        char cchar = in[i];
        char nchar = next < len ? in[next] : '0';

        if(swap_endianness)
        {
            std::swap(cchar, nchar);
        }

        int lower = char_to_val(cchar) + (char_to_val(nchar) << 4);

        ret.push_back(lower);
    }

    return ret;
}

void on_create_user(const std::string& usr)
{
    run_in_user_context(usr, "#msg.manage({join:\"local\"})", std::nullopt);
    run_in_user_context(usr, "#msg.manage({join:\"global\"})", std::nullopt);
    run_in_user_context(usr, "#msg.manage({join:\"help\"})", std::nullopt);
    run_in_user_context(usr, "#msg.manage({join:\"memes\"})", std::nullopt);

    /*{
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
        usr.load_from_db(ctx, name);
    }*/

    user_first_time_network_setup(get_global_playspace_network_manager(), usr);

    tutorial_first_time_dialogue(usr);

    /*{
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
        usr.load_from_db(ctx, usr.name);
    }*/
}

std::string get_update_message()
{
    return "Welcome to net_code_! Pm me (20k) on discord if you have any issues, otherwise good luck :)";
}

void delete_notifs_for(const std::string& name)
{
    chats::delete_notifs_for(name);
}

void leave_channels_for(const std::string& name)
{
    chats::leave_channels_for(name);
}

void delete_user_db_for(const std::string& name)
{
    ///delete user db
    {
        disk_lock_proxy user_db = get_global_disk_user_accessible_context();
        user_db.change_collection(name);

        user_db->remove_json_many_new(nlohmann::json());
    }
}

void delete_nodes_for(const std::string& name)
{
    ///delete nodes
    {
        mongo_lock_proxy ctx = get_global_mongo_node_properties_context(-2);

        user_nodes nodes;
        nodes.owned_by = name;

        db_disk_remove(ctx, nodes);
    }
}

void delete_user_for(const std::string& name)
{
    ///DELETE USER
    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
        ctx.change_collection(name);

        user usr;
        usr.name = name;

        db_disk_remove(ctx, usr);
    }
}

void delete_npc_db_for(const std::string& name)
{
    mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

    npc_prop_list npc_props;
    npc_props.name = name;

    db_disk_remove(ctx, npc_props);
}

void delete_links_for(const std::string& name)
{
    {
        playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

        playspace_network_manage.unlink_all(name);
    }
}

void delete_structure_for(const std::string& name)
{
    {
        low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

        for(low_level_structure& i : low_level_structure_manage.systems)
        {
            for(auto it = i.user_list.begin(); it != i.user_list.end();)
            {
                if(*it == name)
                {
                    it = i.user_list.erase(it);

                    mongo_lock_proxy ctx = get_global_mongo_low_level_structure_context(-2);

                    db_disk_remove(ctx, i);
                }
                else
                {
                    it++;
                }
            }
        }
    }
}

void delete_quests_for(const std::string& name)
{
    quest_manager& quest_manage = get_global_quest_manager();

    mongo_lock_proxy ctx = get_global_mongo_quest_manager_context(-2);

    auto all = quest_manage.fetch_quests_of(ctx, name);

    for(auto& i : all)
    {
        db_disk_remove(ctx, i);
    }
}

void delete_events_for(const std::string& name)
{
    mongo_nolock_proxy ctx = get_global_mongo_event_manager_context(-2);
    ctx.change_collection(name);

    std::vector<event_impl> all_events = db_disk_load_all(ctx, event_impl());

    for(event_impl& e : all_events)
    {
        if(e.user_name == name)
        {
            db_disk_remove(ctx, e);
        }
    }
}

///ok
///things this function needs to do
///preserve items
///preserve nodes

///things this function does not need to do
///fix up notifs (?)
///fix up user db
///fixup auth

///NOT UPDATED FOR SYSTEM STUFF

///not updated for quest stuff
///not updated for event stuff
#if 0
std::string rename_user_force(const std::string& from_name, const std::string& to_name)
{
    user usr;

    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

        if(user().exists(ctx, to_name))
            return "User already exists";

        if(!usr.load_from_db(ctx, from_name))
            return "No such user";

        if(!usr.is_npc())
            return "Currently only allowed for npcs";
    }

    ///so
    {
        npc_user tusr;

        {
            mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

            ///we construct the new user
            tusr.construct_in_user_database(ctx, to_name);

            ///rename the old user
            usr.name = to_name;
            ///then rewrite the user over the constructed npc user
            usr.overwrite_user_in_db(ctx);
        }

        npc_prop_list props;

        ///oh jesus this is so much easier with the new db system
        ///overwrite does upsert
        {
            mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

            ///load old props
            //props.load_from_db(ctx, from_name);

            db_disk_load(ctx, props, from_name);
            ///delete those bad boys
            db_disk_remove(ctx, props);

            ///rename props
            props.name = to_name;
            ///insert into db under new name
            db_disk_overwrite(ctx, props);
        }
    }

    ///transfer items
    {
        mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(-2);

        mongo_requester req;
        req.set_prop("owner", from_name);

        auto found = req.fetch_from_db(items_ctx);

        for(auto& i : found)
        {
            i.set_prop("owner", to_name);

            i.insert_in_db(items_ctx);
        }
    }

    ///IGNORE AUTH

    ///NODES
    user_nodes nodes = get_nodes(from_name, -2);

    {
        nodes.owned_by = to_name;

        std::map<std::string, std::string> rename_map;

        for(user_node& node : nodes.nodes)
        {
            node.owned_by = to_name;

            std::string old_name = node.unique_id;

            std::string ext = node.unique_id;

            for(int i=0; i < (int)node.owned_by.size() + 1; i++)
            {
                ext.erase(ext.begin());
            }

            node.unique_id = to_name + "_" + ext;

            rename_map[old_name] = node.unique_id;
        }

        for(auto& i : nodes.nodes)
        {
            for(auto& j : i.connected_to)
            {
                j  = rename_map[j];
            }
        }
    }

    {
        mongo_lock_proxy nodes_ctx = get_global_mongo_node_properties_context(-2);
        nodes_ctx.change_collection(to_name);

        user_nodes tmp;
        tmp.ensure_exists(nodes_ctx, to_name);

        nodes.overwrite_in_db(nodes_ctx);
    }

    ///ok
    ///need to relink
    {
        get_global_playspace_network_manager().rename(from_name, to_name);
    }

    delete_nodes_for(from_name);
    delete_user_db_for(from_name);
    delete_user_for(from_name);
    delete_notifs_for(from_name);

    return "Renamed " + from_name + " " + to_name;
}
#endif // 0

///should really queue this or something
std::string delete_user(command_handler_state& state, const std::string& str, bool cli_force)
{
    std::string auth_token;

    std::string name;

    if(!cli_force)
    {

        {
            auth_token = state.get_auth();
        }

        if(auth_token == "")
            return "No auth";

        std::string command_str = "#delete_user ";

        if(str.size() == command_str.size())
            return "Invalid username";

        auto splits = no_ss_split(str, " ");

        if(splits.size() > 2)
            return "Invalid command or username";

        name = splits[1];

        ///override default name checking so we can delete users
        ///with uppercase names
        if(!is_valid_string(name, true))
            return "Invalid name";

        ///Things to clean up
        ///user itself - done
        ///notifs - done
        ///items - done
        ///auth - done
        ///user db - done
        ///nodes - done

        {
            mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

            user to_delete;
            to_delete.load_from_db(ctx, name);

            if(to_delete.get_auth_token_binary() != auth_token)
                return "Invalid Auth";

            if(SHOULD_RATELIMIT(auth_token, DELETE_USER))
                return "You may only delete 1 user per hour";
        }
    }
    else
    {
        name = str;
    }

    ///DELETE ITEMS
    ///TODO: RACEY
    {
        user usr;

        {
            mongo_lock_proxy user_ctx = get_global_mongo_user_info_context(-2);

            usr.load_from_db(user_ctx, name);
        }

        for(auto& i : usr.get_all_items())
        {
            mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(-2);

            item it;
            it.item_id = i;

            db_disk_remove(items_ctx, it);
        }
    }

    ///DELETE AUTH
    {
        mongo_lock_proxy auth_db = get_global_mongo_global_properties_context(-2);

        auth found_auth;

        if(found_auth.load_from_db(auth_db, auth_token))
        {
            for(int i=0; i < (int)found_auth.users.size(); i++)
            {
                if(found_auth.users[i] == name)
                {
                    found_auth.users.erase(found_auth.users.begin() + i);
                    i--;
                    continue;
                }
            }

            found_auth.overwrite_in_db(auth_db);
        }
        else
        {
            if(!cli_force)
            {
                return "Auth Error: Purple Catepillar";
            }
        }
    }

    {
        user funtimes;

        {
            mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

            funtimes.load_from_db(ctx, name);
        }

        if(funtimes.is_npc())
        {
            delete_npc_db_for(name);
        }
    }

    delete_notifs_for(name);

    leave_channels_for(name);

    delete_user_db_for(name);

    delete_nodes_for(name);

    delete_user_for(name);

    delete_links_for(name);

    delete_structure_for(name);

    delete_quests_for(name);

    delete_events_for(name);

    return "Deleted";
}

bool is_allowed_user(const std::string& user)
{
    std::set<std::string> banned;

    for(auto& i : privileged_args)
    {
        std::string script_name = i.first;

        std::string str = no_ss_split(script_name, ".")[0];

        banned.insert(str);
    }

    banned.insert("db");
    banned.insert("core");
    banned.insert("extern");
    banned.insert("market");
    banned.insert("news");
    banned.insert("world_service");
    banned.insert("stocks");
    banned.insert("stock");

    return banned.find(user) == banned.end();
}

/*struct handle_command_return
{
    enum class return_type
    {
        DEFAULT,
        RAW,
        ERR,
    };

    return_type type = return_type::ERR;
    std::string val;

    handle_command_return(const std::string& pval)
    {
        type = return_type::DEFAULT;
        val = pval;
    }

    handle_command_return(const char* ptr) : handle_command_return(std::string(ptr))
    {

    }

    handle_command_return(const std::string& pval, const std::string& tag)
    {
        type = return_type::RAW;
        val = tag + " " + pval;
    }
};*/

nlohmann::json make_response(const std::string& str)
{
    nlohmann::json data;
    data["type"] = "server_msg";
    data["data"] = str;

    return data;
}

nlohmann::json handle_command_impl(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str)
{
    //printf("yay command\n");

    //lg::log(str);

    if(strip_whitespace(tolower_str(str)) == "help" || strip_whitespace(tolower_str(str)) == "#help")
    {
        return make_response("Lost? Run #ada.access() to get started");
    }
    else if(starts_with(str, "user "))
    {
        if(all_shared->state.get_auth() == "")
            return make_response(make_error_col("Please create account with \"register client\""));

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() != 2)
            return make_response(make_error_col("Invalid Command Error"));

        std::string user_name = strip_whitespace(split_string[1]);

        if(!is_valid_string(user_name))
            return make_response(make_error_col("Invalid username"));

        if(!is_allowed_user(user_name))
            return make_response(make_error_col("Claiming or using this specific username is disallowed. If you already own it you may #delete_user the user in question"));

        {
            std::optional<std::string> found_username;

            {
                db::read_tx rtx;

                auth my_auth;

                if(my_auth.load_from_db(rtx, all_shared->state.get_auth()) && my_auth.contains_user(user_name))
                {
                    found_username = user_name;
                }

                user fnd;

                if(fnd.exists(rtx, user_name))
                {
                    fnd.load_from_db(rtx, user_name);

                    if(fnd.get_auth_token_hex() != all_shared->state.get_auth_hex())
                    {
                        return make_response(make_error_col("Incorrect Auth, someone else has registered this account or you are using a different pc and key.key file"));
                    }

                    found_username = fnd.name;
                }
            }

            //auto allowed = fnd.get_call_stack();

            if(found_username.has_value())
            {
                ///What? Why is this a locking db operation?
                /*{
                    mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(-2);

                    auth to_check;

                    if(!to_check.load_from_db(mongo_ctx, all_shared->state.get_auth()))
                        return make_response(make_error_col("Trying something sneaky eh?"));

                    to_check.insert_user_exclusive(user_name);
                    to_check.overwrite_in_db(mongo_ctx);
                }*/

                all_shared->state.set_user_name(found_username.value());
                return make_response("Switched to user");
            }

            /*for(auto& i : allowed)
            {
                std::cout <<" a  " << i << std::endl;
            }*/
        }

        ///user does not exist
        auth my_auth;

        {
            all_shared->state.set_user_name("");

            {
                db::read_write_tx rtx;

                {
                    auto fauth = all_shared->state.get_auth();

                    if(!my_auth.load_from_db(rtx, fauth))
                        return make_response(make_error_col("Trying something sneaky eh 2?"));

                    #ifdef TESTING
                    #define MAX_USERS 999
                    #else // TESTING
                    #define MAX_USERS 5
                    #endif

                    if(my_auth.users.size() >= MAX_USERS)
                        return make_response(make_error_col("Max users " + std::to_string(my_auth.users.size()) + "/" + std::to_string(MAX_USERS)));

                    std::vector<auth> all = db_disk_load_all(rtx, auth());

                    for(const auth& a : all)
                    {
                        for(const std::string& username : a.users)
                        {
                            if(username == user_name && a.auth_token_hex != all_shared->state.get_auth_hex())
                                return make_response(make_error_col("Another account has already claimed this username"));
                        }
                    }

                    my_auth.insert_user_exclusive(user_name);
                    my_auth.overwrite_in_db(rtx);
                }

                if(!my_auth.is_free_account)
                {
                    user new_user;

                    auto fauth = all_shared->state.get_auth();

                    {
                        new_user.construct_new_user(rtx, user_name, fauth);
                        new_user.load_from_db(rtx, user_name);
                        new_user.overwrite_user_in_db(rtx);
                    }
                }

                all_shared->state.set_user_name(user_name);
            }

            std::string cur_name = all_shared->state.get_user_name();

            if(!my_auth.is_free_account)
                on_create_user(cur_name);

            all_shared->state.set_user_name(cur_name);

            fiber_sleep(1000);

            return make_response(make_success_col("Constructed new User"));
        }
    }
    else if(starts_with(str, "#delete_user "))
    {
        return make_response(delete_user(all_shared->state, str));
    }
    else if(starts_with(str, "#down"))
    {
        if(all_shared->state.get_auth() == "")
            return make_response(make_error_col("No Auth"));

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() != 2)
        {
            return make_response("Syntax is #down scriptname");
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = all_shared->state.get_user_name() + "." + scriptname;

        std::string unparsed_source;

        {
            mongo_read_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_info inf;
            inf.name = fullname;
            inf.load_from_db(mongo_ctx);

            if(!inf.valid)
                return make_response(make_error_col("Could not find script " + fullname));

            unparsed_source = inf.unparsed_source;
        }

        nlohmann::json data;
        data["type"] = "script_down";
        data["name"] = fullname;
        data["data"] = unparsed_source;

        return data;
    }
    else if(starts_with(str, "#delete_steam_and_tie_to_auth"))
    {
        if(all_shared->state.get_auth() == "")
            return make_response(make_error_col("No Auth"));

        std::string auth_binary = all_shared->state.get_auth();

        if(auth_binary.size() != 128)
            return make_response(make_error_col("User auth must be of length 256 in hex or 128 in binary"));

        uint64_t steam_id = all_shared->state.get_steam_id();

        if(steam_id == 0)
            return make_response(make_error_col("No steam auth"));

        {
            mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);

            auth old_auth;

            if(!old_auth.load_from_db(ctx, auth_binary))
                return make_response(make_error_col("key.key auth is not valid, this should be impossible if you're logged in"));

            auth steam_user_auth;

            if(!steam_user_auth.load_from_db_steamid(ctx, steam_id))
                return make_response(make_error_col("Steam Auth Failed?"));

            if(old_auth.steam_id == steam_id)
                return make_response("Auth already tied to steam id");

            ///this is massively safer
            ///it will result in orphaned auths but this doesn't matter
            steam_user_auth.steam_id = 0;
            steam_user_auth.overwrite_in_db(ctx);

            ///if request being removed from the db deletes old auth that's kind of dangerous
            ///as auth.overwrite_in_db doesn't have upsert behaviour
            old_auth.steam_id = steam_id;
            old_auth.overwrite_in_db(ctx);
        }

        return make_response(make_success_col("Success"));
    }
    else if(starts_with(str, "#dl_auth"))
    {
        nlohmann::json data;
        data["type"] = "auth";
        data["data"] = binary_to_hex(all_shared->state.get_auth());

        return data;
    }
    else if(starts_with(str, "#up ") || starts_with(str, "#dry ") || starts_with(str, "#up_es6 "))
    {
        if(all_shared->state.get_auth() == "")
            return make_response(make_error_col("No Auth"));

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 3)
        {
            if(starts_with(str, "#up "))
                return make_response("Syntax is #up scriptname or invalid scriptname");
            if(starts_with(str, "#dry "))
                return make_response("Syntax is #dry scriptname or invalid scriptname");
            if(starts_with(str, "#up_es6 ")) ///this is not client facing
                return make_response("Syntax is #up scriptname or invalid scriptname");
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = all_shared->state.get_user_name() + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
        {
            return make_response(make_error_col("Invalid script name " + fullname));
        }

        auto begin_it = str.begin();

        int num_spaces = 0;

        while(num_spaces != 2 && begin_it != str.end())
        {
            if(*begin_it == ' ')
            {
                num_spaces++;
            }

            begin_it++;
        }

        bool is_es6 = starts_with(str, "#up_es6 ");

        if(begin_it != str.end())
        {
            std::string data_source(begin_it, str.end());

            bool was_public = false;

            {
                script_info script_inf;
                script_inf.name = fullname;

                mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(-2);
                script_inf.load_from_db(item_ctx);

                if(script_inf.valid && script_inf.in_public)
                    was_public = true;
            }

            script_info script_inf;
            std::string compile_error;

            {
                js::value_context vctx(nullptr, nullptr);

                compile_error = script_inf.load_from_unparsed_source(vctx, data_source, fullname, is_es6, false);
            }

            if(compile_error != "")
                return make_response(compile_error);

            user cur;

            {
                auto uname = all_shared->state.get_user_name();

                mongo_lock_proxy user_locks = get_global_mongo_user_info_context(-2);

                if(!cur.load_from_db(user_locks, uname))
                    return make_response(make_error_col("Bad User"));
            }

            std::map<std::string, double> user_details;

            user_details = cur.get_total_user_properties();

            int num_chars = script_inf.unparsed_source.size();
            int max_chars = user_details["char_count"];

            if(!starts_with(str, "#dry "))
            {
                script_inf.in_public = was_public;

                mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                //std::cout << "overwriting\n";

                script_inf.overwrite_in_db(mongo_ctx);
            }

            std::string rstr = "Upload Successful ";

            if(starts_with(str, "#dry "))
                rstr = "Dry Upload Successful ";

            if(scriptname == "invoke")
            {
                rstr += "[Set as command line wrapper] ";
            }

            return make_response(make_success_col(rstr + std::to_string(num_chars) + "/" + std::to_string(max_chars)));
        }
    }
    else if(starts_with(str, "#remove "))
    {
        if(all_shared->state.get_auth() == "")
            return make_response(make_error_col("No Auth"));

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 2)
        {
            return make_response("Syntax is #remove scriptname");
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = all_shared->state.get_user_name() + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
            return make_response(make_error_col("Invalid script name " + fullname));

        {
            auto uname = all_shared->state.get_user_name();

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_info script_inf;
            script_inf.name = uname + "." + scriptname;

            if(!script_inf.exists_in_db(mongo_ctx))
                return make_response(make_error_col("Script not found"));

            item titem;
            titem.item_id = script_inf.name;

            db_disk_remove(mongo_ctx, titem);
        }

        std::string str = "Script removed from server";

        if(scriptname == "invoke")
        {
            str += " [Removed as command line wrapper]";
        }

        return make_response(make_success_col(str));
    }
    else if(starts_with(str, "#public ") || starts_with(str, "#private "))
    {
        if(all_shared->state.get_auth() == "")
            return make_response(make_error_col("No Auth"));

        int in_public_state = starts_with(str, "#public ");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 2)
        {
            return make_response("Syntax is #public scriptname or #private scriptname");
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = all_shared->state.get_user_name() + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
            return make_response(make_error_col("Invalid script name " + fullname));

        {
            auto uname = all_shared->state.get_user_name();

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_info script_inf;
            script_inf.name = uname + "." + scriptname;

            if(!script_inf.load_from_db(mongo_ctx))
                return make_response(make_error_col("Script not found"));

            script_inf.in_public = in_public_state;

            script_inf.overwrite_in_db(mongo_ctx);

            //std::cout << "overwriting public " << script_inf.name << " public? " << script_inf.in_public << std::endl;
        }

        return make_response(make_success_col("Success"));
    }
    //#define ALLOW_SELF_AUTH
    #ifdef ALLOW_SELF_AUTH
    else if(starts_with(str, "register client"))
    {
        std::string to_ret = random_binary_string(128);

        mongo_requester request;
        //request.set_prop_bin("account_token", to_ret);
        request.set_prop("auth_token_hex", binary_to_hex(to_ret));

        all_shared->state.set_auth(to_ret);

        mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);
        request.insert_in_db(ctx);

        is_auth = true;

        if(starts_with(str, "register client_hex"))
        {
            return "secret_hex " + binary_to_hex(to_ret);
        }

        return "secret " + to_ret;
    }
    #endif // ALLOW_SELF_AUTH
    else if(starts_with(str, "#make_free_auth"))
    {
        fiber_sleep(1000);

        printf("Created non-Steam Account\n");

        std::string to_ret = random_binary_string(128);

        auth to_insert;
        to_insert.auth_token_hex = binary_to_hex(to_ret);
        to_insert.is_free_account = true;

        all_shared->state.set_auth(to_ret);

        {
            db::read_write_tx rwtx;
            to_insert.overwrite_in_db(rwtx);
        }

        std::string full_string = "Account Created. Type user <username> to create a new user";

        nlohmann::json data;
        data["type"] = "server_msg";
        data["data"] = make_success_col("Auth Success") + "\n" + full_string + "\n" + get_update_message();
        data["authenticated"] = 1;

        return data;
    }
    else
    {
        auto name = all_shared->state.get_user_name();

        if(name == "")
            return make_response("No account or not logged in");

        return make_response(run_in_user_context(name, str, all_shared));
    }

    return make_response(make_error_col("Command Not Found or Unimplemented"));
}

void strip_old_msg_or_notif(mongo_lock_proxy& ctx)
{
    #if 0
    nlohmann::json exists;
    exists["$exists"] = true;

    nlohmann::json to_fetch;
    to_fetch["processed"] = exists;

    auto all = fetch_from_db(ctx, to_fetch);

    for(auto& req : all)
    {
        size_t found_time = 0;

        if(req.find("time_ms") != req.end())
        {
            if(req["time_ms"].is_number())
                found_time = (size_t)req["time_ms"];
            else if(req["time_ms"].is_string())
            {
                std::string str = req["time_ms"];
                found_time = std::stoll((std::string)str);
            }
        }

        size_t thirty_days = 1000ull * 60ull * 60ull * 24ull * 30ull;

        if(get_wall_time() >= found_time + thirty_days)
        {
            //printf("removing from db\n");

            req["processed"] = 1;
            remove_all_from_db(ctx, req);
        }
    }
    #endif // 0

    /*for_each_user([](user& usr)
    {
        chats::strip_old_for(usr.name);
    });

    for_each_npc([](user& usr)
    {
        chats::strip_old_for(usr.name);
    });*/

    chats::strip_all_old();
}

///todo: return nothing if nothing has changed
nlohmann::json handle_client_poll_json(user& usr)
{
    usr.cleanup_call_stack(-2);
    std::string name = usr.get_call_stack().back();

    std::vector<std::string> channels = chats::get_channels_for_user(name);

    std::vector<std::pair<std::string, chat_message>> msgs = chats::get_and_update_chat_msgs_for_user(name);
    std::vector<chat_message> tells = chats::get_and_update_tells_for_user(name);
    std::vector<chat_message> notifs = chats::get_and_update_notifs_for_user(name);

    nlohmann::json all;

    all["channels"] = channels;

    std::vector<nlohmann::json> cdata;
    std::vector<nlohmann::json> tdata;
    std::vector<std::string> ndata;

    for(auto& [channel_name, msg] : msgs)
    {
        nlohmann::json api;
        api["channel"] = channel_name;
        api["text"] = chats::prettify({msg}, true, channel_name);

        cdata.push_back(api);
    }

    for(auto& msg : tells)
    {
        nlohmann::json api;

        api["user"] = msg.originator;
        api["text"] = chats::prettify({msg}, false, "");

        tdata.push_back(api);
    }

    for(auto& msg : notifs)
    {
        ndata.push_back(msg.msg);
    }

    all["data"] = cdata;
    all["tells"] = tdata;
    all["notifs"] = ndata;
    all["user"] = usr.get_call_stack().back();
    all["root_user"] = usr.name;

    all["type"] = "chat_api";

    return all;
}

///needs to handle script bundles
///use unified script loading
std::optional<std::vector<script_arg>> get_uniform_script_args(const std::string& script)
{
    if(privileged_args.find(script) != privileged_args.end())
    {
        return privileged_args[script];
    }

    std::string err;

    unified_script_info script_inf = unified_script_loading(-2, script, err);

    if(!script_inf.valid)
        return std::nullopt;

    if(script_inf.args.size() != script_inf.params.size())
        return std::nullopt;

    std::vector<script_arg> args;

    for(int i=0; i < (int)script_inf.args.size(); i++)
    {
        args.push_back({script_inf.args[i], script_inf.params[i]});
    }

    return args;
}

nlohmann::json handle_autocompletes_json(const std::string& username, const std::string& in)
{
    std::string script = in;

    using json = nlohmann::json;

    json obj;
    obj["script"] = script;

    if(!is_valid_full_name_string(script))
    {
        obj["type"] = "script_args_invalid";

        return obj;
    }

    if(SHOULD_RATELIMIT(username, AUTOCOMPLETES))
    {
        obj["type"] = "script_args_ratelimit";

        return obj;
    }

    auto opt_arg = get_uniform_script_args(script);

    if(!opt_arg.has_value())
    {
        obj["type"] = "script_args_invalid";

        return obj;
    }

    auto args = *opt_arg;

    std::vector<std::string> keys;
    std::vector<std::string> vals;

    for(script_arg& arg : args)
    {
        keys.push_back(arg.key);
        vals.push_back(arg.val);
    }

    obj["keys"] = keys;
    obj["vals"] = vals;

    obj["type"] = "script_args";

    return obj;
}

std::vector<std::string> sanitise_input_vec(std::vector<std::string> vec)
{
    if(vec.size() > 10)
        vec.resize(10);

    for(auto& i : vec)
    {
        if(i.size() > 10)
            i.resize(10);
    }

    return vec;
}

nlohmann::json handle_command(std::shared_ptr<shared_command_handler_state> all_shared, nlohmann::json str)
{
    std::string current_user = all_shared->state.get_user_name();
    std::string current_auth = all_shared->state.get_auth();

    if(current_user != "")
    {
        db::read_tx rtx;

        auth my_auth;

        if(!my_auth.load_from_db(rtx, current_auth))
        {
            nlohmann::json data;
            data["type"] = "server_msg";
            data["data"] = "User not logged in while having a set user, fatal error";

            return data;
        }

        if(!my_auth.contains_user(current_user))
        {
            all_shared->state.set_user_name("");

            nlohmann::json data;
            data["type"] = "server_msg";
            data["data"] = "Logged into a user for which you don't have auth, fatal error";

            return data;
        }
    }

    {
        safe_lock_guard guard(all_shared->state.script_data_lock);

        while(all_shared->state.script_data.size() > 50)
        {
            all_shared->state.script_data.erase(all_shared->state.script_data.begin());
        }
    }

    std::string type = str["type"];

    if(type == "generic_server_command")
    {
        std::string to_exec;
        nlohmann::json tag;
        bool tagged = str.count("tag") > 0;

        if(tagged)
        {
            tag = str["tag"];
        }

        to_exec = str["data"];

        nlohmann::json data = handle_command_impl(all_shared, to_exec);

        if(tagged)
        {
            data["tag"] = tag;
        }

        return data;
    }

    if(type == "client_chat")
    {
        bool respond = str.count("respond") > 0 && ((int)str["respond"]) > 0;

        std::string to_exec = str["data"];

        nlohmann::json data = handle_command_impl(all_shared, to_exec);

        if(respond)
        {
            nlohmann::json ndata;
            ndata["type"] = "chat_api_response";
            ndata["data"] = data["data"];

            return ndata;
        }

        return nlohmann::json();
    }

    ///matches both client poll and json
    ///this path specifically may be called in parallel with the other parts
    ///hence the current user guard
    ///latter check is just for backwards compat
    if(type == "client_poll" || type == "client_poll_json")
    {
        if(current_auth == "" || current_user == "")
            return nlohmann::json();

        if(SHOULD_RATELIMIT(current_auth, POLL))
            return nlohmann::json();

        user usr;

        {
            db::read_tx rtx;

            if(!usr.load_from_db(rtx, current_user))
                return nlohmann::json();
        }

        return handle_client_poll_json(usr);
    }

    if(type == "autocomplete_request")
    {
        if(current_auth == "" || current_user == "")
            return nlohmann::json();

        return handle_autocompletes_json(current_user, str["data"]);
    }

    if(type == "key_auth")
    {
        if(str.count("data") == 0)
        {
            nlohmann::json data;
            data["type"] = "server_msg";
            data["data"] = make_error_col("No .data property in json key_auth");

            return data;
        }

        printf("auth client\n");
        std::string auth_token = hex_to_binary(str["data"]);

        if(auth_token.length() > 140)
        {
            nlohmann::json data;
            data["type"] = "server_msg";
            data["data"] = make_error_col("Auth too long");

            return data;
        }

        if(auth_token.size() == 0)
        {
            nlohmann::json data;
            data["type"] = "server_msg";
            data["data"] = make_error_col("No auth token found");

            return data;
        }

        std::vector<std::string> users;

        {
            enforce_constant_time ect;

            db::read_tx rtx;

            auth user_auth;

            if(!user_auth.load_from_db(rtx, auth_token))
            {
                nlohmann::json data;
                data["type"] = "server_msg";
                data["data"] = make_error_col("Auth Failed");

                return data;
            }

            users = user_auth.users;
        }

        all_shared->state.set_auth(auth_token);

        std::string auth_string;

        for(auto& i : users)
        {
            auth_string += " " + colour_string(i);
        }

        std::string full_string = "Users Found:";

        if(auth_string == "")
            full_string = "No Users Found. Type user <username> to register";

        //std::cout << auth_string << std::endl;

        nlohmann::json data;
        data["type"] = "server_msg";
        data["data"] = make_success_col("Auth Success") + "\n" + full_string + auth_string + "\n" + get_update_message();
        data["authenticated"] = 1;

        return data;
    }

    if(type == "steam_auth")
    {
        printf("AUTH STEAM\n");

        std::string steam_encrypted_auth_token = str["data"];

        ///don't actually do anything with this yet
        std::optional<steam_auth_data> opt_steam_id = get_steam_auth(steam_encrypted_auth_token);

        if(!opt_steam_id.has_value())
        {
            nlohmann::json data;
            data["type"] = "server_msg";
            data["data"] = make_error_col("Error using steam auth, check your client's debug log");

            return data;
        }

        steam_auth_data steam_auth = opt_steam_id.value();

        uint64_t steam_id = steam_auth.steam_id;

        all_shared->state.set_steam_id(steam_id);

        auth fauth;

        std::vector<std::string> users;

        bool is_steam_auth = false;

        {
            enforce_constant_time ect;

            mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);

            bool should_create_account = !auth().load_from_db_steamid(ctx, steam_id);

            if(steam_auth.user_data.size() == 128)
            {
                printf("Steam auth using key token\n");

                if(!fauth.load_from_db(ctx, steam_auth.user_data))
                {
                    nlohmann::json data;
                    data["type"] = "server_msg";
                    data["data"] = make_error_col("Bad user auth in encrypted token, eg your key.key file is corrupt whilst simultaneously using steam auth");

                    return data;
                }

                is_steam_auth = false;
            }
            else
            {
                printf("Steam auth using only steam\n");

                fauth.load_from_db_steamid(ctx, steam_id);

                is_steam_auth = true;
            }

            if(should_create_account)
            {
                printf("Created Steam Account\n");

                std::string to_ret = random_binary_string(128);

                auth to_insert;
                to_insert.auth_token_hex = binary_to_hex(to_ret);
                to_insert.steam_id = all_shared->state.get_steam_id();

                all_shared->state.set_auth(to_ret);

                to_insert.overwrite_in_db(ctx);

                if(steam_auth.user_data.size() != 128)
                {
                    if(!fauth.load_from_db_steamid(ctx, steam_id))
                        throw std::runtime_error("Something catastrophically wrong in the server");
                }
            }

            users = fauth.users;
        }

        ///SO IMPORTANT
        ///THE AUTH TOKEN HERE MAY NOT CORRESPOND TO THE STEAM ACCOUNT *BY DESIGN*
        all_shared->state.set_auth(hex_to_binary(fauth.auth_token_hex));
        all_shared->state.set_steam_id(steam_id);

        std::string auth_string;

        for(auto& i : users)
        {
            auth_string += " " + colour_string(i);
        }

        std::string full_string = "Users Found:";

        if(auth_string == "")
            full_string = "No Users Found. Type user <username> to register";

        std::string auth_str = "";

        if(is_steam_auth)
        {
            auth_str = "Auth (Steam) Success";
        }
        else
        {
            auth_str = "Auth (non-Steam) Success";
        }

        std::cout << auth_string << std::endl;

        nlohmann::json data;
        data["type"] = "server_msg";
        data["data"] = make_success_col(auth_str) + "\n" + full_string + auth_string + "\n" + get_update_message();
        data["authenticated"] = 1;

        return data;
    }

    if(type == "client_terminate_scripts")
    {
        int id = str["id"];

        if(id <= -1)
        {
            all_shared->state.should_terminate_any_realtime = true;
            all_shared->terminate_timer.restart();
        }
        else
        {
            safe_lock_guard guard(all_shared->state.script_data_lock);

            all_shared->state.script_data[id].should_terminate_realtime = true;
        }

        return nlohmann::json();
    }

    if(type == "send_keystrokes_to_script")
    {
        int id = str["id"];

        ///todo
        ///keystroke special funtime
        ///create a push vector function
        ///need to also have an internal map of key state
        ///assume keys are not pressed when we get them the first time, dont do any magic

        if(str.find("input_keys") != str.end())
        {
            std::vector<std::string> keys = str["input_keys"];
            keys = sanitise_input_vec(keys);

            {
                for(auto& i : keys)
                {
                    unprocessed_key_info info;
                    info.key = i;
                    info.is_repeat = all_shared->state.get_key_state(id)[i];

                    safe_lock_guard guard(all_shared->state.script_data_lock);
                    all_shared->state.script_data[id].unprocessed_text_input.push_back(info);
                }

                safe_lock_guard guard(all_shared->state.script_data_lock);
                realtime_script_data& dat = all_shared->state.script_data[id];

                while(dat.unprocessed_text_input.size() > 200)
                {
                    dat.unprocessed_text_input.erase(dat.unprocessed_text_input.begin());
                }
            }
        }

        if(str.find("pressed_keys") != str.end())
        {
            std::vector<std::string> keys = str["pressed_keys"];
            keys = sanitise_input_vec(keys);

            for(auto& i : keys)
            {
                all_shared->state.set_key_state(id, i, true);
            }
        }

        if(str.find("released_keys") != str.end())
        {
            std::vector<std::string> keys = str["released_keys"];
            keys = sanitise_input_vec(keys);

            for(auto& i : keys)
            {
                all_shared->state.set_key_state(id, i, false);
            }
        }

        return nlohmann::json();
    }

    if(type == "client_ui_element")
    {
        int id = str["id"];
        std::string ui_id = str["ui_id"];
        std::vector<std::string> found_state = str["state"];

        if(found_state.size() > 5)
            found_state.clear();

        safe_lock_guard guard(all_shared->state.script_data_lock);

        auto found_it = all_shared->state.script_data.find(id);

        if(found_it != all_shared->state.script_data.end())
        {
            if(ui_id.size() > 50)
                ui_id.resize(50);

            if(found_state.size() > 50)
                found_state.resize(50);

            realtime_script_data& dat = found_it->second;

            ui_element_state& st = dat.realtime_ui.element_states[ui_id];
            //st.processed = true;

            std::set<std::string> old_events;

            for(auto& i : st.value)
            {
                if(js_ui::is_edge_event(i))
                {
                    old_events.insert(i);
                }
            }

            for(auto& i : found_state)
            {
                old_events.insert(i);
            }

            st.value.clear();
            st.value.reserve(old_events.size());

            for(auto it = old_events.begin(); it != old_events.end();)
            {
                 st.value.push_back(std::move(old_events.extract(it++).value()));
            }

            ///so arguments might overwrite the last one, but not the end of the world
            ///big issue is that if we have a lot of dynamic ui elements, might pile up unused junk
            if(str.count("arguments") > 0)
            {
                st.client_override_arguments = str["arguments"];
            }

            if(str.count("sequence_id") > 0)
            {
                dat.client_seq_id = str["sequence_id"];
            }

            ///unused junk will get cleaned up here, but basically randomly
            while(dat.realtime_ui.element_states.size() > 100)
            {
                dat.realtime_ui.element_states.erase(dat.realtime_ui.element_states.begin());
            }
        }

        return nlohmann::json();
    }

    if(type == "send_script_info")
    {
        int id = str["id"];

        if(id < 0)
            return nlohmann::json();

        int width = str["width"];
        int height = str["height"];

        all_shared->state.set_width_height(id, width, height);

        return nlohmann::json();
    }

    if(type == "update_mouse_to_script")
    {
        int id = str["id"];

        if(id < 0)
            return nlohmann::json();

        float mouse_x = str["mouse_x"];
        float mouse_y = str["mouse_y"];

        float mousewheel_x = str["mousewheel_x"];
        float mousewheel_y = str["mousewheel_y"];

        all_shared->state.add_mouse_state(id, {mouse_x, mouse_y}, {mousewheel_x, mousewheel_y});

        return nlohmann::json();
    }

    nlohmann::json data;
    data["type"] = "server_msg";
    data["data"] = "Command not understood";

    return data;
}

void async_handle_command(std::shared_ptr<shared_command_handler_state> all_shared, nlohmann::json data)
{
    #ifndef USE_FIBERS

    sthread([all_shared, data = std::move(data)]()
    {
        nlohmann::json result = handle_command(all_shared, std::move(data));

        all_shared->execution_requested = false;

        if(result.count("type") == 0)
            return;

        shared_data& shared = all_shared->shared;
        shared.add_back_write(result.dump());
    }).detach();

    #else

    get_global_fiber_queue().add([all_shared, data = std::move(data)]()
    {
        nlohmann::json result = handle_command(all_shared, std::move(data));

        all_shared->execution_requested = false;

        if(result.count("type") == 0)
            return;

        shared_data& shared = all_shared->shared;
        shared.add_back_write(result.dump());
    });

    #endif // USE_FIBERS
}
