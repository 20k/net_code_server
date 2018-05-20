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
