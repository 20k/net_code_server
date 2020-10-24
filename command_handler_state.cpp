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

    std::lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    if(is_down)
    {
        unprocessed_key_info key;
        key.key = str;

        dat.unprocessed_key_input.push_back(key);

        if(dat.unprocessed_key_input.size() > 250)
            dat.unprocessed_key_input.clear();

        if(dat.unprocessed_key_input.size() > 2500)
            dat.unprocessed_key_input.clear();
    }

    if(dat.key_states.size() > 250)
        dat.key_states.clear();

    dat.key_states[str] = is_down;
}

std::map<std::string, bool> command_handler_state::get_key_state(int script_id)
{
    std::lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    return dat.key_states;
}

int command_handler_state::number_of_running_realtime_scripts()
{
    std::lock_guard guard(script_data_lock);

    return script_data.size();
}

int command_handler_state::number_of_running_oneshot_scripts()
{
    return number_of_oneshot_scripts - number_of_oneshot_scripts_terminated;
}

float command_handler_state::number_of_running_realtime_work_units()
{
    std::lock_guard guard(script_data_lock);

    double amount = 0;

    for(auto& i : script_data)
    {
        amount += i.second.realtime_script_deltas_ms;
    }

    return amount;
}

bool command_handler_state::has_new_width_height(int script_id)
{
    safe_lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    return dat.received_sizes.has_value();
}

void command_handler_state::set_width_height(int script_id, int pwidth, int pheight)
{
    safe_lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    pwidth = clamp(pwidth, 5, 400);
    pheight = clamp(pheight, 5, 400);

    dat.received_sizes = {pwidth, pheight};
}

std::pair<int, int> command_handler_state::consume_width_height(int script_id)
{
    safe_lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    auto val = dat.received_sizes.value_or(std::pair<int, int>{0, 0});

    dat.received_sizes = std::nullopt;

    return val;
}

void command_handler_state::add_mouse_state(int script_id, vec2f mpos, vec2f mwheel_add)
{
    mwheel_add = clamp(mwheel_add, -1000.f, 1000.f);

    safe_lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    dat.mouse_pos = mpos;
    dat.mousewheel_state += mwheel_add;
}

vec2f command_handler_state::get_mouse_pos(int script_id)
{
    safe_lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    return dat.mouse_pos;
}

vec2f command_handler_state::consume_mousewheel_state(int script_id)
{
    safe_lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    vec2f scrollwheel = dat.mousewheel_state;

    dat.mousewheel_state = {0,0};

    return scrollwheel;
}

bool command_handler_state::has_mousewheel_state(int script_id)
{
    safe_lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    vec2f scrollwheel = dat.mousewheel_state;

    return scrollwheel.x() != 0 || scrollwheel.y() != 0;
}

void command_handler_state::add_realtime_script(int script_id)
{
    std::lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    dat.realtime_script_deltas_ms = 1;
}

void command_handler_state::remove_realtime_script(int script_id)
{
    std::lock_guard guard(script_data_lock);

    auto it = script_data.find(script_id);

    if(it != script_data.end())
    {
        script_data.erase(it);
    }
}

void command_handler_state::set_realtime_script_delta(int script_id, float work_units)
{
    std::lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];

    dat.realtime_script_deltas_ms = work_units;
}

/*void command_handler_state::set_client_sequence_id(int script_id, int seq_id)
{
    std::lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];
    dat.client_seq_id = seq_id;
}

int command_handler_state::get_client_sequence_id(int script_id)
{
    std::lock_guard guard(script_data_lock);

    realtime_script_data& dat = script_data[script_id];
    return dat.client_seq_id;
}*/
