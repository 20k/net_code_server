#include "shared_duk_worker_state.hpp"
#include "safe_thread.hpp"

void shared_duk_worker_state::set_realtime()
{
    is_realtime_script = 1;
}

void shared_duk_worker_state::disable_realtime()
{
    is_realtime_script = 0;
}

bool shared_duk_worker_state::is_realtime()
{
    return is_realtime_script;
}

void shared_duk_worker_state::add_output_data(const std::string& str)
{
    safe_lock_guard guard(lck);

    realtime_output_data.push_back(str);
    has_output_data = true;
}

std::string shared_duk_worker_state::consume_output_data()
{
    safe_lock_guard guard(lck);

    if(!has_output_data)
        return "";

    if(realtime_output_data.size() == 0)
        return "";

    has_output_data = false;

    std::string f = realtime_output_data.front();

    realtime_output_data.erase(realtime_output_data.begin());

    return f;
}

bool shared_duk_worker_state::has_output_data_available()
{
    return has_output_data;
}

void shared_duk_worker_state::set_close_window_on_exit()
{
    should_close_window_on_exit = true;
}

bool shared_duk_worker_state::close_window_on_exit()
{
    return should_close_window_on_exit;
}

void shared_duk_worker_state::set_width_height(int pwidth, int pheight)
{
    safe_lock_guard guard(whguard);

    width = pwidth;
    height = pheight;
}

std::pair<int, int> shared_duk_worker_state::get_width_height()
{
    safe_lock_guard guard(whguard);

    return {width, height};
}

void shared_duk_worker_state::set_key_state(const std::map<std::string, bool>& key_state)
{
    safe_lock_guard guard(key_lock);

    ikey_state = key_state;
}

bool shared_duk_worker_state::is_key_down(const std::string& str)
{
    safe_lock_guard guard(key_lock);

    return ikey_state[str];
}

void shared_duk_worker_state::set_mouse_pos(vec2f pos)
{
    mouse_pos = pos;
}

vec2f shared_duk_worker_state::get_mouse_pos()
{
    return mouse_pos;
}
