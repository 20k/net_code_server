#include "shared_duk_worker_state.hpp"

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

void shared_duk_worker_state::set_output_data(const std::string& str)
{
    std::lock_guard guard(lck);

    if(realtime_output_data == str)
        return;

    realtime_output_data = str;
    has_output_data = true;
}

std::string shared_duk_worker_state::consume_output_data()
{
    std::lock_guard guard(lck);

    if(!has_output_data)
        return "";

    has_output_data = false;

    std::string ret = realtime_output_data;

    return ret;
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
    std::lock_guard guard(whguard);

    width = pwidth;
    height = pheight;
}

std::pair<int, int> shared_duk_worker_state::get_width_height()
{
    std::lock_guard guard(whguard);

    return {width, height};
}

void shared_duk_worker_state::set_key_state(const std::map<std::string, bool>& key_state)
{
    std::lock_guard guard(key_lock);

    ikey_state = key_state;
}

bool shared_duk_worker_state::is_key_down(const std::string& str)
{
    std::lock_guard guard(key_lock);

    return ikey_state[str];
}
