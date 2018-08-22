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
#include <json/json.hpp>
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

struct unsafe_info
{
    user* usr;
    std::string command;
    duk_context* ctx;
    volatile int finished = 0;

    std::string ret;
};

inline
duk_ret_t unsafe_wrapper(duk_context* ctx, void* udata)
{
    unsafe_info* info = (unsafe_info*)udata;

    std::string ret = js_unified_force_call_data(info->ctx, info->command, info->usr->get_call_stack().back());

    info->ret = ret;

    return 1;
}

void managed_duktape_thread(unsafe_info* info)
{
    try{
    int id = get_thread_id(info->ctx);

    ///set thread storage hack
    ///convert from int to size_t
    mongo_lock_proxy::thread_id_storage_hack = (size_t)id;

    if(duk_safe_call(info->ctx, unsafe_wrapper, (void*)info, 0, 1) != 0)
    {
        duk_dup(info->ctx, -1);

        printf("Err in safe wrapper %s\n", duk_safe_to_string(info->ctx, -1));

        duk_pop(info->ctx);
    }

    //duk_pop(info->ctx);

    info->finished = 1;
    }
    catch(...)
    {
        std::cout << "what?\n";
    }
}

struct cleanup_auth_at_exit
{
    std::mutex& to_lock;
    std::map<std::string, int>& to_cleanup;
    std::string& auth;
    bool blocked = true;

    cleanup_auth_at_exit(std::mutex& lk, std::map<std::string, int>& cleanup, std::string& ath) : to_lock(lk), to_cleanup(cleanup), auth(ath) {}

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

void sleep_thread_for(std::thread& t, int sleep_ms)
{
    pthread_t thread = t.native_handle();
    void* native_handle = pthread_gethandle(thread);
    SuspendThread(native_handle);
    Sleep(sleep_ms);
    ResumeThread(native_handle);
}

void async_realtime_script_handler(duk_context* nctx, shared_data& shared, command_handler_state& state, double& time_of_last_on_update, std::string& ret,
                                   std::atomic_bool& terminated, std::atomic_bool& request_long_sleep, std::atomic_bool& fedback, int current_id,
                                   std::atomic_bool& force_terminate, std::atomic<double>& avg_exec_time)
{
    try{
    sf::Clock clk;

    avg_exec_time = 4;

    duk_push_thread_new_globalenv(nctx);
    duk_context* ctx = duk_get_context(nctx, -1);

    duk_dup(nctx, -2);

    duk_xmove_top(ctx, nctx, 1);

    //duk_context* ctx = nctx;

    while(!state.should_terminate_any_realtime && !force_terminate)
    {
        try
        {
            sf::Clock elapsed;

            bool any = false;

            if(duk_has_prop_string(ctx, -1, "on_wheelmoved"))
            {
                if(state.has_mousewheel_state(current_id))
                {
                    vec2f wheel = state.consume_mousewheel_state(current_id);

                    duk_push_string(ctx, "on_wheelmoved");
                    push_dukobject(ctx, "x", wheel.x(), "y", wheel.y());

                    if(duk_pcall_prop(ctx, -3, 1) != DUK_EXEC_SUCCESS)
                    {
                        ret = duk_safe_to_std_string(ctx, -1);
                        force_terminate = true;
                        duk_pop(ctx);
                        break;
                    }

                    duk_pop(ctx);
                }

                any = true;
            }

            if(duk_has_prop_string(ctx, -1, "on_resize"))
            {
                if(state.has_new_width_height(current_id))
                {
                    std::pair<int, int> width_height = state.consume_width_height(current_id);

                    width_height.first = clamp(width_height.first, 5, 400);
                    width_height.second = clamp(width_height.second, 5, 400);

                    duk_push_string(ctx, "on_resize");
                    push_dukobject(ctx, "width", width_height.first, "height", width_height.second);

                    if(duk_pcall_prop(ctx, -3, 1) != DUK_EXEC_SUCCESS)
                    {
                        ret = duk_safe_to_std_string(ctx, -1);
                        force_terminate = true;
                        duk_pop(ctx);
                        break;
                    }

                    duk_pop(ctx);
                }

                ///DONT SET real_operation
                any = true;
            }

            if(duk_has_prop_string(ctx, -1, "on_input"))
            {
                std::vector<unprocessed_key_info> unprocessed_keystrokes;

                {
                    safe_lock_guard guard(state.lock);

                    unprocessed_keystrokes = state.unprocessed_keystrokes[current_id];

                    state.unprocessed_keystrokes[current_id].clear();
                }

                while(unprocessed_keystrokes.size() > 0)
                {
                    std::string c = unprocessed_keystrokes[0].key;
                    bool is_repeat = unprocessed_keystrokes[0].is_repeat;
                    unprocessed_keystrokes.erase(unprocessed_keystrokes.begin());

                    //std::cout << "called on_input " << get_wall_time() << " " << c << std::endl;

                    duk_push_string(ctx, "on_input");
                    duk_push_string(ctx, c.c_str());
                    push_duk_val(ctx, is_repeat);

                    if(duk_pcall_prop(ctx, -4, 2) != DUK_EXEC_SUCCESS)
                    {
                        ret = duk_safe_to_std_string(ctx, -1);
                        force_terminate = true;
                        duk_pop(ctx);
                        break;
                    }

                    duk_pop(ctx);
                }

                ///DONT SET real_operation
                any = true;
            }

            if(duk_has_prop_string(ctx, -1, "on_update"))
            {
                double current_dt = clk.restart().asMicroseconds() / 1000.;

                //std::cout << " dt " << current_dt << std::endl;

                duk_push_string(ctx, "on_update");
                duk_push_number(ctx, current_dt);

                if(duk_pcall_prop(ctx, -3, 1) != DUK_EXEC_SUCCESS)
                {
                    ret = duk_safe_to_std_string(ctx, -1);
                    force_terminate = true;
                    duk_pop(ctx);
                    break;
                }

                duk_pop(ctx);

                any = true;
            }

            if(duk_has_prop_string(ctx, -1, "on_draw"))
            {
                duk_push_string(ctx, "on_draw");

                if(duk_pcall_prop(ctx, -2, 0) != DUK_EXEC_SUCCESS)
                {
                    ret = duk_safe_to_std_string(ctx, -1);
                    force_terminate = true;
                    duk_pop(ctx);
                    break;
                }

                if(!duk_is_undefined(ctx, -1))
                {
                    async_pipe(ctx);
                }

                duk_pop(ctx);

                any = true;
            }

            if(!any)
            {
                force_terminate = true;
                break;
            }

            request_long_sleep = true;

            //std::cout << "Full frame too " << elapsed.getElapsedTime().asMicroseconds() / 1000. << std::endl;

            double exec_time = elapsed.getElapsedTime().asMicroseconds() / 1000.;
            avg_exec_time = (avg_exec_time + exec_time)/2.;

            while(!fedback)
            {
                std::this_thread::yield();
            }

            fedback = false;
        }
        catch(...)
        {
            force_terminate = true;

            printf("Caugh exception in async\n");
            break;
        }
    }

    duk_pop(ctx);

    terminated = true;
    }
    catch(...)
    {
        terminated = true;
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
            shared.value()->execution_is_blocked = false;

        blocked = false;
    }

    ~execution_blocker_guard()
    {
        if(shared && blocked)
            shared.value()->execution_is_blocked = false;
    }
};

std::string run_in_user_context(const std::string& username, const std::string& command, std::optional<std::shared_ptr<shared_command_handler_state>> all_shared, std::optional<float> custom_exec_time_s, bool force_exec)
{
    try
    {
        execution_blocker_guard exec_guard(all_shared);

        user usr;

        {
            mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(-2);
            mongo_ctx.change_collection(username);

            if(!usr.load_from_db(mongo_ctx, username))
                return "No such user";
        }

        static std::mutex id_mut;

        static std::map<std::string, int> auth_guard;
        static int32_t gthread_id = 1;
        int32_t local_thread_id;

        {
            safe_lock_guard lk(id_mut);

            local_thread_id = gthread_id++;

            if(!force_exec)
            {
                if(auth_guard[usr.auth] == 1)
                    return make_error_col("Cannot run two scripts at once in different contexts!");

                auth_guard[usr.auth] = 1;
            }
        }

        cleanup_auth_at_exit cleanup(id_mut, auth_guard, usr.auth);

        if(force_exec)
        {
            cleanup.unblock();
            exec_guard.unblock();
        }

        duk_context* ctx;
        //init_js_interop(sd, std::string());
        ctx = create_sandbox_heap();

        duk_memory_functions funcs;
        duk_get_memory_functions(ctx, &funcs);

        sandbox_data* sand_data = (sandbox_data*)funcs.udata;

        //fully_freeze(ctx, "JSON", "Array", "parseInt", "parseFloat", "Math", "Date", "Error", "Number", "Object", "Duktape");

        usr.cleanup_call_stack(local_thread_id);
        std::string executing_under = usr.get_call_stack().back();

        shared_duk_worker_state* shared_duk_state = new shared_duk_worker_state;

        startup_state(ctx, executing_under, executing_under, "invoke", usr.get_call_stack(), shared_duk_state);

        set_global_int(ctx, "thread_id", local_thread_id);

        if(all_shared.has_value())
        {
            shared_data& shared = all_shared.value()->shared;

            dukx_put_pointer(ctx, &shared, "shared_data_ptr");
            ///taking a pointer to a shared pointer passed in by value is a great idea
            ///right?
            dukx_put_pointer(ctx, &all_shared.value()->state, "command_handler_state_pointer");
        }
        else
        {
            dukx_put_pointer(ctx, nullptr, "shared_data_ptr");
            dukx_put_pointer(ctx, nullptr, "command_handler_state_pointer");
        }

        unsafe_info inf;
        inf.usr = &usr;
        inf.command = command;
        inf.ctx = ctx;

        std::thread* launch = new std::thread(managed_duktape_thread, &inf);

        if(all_shared.has_value())
        {
            all_shared.value()->state.number_of_oneshot_scripts++;
        }

        //launch->detach();

        bool terminated = false;

        //sf::Clock clk;
        #ifdef TESTING
        float max_time_ms = 50000;
        #else
        float max_time_ms = 5000;
        #endif
        float db_grace_time_ms = 2000;

        if(custom_exec_time_s.has_value())
            max_time_ms = custom_exec_time_s.value() * 1000.;

        auto time_start = std::chrono::high_resolution_clock::now();

        #define ACTIVE_TIME_MANAGEMENT
        #ifdef ACTIVE_TIME_MANAGEMENT
        int active_time_slice_ms = 1;
        int sleeping_time_slice_ms = 1;
        #endif // ACTIVE_TIME_MANAGEMENT

        script_management_mode::mode current_mode = script_management_mode::DEFAULT;

        while(!inf.finished)
        {
            int sleep_mult = 1;

            if(all_shared.has_value())
            {
                sleep_mult = all_shared.value()->live_work_units();
            }

            #ifdef ACTIVE_TIME_MANAGEMENT
            {
                Sleep(active_time_slice_ms);

                pthread_t thread = launch->native_handle();
                void* native_handle = pthread_gethandle(thread);
                SuspendThread(native_handle);
                Sleep(sleeping_time_slice_ms * sleep_mult);
                ResumeThread(native_handle);
            }
            #endif // ACTIVE_TIME_MANAGEMENT

            auto time_current = std::chrono::high_resolution_clock::now();

            auto diff = time_current - time_start;

            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(diff);

            double elapsed = dur.count();

            ///you know
            ///if we change db stuff to happen on a separate thread and spinlock
            ///the worker trying to retrieve data
            ///so that it can throw on a long calling db op
            ///we could remove forceful thread termination and dear god my life
            ///would become so much easier
            ///maybe a structure with a thread pool to manage incoming db requests and
            ///service them, which returns an event thing to wait on
            if(elapsed >= max_time_ms + db_grace_time_ms)
            {
                pthread_t thread = launch->native_handle();

                void* native_handle = pthread_gethandle(thread);

                printf("UNSAFE THREAD TERMINATION\n");

                ///this is obviously very unsafe, doubly so due to the whole mutex thing, which may leave them locked
                ///going to need to have an intermittent sync point, where all threads block going in and we free all locks or something
                SuspendThread(native_handle);
                TerminateThread(native_handle, 1);
                CloseHandle(native_handle);

                inf.ret = "Ran for longer than " + std::to_string((int)max_time_ms) + "ms and was uncooperatively terminated";

                terminated = true;

                break;
            }

            if(elapsed >= max_time_ms)
            {
                sand_data->terminate_semi_gracefully = true;
            }

            Sleep(1);
        }

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

        if(inf.finished && !terminated)
        {
            launch->join();
            delete launch;

            if(current_mode == script_management_mode::REALTIME && all_shared.has_value() && !sand_data->terminate_semi_gracefully && !sand_data->terminate_realtime_gracefully)
            {
                exec_guard.unblock();
                cleanup.unblock();

                //all_shared.value()->state.number_of_realtime_scripts++;

                launched_realtime = true;

                int current_id = 0;

                {
                    mongo_requester req;

                    mongo_lock_proxy mctx = get_global_mongo_global_properties_context(-2);

                    req.set_prop("worker_id_is_gid", 1);

                    auto found = req.fetch_from_db(mctx);

                    if(found.size() == 0)
                    {
                        req.set_prop("worker_id_gid", 0);

                        req.insert_in_db(mctx);
                    }
                    else
                    {
                        current_id = found[0].get_prop_as_integer("worker_id_gid");

                        mongo_requester to_set;
                        to_set.set_prop("worker_id_gid", current_id+1);

                        req.update_in_db_if_exact(mctx, to_set);
                    }

                    printf("%i cid\n", current_id);
                }

                launched_realtime_id = current_id;

                all_shared.value()->state.add_realtime_script(current_id);

                //double last_time = get_wall_time();

                bool is_valid = !duk_is_undefined(ctx, -1);

                std::atomic_bool request_going{false};
                std::atomic_bool request_finished{true};
                std::atomic_bool fedback{false};

                ///finished last means did we execute the last element in the chain
                ///so that we dont execute more than one whole sequence in a frame
                std::atomic_bool finished_last{false};

                ///default is 60
                double current_framerate = get_global_number(ctx, "framerate_limit");

                current_framerate = clamp(current_framerate, 1., 60.);

                const double max_frame_time_ms = (1./current_framerate) * 1000.;
                const double max_allowed_frame_time_ms = max_frame_time_ms/4; ///before we sleep for (max_frame - max_allowed)
                double current_frame_time_ms = 0;
                double current_goodwill_ms = 0;
                double max_goodwill_ms = (6./16.) * max_frame_time_ms;

                std::atomic<double> avg_exec_time = 0;
                double estimated_time_remaining = max_allowed_frame_time_ms;

                double time_of_last_on_update = get_wall_time();

                sf::Clock clk;

                if(is_valid)
                {
                    command_handler_state& cstate = all_shared.value()->state;
                    shared_data& cqueue = all_shared.value()->shared;

                    std::atomic_bool terminated{false};
                    std::atomic_bool request_long_sleep{false};
                    std::atomic_bool force_terminate{false};

                    ///pipe window size
                    {
                        auto [width, height] = shared_duk_state->get_width_height();

                        try
                        {
                            using json = nlohmann::json;

                            json j;
                            j["id"] = current_id;
                            j["width"] = width;
                            j["height"] = height;
                            j["script_name"] = get_global_string(ctx, "realtime_script_name");

                            all_shared.value()->shared.add_back_write("command_realtime_json " + j.dump());
                        }
                        catch(...){}
                    }

                    std::thread thrd = std::thread(async_realtime_script_handler, ctx, std::ref(cqueue), std::ref(cstate), std::ref(time_of_last_on_update), std::ref(inf.ret),
                                                   std::ref(terminated), std::ref(request_long_sleep), std::ref(fedback), current_id, std::ref(force_terminate),
                                                   std::ref(avg_exec_time));

                    while(!force_terminate)
                    {
                        if(shared_duk_state->has_output_data_available())
                        {
                            std::string str = shared_duk_state->consume_output_data();

                            if(str != "")
                            {
                                try
                                {
                                    using json = nlohmann::json;

                                    json j;
                                    j["id"] = current_id;
                                    j["msg"] = str;

                                    all_shared.value()->shared.add_back_write("command_realtime_json " + j.dump());
                                }
                                catch(...){}
                            }
                        }

                        if(all_shared.value()->state.should_terminate_any_realtime)
                            break;

                        {
                            safe_lock_guard guard(all_shared.value()->state.lock);

                            if(all_shared.value()->state.should_terminate_realtime[current_id])
                                break;
                        }

                        if(!shared_duk_state->is_realtime())
                            break;

                        if(all_shared.value()->live_work_units() > 10)
                            break;

                        shared_duk_state->set_key_state(all_shared.value()->state.get_key_state(current_id));
                        shared_duk_state->set_mouse_pos(all_shared.value()->state.get_mouse_pos(current_id));

                        double dt_ms = clk.restart().asMicroseconds() / 1000.;

                        current_frame_time_ms += dt_ms;
                        estimated_time_remaining -= dt_ms;

                        bool long_sleep_requested = request_long_sleep;

                        if(current_frame_time_ms >= max_allowed_frame_time_ms + current_goodwill_ms || long_sleep_requested)
                        {
                            //std::cout << "ftime " << current_frame_time_ms << std::endl;

                            if(current_frame_time_ms >= max_allowed_frame_time_ms)
                            {
                                current_goodwill_ms -= current_frame_time_ms - max_allowed_frame_time_ms;
                            }


                            double work_units = current_frame_time_ms / (max_allowed_frame_time_ms);
                            work_units = clamp(work_units, 0., 1.);
                            all_shared.value()->state.set_realtime_script_delta(current_id, work_units);


                            ///THIS ISNT QUITE CORRECT
                            ///it makes the graphics programmer sad as frames will come out IRREGULARLY
                            ///needs to take into account the extra time we've elapsed for
                            double to_sleep = max_frame_time_ms - current_frame_time_ms;

                            to_sleep = clamp(floor(to_sleep), 0., 200.);

                            to_sleep = to_sleep * all_shared.value()->live_work_units();

                            //std::cout << "live work units " << all_shared.value()->live_work_units() << std::endl;

                            //printf("%f sleeping for\n", to_sleep);


                            //sf::Clock slept_for;

                            sleep_thread_for(thrd, to_sleep);

                            estimated_time_remaining = avg_exec_time;

                            //std::cout << "slept for " << slept_for.getElapsedTime().asMicroseconds() / 1000. << std::endl;

                            clk.restart();

                            if(long_sleep_requested)
                            {
                                request_long_sleep = false;

                                fedback = true;
                            }

                            if(current_frame_time_ms < max_allowed_frame_time_ms)
                            {
                                current_goodwill_ms += max_allowed_frame_time_ms - current_frame_time_ms;
                            }

                            current_goodwill_ms = clamp(current_goodwill_ms, 0.f, max_goodwill_ms);

                            current_frame_time_ms = 0;
                        }


                        if(estimated_time_remaining >= 1.5f)
                            Sleep(1);
                        else
                            std::this_thread::yield();
                    }

                    force_terminate = true;
                    fedback = true;

                    Sleep(50);

                    //sand_data->terminate_semi_gracefully = true;
                    sand_data->terminate_realtime_gracefully = true;

                    thrd.join();

                    while(!terminated)
                    {

                    }

                    if(shared_duk_state->close_window_on_exit())
                    {
                        try
                        {
                            using json = nlohmann::json;

                            json j;
                            j["id"] = current_id;
                            j["close"] = true;

                            all_shared.value()->shared.add_back_write("command_realtime_json " + j.dump());
                        }
                        catch(...)
                        {

                        }
                    }
                }

                printf("Ended realtime\n");
            }
        }

        if(terminated)
        {
            for(auto& i : mongo_databases)
            {
                i->unlock_if(local_thread_id);
            }
        }

        //managed_duktape_thread(&inf);

        //if(!terminated)
        try
        {
            if(terminated)
                printf("Attempting unsafe resource cleanup\n");

            teardown_state(ctx);

            duk_destroy_heap(ctx);
        }
        catch(...)
        {
            printf("Failed to cleanup resources\n");
        }

        printf("cleaned up unsafe\n");


        if(launched_realtime)
        {
            all_shared.value()->state.remove_realtime_script(launched_realtime_id);;
            //all_shared.value()->state.number_of_realtime_scripts_terminated++;
        }

        std::string ret = inf.ret;

        return ret;
    }
    catch(...)
    {
        return "Caught exception";
    }
}

void throwaway_user_thread(const std::string& username, const std::string& command, std::optional<float> custom_exec_time_s, bool force_exec)
{
    std::thread(run_in_user_context, username, command, std::nullopt, custom_exec_time_s, force_exec).detach();
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

std::string hex_to_binary(const std::string& in)
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

    /*{
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
        usr.load_from_db(ctx, usr.name);
    }*/
}

std::string get_update_message()
{
    return "If you cannot login, a bad update deleted key.key files. PM me (20k) on discord with a username that you owned and I will recover it. Run #scripts.core() to get started";
}

void delete_notifs_for(const std::string& name)
{
    ///DELETE NOTIFS
    {
        mongo_lock_proxy notifs_db = get_global_mongo_pending_notifs_context(-2);
        notifs_db.change_collection(name);

        bson_t bson;
        bson_init(&bson);

        notifs_db->remove_bson(name, &bson);
        bson_destroy(&bson);
    }
}

void delete_user_db_for(const std::string& name)
{
    ///delete user db
    {
        mongo_lock_proxy user_db = get_global_mongo_user_accessible_context(-2);
        user_db.change_collection(name);

        bson_t bson;
        bson_init(&bson);
        user_db->remove_bson(name, &bson);
        bson_destroy(&bson);
    }
}

void delete_nodes_for(const std::string& name)
{
    ///delete nodes
    {
        mongo_lock_proxy nodes_db = get_global_mongo_node_properties_context(-2);
        nodes_db.change_collection(name);

        mongo_requester req;
        req.set_prop("owned_by", name);

        req.remove_all_from_db(nodes_db);

        user_nodes::delete_from_cache(name);
    }
}

void delete_user_for(const std::string& name)
{
    ///DELETE USER
    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);
        ctx.change_collection(name);

        mongo_requester req;
        req.set_prop("name", name);

        req.remove_all_from_db(ctx);

        user::delete_from_cache(name);
    }
}

void delete_npc_db_for(const std::string& name)
{
    mongo_lock_proxy ctx = get_global_mongo_npc_properties_context(-2);

    npc_prop_list npc_props;
    npc_props.set_as("name", name);

    npc_props.remove_from_db(ctx);
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
            for(auto it = (*i.user_list).begin(); it != (*i.user_list).end();)
            {
                if(*it == name)
                {
                    it = i.user_list->erase(it);

                    mongo_lock_low_level ctx = get_global_mongo_low_level_structure_context(-2);

                    i.overwrite_in_db(ctx);
                }
                else
                {
                    it++;
                }
            }
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
            props.load_from_db(ctx, from_name);
            ///delete those bad boys
            props.remove_from_db(ctx);

            ///rename props
            props.data[props.key_name] = to_name;
            ///insert into db under new name
            props.overwrite_in_db(ctx);
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

///should really queue this or something
std::string delete_user(command_handler_state& state, const std::string& str, bool cli_force)
{
    std::string auth;

    std::string name;

    if(!cli_force)
    {

        {
            auth = state.get_auth();
        }

        if(auth == "")
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

            if(to_delete.auth != auth)
                return "Invalid Auth";

            if(SHOULD_RATELIMIT(auth, DELETE_USER))
                return "You may only delete 1 user per hour";
        }
    }
    else
    {
        name = str;
    }

    ///DELETE ITEMS
    {
        mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(-2);

        mongo_requester req;
        req.set_prop("owner", name);

        req.remove_all_from_db(items_ctx);
    }

    ///DELETE AUTH
    {
        mongo_lock_proxy auth_db = get_global_mongo_global_properties_context(-2);

        mongo_requester req;
        req.set_prop_bin("account_token", auth);

        auto found = req.fetch_from_db(auth_db);

        if(found.size() == 1)
        {
            auto found_req = found[0];

            auto arr = str_to_array(found_req.get_prop("users"));

            for(int i=0; i < (int)arr.size(); i++)
            {
                if(arr[i] == name)
                {
                    arr.erase(arr.begin() + i);
                    i--;
                    continue;
                }
            }

            found_req.set_prop("users", array_to_str(arr));

            req.update_in_db_if_exact(auth_db, found_req);
        }
        else
        {
            if(!cli_force)
                return "Auth Error: Purple Catepillar";
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

    delete_user_db_for(name);

    delete_nodes_for(name);

    delete_user_for(name);

    delete_links_for(name);

    delete_structure_for(name);

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

    return banned.find(user) == banned.end();
}

std::string handle_command_impl(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str, bool& is_auth)
{
    is_auth = false;

    printf("yay command\n");

    lg::log(str);

    if(starts_with(str, "user "))
    {
        if(all_shared->state.get_auth() == "")
            return make_error_col("Please create account with \"register client\"");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() != 2)
            return make_error_col("Invalid Command Error");

        std::string user_name = strip_whitespace(split_string[1]);

        if(!is_valid_string(user_name))
            return make_error_col("Invalid username");

        if(!is_allowed_user(user_name))
            return make_error_col("Claiming or using this specific username is disallowed. If you already own it you may #delete_user the user in question");

        bool user_exists = false;

        {
            bool should_set = false;
            user fnd;

            {
                mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

                if(fnd.exists(mongo_user_info, user_name))
                {
                    user_exists = true;

                    fnd.load_from_db(mongo_user_info, user_name);

                    should_set = true;
                }

                auto allowed = fnd.get_call_stack();
            }

            if(should_set)
            {
                if(fnd.auth != all_shared->state.get_auth())
                {
                    return make_error_col("Incorrect Auth, someone else has registered this account or you are using a different pc and key.key file");
                }

                all_shared->state.set_user_name(fnd.name);
            }

            /*for(auto& i : allowed)
            {
                std::cout <<" a  " << i << std::endl;
            }*/

            /*{
                bool overwrite = false;

                mongo_user_info.change_collection(user_name, true);

                if(state.current_user.exists(mongo_user_info, user_name) && !user_exists)
                {
                    overwrite = true;
                    user_exists = true;

                    state.current_user.load_from_db(mongo_user_info, user_name);
                }

                mongo_user_info.change_collection("all_users", true);

                if(overwrite && !state.current_user.exists(mongo_user_info, user_name))
                {
                    state.current_user.construct_new_user(mongo_user_info, user_name, state.current_user.auth);
                    state.current_user.overwrite_user_in_db(mongo_user_info);

                    return "User Migrated. Please run this command again";
                }
            }*/
        }

        if(user_exists)
        {
            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(-2);

                auth to_check;
                to_check.load_from_db(mongo_ctx, all_shared->state.get_auth());

                if(!to_check.valid)
                    return make_error_col("Trying something sneaky eh?");

                to_check.insert_user_exclusive(user_name);
                to_check.overwrite_in_db(mongo_ctx);
            }

            return "Switched to User";
        }
        else
        {
            all_shared->state.set_user_name("");

            {
                auto fauth = all_shared->state.get_auth();

                mongo_lock_proxy mongo_ctx = get_global_mongo_global_properties_context(-2);

                auth to_check;
                to_check.load_from_db(mongo_ctx, fauth);

                if(!to_check.valid)
                    return make_error_col("Trying something sneaky eh 2?");

                #ifdef TESTING
                #define MAX_USERS 999
                #else // TESTING
                #define MAX_USERS 8
                #endif

                if(to_check.users.size() >= MAX_USERS)
                    return make_error_col("Max users " + std::to_string(to_check.users.size()) + "/" + std::to_string(MAX_USERS));

                to_check.insert_user_exclusive(user_name);
                to_check.overwrite_in_db(mongo_ctx);
            }

            {

                user new_user;

                auto fauth = all_shared->state.get_auth();

                {
                    mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

                    new_user.construct_new_user(mongo_user_info, user_name, fauth);
                    new_user.load_from_db(mongo_user_info, user_name);
                    new_user.overwrite_user_in_db(mongo_user_info);
                }

                all_shared->state.set_user_name(new_user.name);
            }

            std::string cur_name = all_shared->state.get_user_name();

            on_create_user(cur_name);

            all_shared->state.set_user_name(cur_name);

            return make_success_col("Constructed new User");
        }
    }
    else if(starts_with(str, "#delete_user "))
    {
        return delete_user(all_shared->state, str);
    }
    else if(starts_with(str, "#up ") || starts_with(str, "#dry ") || starts_with(str, "#up_es6 "))
    {
        if(all_shared->state.get_auth() == "")
            return make_error_col("No Auth");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 3)
        {
            if(starts_with(str, "#up "))
                return "Syntax is #up scriptname or invalid scriptname";
            if(starts_with(str, "#dry "))
                return "Syntax is #dry scriptname or invalid scriptname";
            if(starts_with(str, "#up_es6 ")) ///this is not client facing
                return "Syntax is #up scriptname or invalid scriptname";
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = all_shared->state.get_user_name() + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
        {
            return make_error_col("Invalid script name " + fullname);
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

            duk_context* ctx;
            ctx = duk_create_heap_default();
            register_funcs(ctx, 0, "core");


            script_info script_inf;
            std::string compile_error = script_inf.load_from_unparsed_source(ctx, data_source, fullname, is_es6, false);

            duk_destroy_heap(ctx);

            if(compile_error != "")
                return compile_error;

            user cur;

            {
                auto uname = all_shared->state.get_user_name();

                mongo_lock_proxy user_locks = get_global_mongo_user_info_context(-2);

                cur.load_from_db(user_locks, uname);
            }

            std::map<std::string, double> user_details;

            {
                user_details = cur.get_total_user_properties(-2);
            }

            int num_chars = script_inf.unparsed_source.size();
            int max_chars = user_details["char_count"];

            if(!starts_with(str, "#dry "))
            {
                script_inf.in_public = was_public;

                mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

                script_inf.overwrite_in_db(mongo_ctx);
            }

            std::string rstr = "Upload Successful ";

            if(starts_with(str, "#dry "))
                rstr = "Dry Upload Successful ";

            return make_success_col(rstr + std::to_string(num_chars) + "/" + std::to_string(max_chars));
        }
    }
    else if(starts_with(str, "#remove "))
    {
        if(all_shared->state.get_auth() == "")
            return make_error_col("No Auth");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 2)
        {
            return "Syntax is #remove scriptname";
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = all_shared->state.get_user_name() + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
            return make_error_col("Invalid script name " + fullname);

        {
            auto uname = all_shared->state.get_user_name();

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_info script_inf;
            script_inf.name = uname + "." + scriptname;

            if(!script_inf.exists_in_db(mongo_ctx))
                return make_error_col("Script not found");

            mongo_requester request;
            request.set_prop("item_id", script_inf.name);

            request.remove_all_from_db(mongo_ctx);
        }

        return make_success_col("Script removed from server");
    }
    else if(starts_with(str, "#public ") || starts_with(str, "#private "))
    {
        if(all_shared->state.get_auth() == "")
            return make_error_col("No Auth");

        int in_public_state = starts_with(str, "#public ");

        std::vector<std::string> split_string = no_ss_split(str, " ");

        if(split_string.size() < 2)
        {
            return "Syntax is #public scriptname or #private scriptname";
        }

        std::string scriptname = strip_whitespace(split_string[1]);

        std::string fullname = all_shared->state.get_user_name() + "." + scriptname;

        if(!is_valid_full_name_string(fullname))
            return make_error_col("Invalid script name " + fullname);

        {
            auto uname = all_shared->state.get_user_name();

            mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(-2);

            script_info script_inf;
            script_inf.name = uname + "." + scriptname;

            if(!script_inf.exists_in_db(mongo_ctx))
                return make_error_col("Script not found");

            mongo_requester request;
            request.set_prop("item_id", script_inf.name);

            mongo_requester to_set;
            to_set.set_prop("in_public", in_public_state);

            request.update_in_db_if_exact(mongo_ctx, to_set);
        }

        return make_success_col("Success");
    }
    #define ALLOW_SELF_AUTH
    #ifdef ALLOW_SELF_AUTH
    else if(starts_with(str, "register client"))
    {
        std::string to_ret = random_binary_string(128);

        mongo_requester request;
        request.set_prop_bin("account_token", to_ret);

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
    else if(starts_with(str, "auth client ") || starts_with(str, "auth client_hex "))
    {
        printf("auth client\n");

        std::string which_str = "auth client ";

        if(starts_with(str, "auth client_hex "))
            which_str = "auth client_hex ";

        auto pos = str.begin() + which_str.size();;
        std::string auth_token = std::string(pos, str.end());

        if(starts_with(str, "auth client_hex "))
        {
            auth_token = hex_to_binary(auth_token);

            std::cout << "detected hex" << std::endl;
        }

        if(auth_token.length() > 140)
            return make_error_col("Auth too long");

        std::vector<std::string> users;

        {
            mongo_lock_proxy ctx = get_global_mongo_global_properties_context(-2);

            mongo_requester request;
            request.set_prop_bin("account_token", auth_token);

            std::cout << "auth len " << auth_token.size() << std::endl;

            if(request.fetch_from_db(ctx).size() == 0)
                return make_error_col("Auth Failed, have you run \"register client\" at least once?");


            auth user_auth;

            user_auth.load_from_db(ctx, auth_token);
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

        std::cout << auth_string << std::endl;

        return make_success_col("Auth Success") + "\n" + full_string + auth_string + "\n" + get_update_message();
    }
    else if(starts_with(str, "auth client") || starts_with(str, "auth client_hex"))
    {
        return "No Auth, send \"register client\"";
    }
    else
    {
        auto name = all_shared->state.get_user_name();

        {
            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

            if(!user().exists(mongo_user_info, name))
                return "No account or not logged in";
        }

        return run_in_user_context(name, str, all_shared);
    }

    return make_error_col("Command Not Found or Unimplemented");
}

void strip_old_msg_or_notif(mongo_lock_proxy& ctx)
{
    nlohmann::json to_fetch;
    to_fetch["processed"] = 1;

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
            req["processed"] = 1;
            remove_all_from_db(ctx, req);
        }
    }
}

std::vector<nlohmann::json> get_and_update_chat_msgs_for_user(user& usr)
{
    std::vector<nlohmann::json> found;

    usr.cleanup_call_stack(-2);

    {
        mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
        ctx.change_collection(usr.get_call_stack().back());

        nlohmann::json to_send;
        to_send["is_chat"] = 1;
        to_send["processed"] = 0;

        found = fetch_from_db(ctx, to_send);

        nlohmann::json old_search = to_send;

        to_send["processed"] = 1;

        update_in_db_if_exact(ctx, old_search, to_send);
    }

    if(found.size() > 1000)
        found.resize(1000);

    return found;
}

std::vector<nlohmann::json> get_and_update_tells_for_user(user& usr)
{
    std::vector<nlohmann::json> found;

    usr.cleanup_call_stack(-2);

    {
        mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
        ctx.change_collection(usr.get_call_stack().back());

        nlohmann::json to_send;
        to_send["is_tell"] = 1;
        to_send["processed"] = 0;

        found = fetch_from_db(ctx, to_send);

        nlohmann::json old_search = to_send;

        to_send["processed"] = 1;

        update_in_db_if_exact(ctx, old_search, to_send);
    }

    if(found.size() > 1000)
        found.resize(1000);

    return found;
}

std::vector<nlohmann::json> get_and_update_notifs_for_user(user& usr)
{
    std::vector<nlohmann::json> found;

    usr.cleanup_call_stack(-2);

    {
        mongo_lock_proxy ctx = get_global_mongo_pending_notifs_context(-2);
        ctx.change_collection(usr.get_call_stack().back());

        nlohmann::json to_send;
        to_send["is_notif"] = 1;
        to_send["processed"] = 0;

        found = fetch_from_db(ctx, to_send);

        nlohmann::json old_search = to_send;

        to_send["processed"] = 1;

        update_in_db_if_exact(ctx, old_search, to_send);
    }

    if(found.size() > 1000)
        found.resize(1000);

    return found;
}

std::vector<std::string> get_channels_for_user(user& usr)
{
    usr.cleanup_call_stack(-2);

    std::string name = usr.get_call_stack().back();

    /*mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

    user fuser;

    if(!fuser.load_from_db(ctx, name))
        return std::vector<std::string>();

    return str_to_array(fuser.joined_channels);*/

    std::vector<std::string> ret;

    static std::vector<mongo_requester> all_data;
    static std::mutex lock;
    static sf::Clock clk;


    mongo_requester all;
    all.exists_check["channel_name"] = 1;

    std::vector<mongo_requester> found;

    if(clk.getElapsedTime().asSeconds() > 1)
    {
        mongo_lock_proxy ctx = get_global_mongo_chat_channel_propeties_context(-2);

        found = all.fetch_from_db(ctx);
        clk.restart();
    }
    else
    {
        safe_lock_guard guard(lock);

        found = all_data;
    }

    {
        safe_lock_guard guard(lock);

        all_data = found;
    }

    for(auto& i : found)
    {
        std::vector<std::string> users = str_to_array(i.get_prop("user_list"));

        for(auto& k : users)
        {
            if(k == name)
            {
                ret.push_back(i.get_prop("channel_name"));
                break;
            }
        }
    }

    return ret;
}

#if 0
std::string handle_client_poll(user& usr)
{
    std::vector<mongo_requester> found = get_and_update_chat_msgs_for_user(usr);

    std::vector<std::string> channels = get_channels_for_user(usr);

    std::string to_send = "";

    std::string prologue_str = std::to_string(channels.size()) + " " + array_to_str(channels);

    while(prologue_str.size() > 0 && prologue_str.back() == ' ')
        prologue_str.pop_back();

    prologue_str = prologue_str + " ";

    to_send = std::to_string(prologue_str.size()) + " " + prologue_str;

    for(mongo_requester& req : found)
    {
        std::string chan = req.get_prop("channel");

        std::vector<mongo_requester> to_col{req};

        std::string full_str = chan + " " + prettify_chat_strings(to_col);

        to_send += std::to_string(full_str.size()) + " " + full_str;

        //to_send += "chat_api " + std::to_string(full_str.size()) + " " + full_str;
    }

    //std::cout << to_send << std::endl;

    if(to_send == "")
        return "";

    return "chat_api " + to_send;
}
#endif // 0

std::string handle_client_poll_json(user& usr)
{
    std::vector<nlohmann::json> found = get_and_update_chat_msgs_for_user(usr);
    std::vector<std::string> channels = get_channels_for_user(usr);

    std::vector<nlohmann::json> tells = get_and_update_tells_for_user(usr);
    std::vector<nlohmann::json> notifs = get_and_update_notifs_for_user(usr);

    using json = nlohmann::json;

    json all;

    all["channels"] = channels;

    /*std::cout << "poll json\n";

    for(auto& i : channels)
    {
        std::cout << "CHAN " << i << std::endl;
    }*/

    std::vector<json> cdata;

    for(nlohmann::json& req : found)
    {
        if(req.count("channel") == 0 || req["channel"].is_string() == false)
            continue;

        json api;
        std::string chan = req["channel"];
        std::vector<nlohmann::json> to_col{req};
        std::string pretty = prettify_chat_strings(to_col);

        api["channel"] = chan;
        api["text"] = pretty;

        cdata.push_back(api);
    }

    std::vector<json> tdata;

    for(nlohmann::json& req : tells)
    {
        std::vector<nlohmann::json> all{req};

        json api;

        api["user"] = req["user"];
        api["text"] = prettify_chat_strings(all, false);

        tdata.push_back(api);
    }

    std::vector<std::string> ndata;

    for(nlohmann::json& req : notifs)
    {
        ndata.push_back(req["msg"]);
    }

    all["data"] = cdata;
    all["tells"] = tdata;
    all["notifs"] = ndata;
    all["user"] = usr.get_call_stack().back();

    return "chat_api_json " + all.dump();
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

std::string handle_autocompletes(const std::string& username, const std::string& in)
{
    std::vector<std::string> dat = no_ss_split(in, " ");

    if(dat.size() < 2)
        return "server_scriptargs_invalid";

    std::string script = dat[1];

    if(!is_valid_full_name_string(script))
        return "server_scriptargs_invalid " + script;

    if(SHOULD_RATELIMIT(username, AUTOCOMPLETES))
        return "server_scriptargs_ratelimit " + script;

    auto opt_arg = get_uniform_script_args(script);

    if(!opt_arg.has_value())
        return "server_scriptargs_invalid " + script;

    auto args = *opt_arg;

    std::string intro = "server_scriptargs " + std::to_string(script.size()) + " " + script + " ";

    std::string ret;

    for(script_arg& arg : args)
    {
        ret += std::to_string(arg.key.size()) + " " + arg.key + " ";
        ret += std::to_string(arg.val.size()) + " " + arg.val + " ";
    }

    ///if!public && not owned by me
    ///return nothing

    return intro + ret;
}

std::string handle_autocompletes_json(const std::string& username, const std::string& in)
{
    std::vector<std::string> dat = no_ss_split(in, " ");

    if(dat.size() < 2)
        return "server_scriptargs_invalid_json";

    std::string script = dat[1];

    using json = nlohmann::json;

    json obj;
    obj["script"] = script;

    if(!is_valid_full_name_string(script))
        return "server_scriptargs_invalid_json " + obj.dump();

    if(SHOULD_RATELIMIT(username, AUTOCOMPLETES))
        return "server_scriptargs_ratelimit_json " + obj.dump();

    auto opt_arg = get_uniform_script_args(script);

    if(!opt_arg.has_value())
        return "server_scriptargs_invalid_json " + obj.dump();

    auto args = *opt_arg;

    std::string intro = "server_scriptargs_json ";

    std::vector<std::string> keys;
    std::vector<std::string> vals;

    for(script_arg& arg : args)
    {
        keys.push_back(arg.key);
        vals.push_back(arg.val);
    }

    obj["keys"] = keys;
    obj["vals"] = vals;

    return intro + obj.dump();
}

std::string handle_command(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str)
{
    //lg::log("Log Command " + str);

    std::string client_command = "client_command ";
    std::string client_command_tagged = "client_command_tagged ";
    std::string client_chat = "client_chat ";
    std::string client_poll = "client_poll";
    std::string client_poll_json = "client_poll_json";

    std::string client_scriptargs = "client_scriptargs ";
    std::string client_scriptargs_json = "client_scriptargs_json ";

    std::string current_user = all_shared->state.get_user_name();
    std::string current_auth = all_shared->state.get_auth();

    user found;

    if(current_user != "")
    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

        if(!found.load_from_db(ctx, current_user))
        {
            all_shared->state.set_user_name("");

            return "command Invalid User";
        }

        if(found.auth != current_auth)
        {
            all_shared->state.set_user_name("");

            return "command Invalid Auth";
        }
    }

    if(starts_with(str, client_command) || starts_with(str, client_command_tagged))
    {
        std::string to_exec;
        std::string tag;
        bool tagged = starts_with(str, client_command_tagged);

        if(tagged)
        {
            int idx = client_command_tagged.size();

            for(int i=idx; i < (int)str.size() && str[i] != ' '; i++)
            {
                tag += str[i];
            }

            int total_size = client_command_tagged.size() + tag.size() + 1;

            if(total_size >= (int)str.size())
                return "command_tagged " + tag + " invalid tag";

            to_exec = std::string(str.begin() + client_command_tagged.size() + tag.size() + 1, str.end());
        }
        else
            to_exec = std::string(str.begin() + client_command.size(), str.end());

        bool is_auth = false;

        std::string ret = handle_command_impl(all_shared, to_exec, is_auth);

        if(is_auth)
        {
            return "command_auth " + ret;
        }

        if(tagged)
        {
            return "command_tagged " + tag + " " + ret;
        }
        else
        {
            return "command " + ret;
        }
    }

    if(starts_with(str, client_chat))
    {
        std::string to_exec(str.begin() + client_chat.size(), str.end());

        bool is_auth = false;

        handle_command_impl(all_shared, to_exec, is_auth);

        return "";
    }

    ///matches both client poll and json

    ///this path specifically may be called in parallel with the other parts
    ///hence the current user guard
    if(starts_with(str, client_poll))
    {
        if(current_auth == "" || current_user == "")
            return "";

        if(SHOULD_RATELIMIT(current_auth, POLL))
            return "";

        {
            mongo_lock_proxy mongo_user_info = get_global_mongo_user_info_context(-2);

            user u1;

            if(!u1.load_from_db(mongo_user_info, current_user))
                return "";

            all_shared->state.set_user_name(u1.name);
        }

        std::string cur_name = all_shared->state.get_user_name();

        if(starts_with(str, client_poll_json))
        {
            user usr;

            {
                mongo_lock_proxy user_info = get_global_mongo_user_info_context(-2);

                if(!usr.load_from_db(user_info, cur_name))
                    return "command error invalid username in client_poll_json";
            }

            auto ret = handle_client_poll_json(usr);

            return ret;
        }

        if(starts_with(str, client_poll))
        {
            /*auto ret = handle_client_poll(cur);

            all_shared->state.set_user(cur);

            return ret;*/

            return "command client_poll unsupported";
        }
    }

    if(starts_with(str, client_scriptargs))
    {
        if(current_auth == "" || current_user == "")
            return "";

        return handle_autocompletes(current_user, str);
    }

    if(starts_with(str, client_scriptargs_json))
    {
        if(current_auth == "" || current_user == "")
            return "";

        return handle_autocompletes_json(current_user, str);
    }

    return "command Command not understood";
}

void async_handle_command(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& str)
{
    std::thread([=]()
                {
                    try{
                    std::string result = handle_command(all_shared, str);

                    if(result == "")
                        return;

                    shared_data& shared = all_shared->shared;
                    shared.add_back_write(result);
                    }
                    catch(...)
                    {
                        std::cout << "caught exception in async handle command\n";
                    }

                }).detach();
}
