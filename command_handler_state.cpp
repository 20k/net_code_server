#include "command_handler_state.hpp"

std::string command_handler_state::get_auth()
{
    safe_lock_guard guard(command_lock);

    return auth;
}

void command_handler_state::set_auth(const std::string& str)
{
    safe_lock_guard guard(command_lock);

    auth = str;
}

void command_handler_state::set_user(const user& usr)
{
    safe_lock_guard guard(command_lock);

    current_user = usr;
}

user command_handler_state::get_user()
{
    safe_lock_guard guard(command_lock);

    return current_user;
}

void command_handler_state::set_key_state(int script_id, const std::string& str, bool is_down)
{
    if(str.size() > 10)
        return;

    safe_lock_guard guard(key_lock);

    ///ur cheating!!!
    if(key_states.size() > 2500)
        key_states.clear();

    if(key_states[script_id].size() > 250)
        key_states[script_id].clear();

    key_states[script_id][str] = is_down;
}

std::map<std::string, bool> command_handler_state::get_key_state(int script_id)
{
    safe_lock_guard guard(key_lock);

    return key_states[script_id];
}

int command_handler_state::number_of_running_realtime_scripts()
{
    return number_of_realtime_scripts - number_of_realtime_scripts_terminated;
}

int command_handler_state::number_of_running_oneshot_scripts()
{
    return number_of_oneshot_scripts - number_of_oneshot_scripts_terminated;
}

bool command_handler_state::has_new_width_height(int script_id)
{
    safe_lock_guard guard(size_lock);

    return received_sizes.find(script_id) != received_sizes.end();
}

void command_handler_state::set_width_height(int script_id, int pwidth, int pheight)
{
    safe_lock_guard guard(size_lock);

    if(received_sizes.size() > 200)
        received_sizes.clear();

    pwidth = clamp(pwidth, 5, 400);
    pheight = clamp(pheight, 5, 400);

    received_sizes[script_id] = {pwidth, pheight};
}

std::pair<int, int> command_handler_state::consume_width_height(int script_id)
{
    safe_lock_guard guard(size_lock);

    auto ret = received_sizes[script_id];

    received_sizes.erase(received_sizes.find(script_id));

    return ret;
}

void command_handler_state::add_mouse_state(int script_id, vec2f mpos, vec2f mwheel_add)
{
    mwheel_add = clamp(mwheel_add, -1000.f, 1000.f);

    safe_lock_guard guard(mouse_lock);

    if(mouse_pos.size() > 250)
        mouse_pos.clear();

    if(mousewheel_state.size() > 250)
        mousewheel_state.clear();

    mouse_pos[script_id] = mpos;
    mousewheel_state[script_id] += mwheel_add;
}

vec2f command_handler_state::get_mouse_pos(int script_id)
{
    safe_lock_guard guard(mouse_lock);

    return mouse_pos[script_id];
}

vec2f command_handler_state::consume_mousewheel_state(int script_id)
{
    safe_lock_guard guard(mouse_lock);

    vec2f scrollwheel = mousewheel_state[script_id];

    mousewheel_state[script_id] = {0,0};

    return scrollwheel;
}

bool command_handler_state::has_mousewheel_state(int script_id)
{
    safe_lock_guard guard(mouse_lock);

    vec2f scrollwheel = mousewheel_state[script_id];

    return scrollwheel.x() != 0 || scrollwheel.y() != 0;
}

void command_handler_state::add_realtime_script(int script_id)
{
    std::lock_guard guard(realtime_script_deltas_lock);

    realtime_script_deltas_ms[script_id] = 16;
}

void command_handler_state::remove_realtime_script(int script_id)
{
    std::lock_guard guard(realtime_script_deltas_lock);

    realtime_script_deltas_ms.erase(script_id);
}

void command_handler_state::set_realtime_script_delta(int script_id, float delta)
{
    std::lock_guard guard(realtime_script_deltas_lock);

    realtime_script_deltas_ms[script_id] = delta;
}
