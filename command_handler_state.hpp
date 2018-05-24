#ifndef COMMAND_HANDLER_STATE_HPP_INCLUDED
#define COMMAND_HANDLER_STATE_HPP_INCLUDED

#include <mutex>
#include <map>
#include <string>
#include <atomic>
#include <vector>
#include "user.hpp"

namespace key_state
{
    enum key_state
    {
        UP = 0,
        DOWN = 1,
        DOWN_THEN_UP = 2,
        UP_THEN_DOWN = 3,
        COUNT,
    };
}

using key_state_t = key_state::key_state;

struct command_handler_state
{

    std::mutex command_lock;
    std::mutex lock;
    std::mutex key_lock;

    std::map<int, std::vector<std::string>> unprocessed_keystrokes;
    std::map<int, std::map<std::string, key_state_t>> key_states;

    std::atomic_bool should_terminate_any_realtime{false};
    std::atomic_int number_of_realtime_scripts{0};
    std::atomic_int number_of_realtime_scripts_terminated{0};

    std::atomic_int number_of_oneshot_scripts{0};
    std::atomic_int number_of_oneshot_scripts_terminated{0};

    std::map<int, bool> should_terminate_realtime;

    std::string get_auth();
    void set_auth(const std::string& str);

    void set_user(const user& usr);
    user get_user();

    void set_key_state(int script_id, const std::string& str, key_state_t is_down);
    std::map<std::string, key_state_t> get_key_state(int script_id);

    int number_of_running_realtime_scripts();
    int number_of_running_oneshot_scripts();

    bool has_new_width_height(int script_id);
    void set_width_height(int script_id, int width, int height);
    std::pair<int, int> consume_width_height(int script_id);

    void add_mouse_state(int script_id, vec2f mpos, vec2f mwheel_pos);
    vec2f get_mouse_pos(int script_id);

    vec2f consume_mousewheel_state(int script_id);
    bool has_mousewheel_state(int script_id);

private:
    std::string auth;
    user current_user;

    std::map<int, std::pair<int, int>> received_sizes;
    std::mutex size_lock;

    std::mutex mouse_lock;

    std::map<int, vec2f> mouse_pos;
    std::map<int, vec2f> mousewheel_state;
};

#endif // COMMAND_HANDLER_STATE_HPP_INCLUDED
