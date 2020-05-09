#ifndef COMMAND_HANDLER_STATE_HPP_INCLUDED
#define COMMAND_HANDLER_STATE_HPP_INCLUDED

#include <mutex>
#include <shared_mutex>
#include <map>
#include <string>
#include <atomic>
#include <vector>
#include "user.hpp"
#include "safe_thread.hpp"

struct unprocessed_key_info
{
    std::string key;
    bool is_repeat = false;
};

struct command_handler_state
{
    lock_type_t command_lock;
    lock_type_t lock;
    shared_lock_type_t key_lock;

    std::map<int, std::vector<unprocessed_key_info>> unprocessed_keystrokes;
    std::map<int, std::map<std::string, bool>> key_states;

    std::atomic_bool should_terminate_any_realtime{false};

    lock_type_t realtime_script_deltas_lock;
    std::map<int, float> realtime_script_deltas_ms;

    std::atomic_int number_of_oneshot_scripts{0};
    std::atomic_int number_of_oneshot_scripts_terminated{0};

    std::map<int, bool> should_terminate_realtime;

    std::string get_auth_hex();
    std::string get_auth();
    void set_auth(const std::string& str);

    bool is_authenticated();

    void set_steam_id(uint64_t psteam_id);
    uint64_t get_steam_id();

    void set_user_name(const std::string& usr);
    std::string get_user_name();

    void set_key_state(int script_id, const std::string& str, bool is_down);
    std::map<std::string, bool> get_key_state(int script_id);

    int number_of_running_realtime_scripts();
    int number_of_running_oneshot_scripts();
    float number_of_running_realtime_work_units();

    bool has_new_width_height(int script_id);
    void set_width_height(int script_id, int width, int height);
    std::pair<int, int> consume_width_height(int script_id);

    void add_mouse_state(int script_id, vec2f mpos, vec2f mwheel_pos);
    vec2f get_mouse_pos(int script_id);

    vec2f consume_mousewheel_state(int script_id);
    bool has_mousewheel_state(int script_id);

    void add_realtime_script(int script_id);
    void remove_realtime_script(int script_id);
    void set_realtime_script_delta(int script_id, float work_units);

private:
    std::string auth;
    uint64_t steam_id = 0;
    //user current_user;
    std::string current_user_name;

    std::map<int, std::pair<int, int>> received_sizes;
    lock_type_t size_lock;

    shared_lock_type_t mouse_lock;

    std::map<int, vec2f> mouse_pos;
    std::map<int, vec2f> mousewheel_state;
};

#endif // COMMAND_HANDLER_STATE_HPP_INCLUDED
