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

struct ui_element_state
{
    bool processed = false;
    std::string value;
};

struct realtime_ui_state
{
    std::map<std::string, ui_element_state> element_states;
};

struct realtime_script_data
{
    std::vector<unprocessed_key_info> unprocessed_text_input;
    std::vector<unprocessed_key_info> unprocessed_key_input;
    std::map<std::string, bool> key_states;
    float realtime_script_deltas_ms = 0;
    bool should_terminate_realtime = false;

    vec2f mouse_pos;
    vec2f mousewheel_state;

    std::optional<std::pair<int, int>> received_sizes;

    realtime_ui_state realtime_ui;
};

struct command_handler_state
{
    lock_type_t command_lock;

    lock_type_t script_data_lock;
    std::map<int, realtime_script_data> script_data;

    std::atomic_bool should_terminate_any_realtime{false};

    std::atomic_int number_of_oneshot_scripts{0};
    std::atomic_int number_of_oneshot_scripts_terminated{0};

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
};

#endif // COMMAND_HANDLER_STATE_HPP_INCLUDED
