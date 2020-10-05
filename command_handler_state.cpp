#include "command_handler_state.hpp"
#include "command_handler.hpp"
#include "safe_thread.hpp"

std::string command_handler_state::get_auth_hex()
{
    safe_lock_guard guard(command_lock);

    return binary_to_hex(auth);
}

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

bool command_handler_state::is_authenticated()
{
    safe_lock_guard guard(command_lock);

    return auth.size() != 0;
}

void command_handler_state::set_steam_id(uint64_t psteam_id)
{
    safe_lock_guard guard(command_lock);

    steam_id = psteam_id;
}

uint64_t command_handler_state::get_steam_id()
{
    safe_lock_guard guard(command_lock);

    return steam_id;
}

void command_handler_state::set_user_name(const std::string& usr)
{
    safe_lock_guard guard(command_lock);

    current_user_name = usr;
}

std::string command_handler_state::get_user_name()
{
    safe_lock_guard guard(command_lock);

    return current_user_name;
}

void command_handler_state::set_key_state(int script_id, const std::string& str, bool is_down)
{
    if(str.size() > 10)
        return;

    std::unique_lock<shared_lock_type_t> guard(key_lock);

    {
        unprocessed_key_info key;
        key.key = str;
        unprocessed_key_input[script_id].push_back(key);

        if(unprocessed_key_input[script_id].size() > 250)
            unprocessed_key_input[script_id].clear();

        if(unprocessed_key_input.size() > 2500)
            unprocessed_key_input.clear();
    }

    ///ur cheating!!!
    if(key_states.size() > 2500)
        key_states.clear();

    if(key_states[script_id].size() > 250)
        key_states[script_id].clear();

    key_states[script_id][str] = is_down;
}

std::map<std::string, bool> command_handler_state::get_key_state(int script_id)
{
    shared_lock<shared_lock_type_t> guard(key_lock);

    auto it = key_states.find(script_id);

    if(it == key_states.end())
        return std::map<std::string, bool>();

    return it->second;
}

int command_handler_state::number_of_running_realtime_scripts()
{
    std::lock_guard guard(realtime_script_deltas_lock);

    return realtime_script_deltas_ms.size();
}

int command_handler_state::number_of_running_oneshot_scripts()
{
    return number_of_oneshot_scripts - number_of_oneshot_scripts_terminated;
}

float command_handler_state::number_of_running_realtime_work_units()
{
    std::lock_guard guard(realtime_script_deltas_lock);

    double amount = 0;

    for(auto& i : realtime_script_deltas_ms)
    {
        amount += i.second;
    }

    return amount;
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

    unique_lock<shared_lock_type_t> guard(mouse_lock);

    if(mouse_pos.size() > 250)
        mouse_pos.clear();

    if(mousewheel_state.size() > 250)
        mousewheel_state.clear();

    mouse_pos[script_id] = mpos;
    mousewheel_state[script_id] += mwheel_add;
}

vec2f command_handler_state::get_mouse_pos(int script_id)
{
    shared_lock<shared_lock_type_t> guard(mouse_lock);

    auto it = mouse_pos.find(script_id);

    if(it == mouse_pos.end())
        return {0,0};

    return it->second;
}

vec2f command_handler_state::consume_mousewheel_state(int script_id)
{
    unique_lock<shared_lock_type_t> guard(mouse_lock);

    vec2f scrollwheel = mousewheel_state[script_id];

    mousewheel_state[script_id] = {0,0};

    return scrollwheel;
}

bool command_handler_state::has_mousewheel_state(int script_id)
{
    shared_lock<shared_lock_type_t> guard(mouse_lock);

    auto it = mousewheel_state.find(script_id);

    if(it == mousewheel_state.end())
        return false;

    vec2f scrollwheel = it->second;

    return scrollwheel.x() != 0 || scrollwheel.y() != 0;
}

void command_handler_state::add_realtime_script(int script_id)
{
    std::lock_guard guard(realtime_script_deltas_lock);

    realtime_script_deltas_ms[script_id] = 1;
}

void command_handler_state::remove_realtime_script(int script_id)
{
    std::lock_guard guard(realtime_script_deltas_lock);

    realtime_script_deltas_ms.erase(script_id);
}

void command_handler_state::set_realtime_script_delta(int script_id, float work_units)
{
    std::lock_guard guard(realtime_script_deltas_lock);

    realtime_script_deltas_ms[script_id] = work_units;
}
