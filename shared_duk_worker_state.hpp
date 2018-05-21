#ifndef SHARED_DUK_WORKER_STATE_HPP_INCLUDED
#define SHARED_DUK_WORKER_STATE_HPP_INCLUDED

#include <atomic>
#include <mutex>
#include <string>
#include <map>

///shared between the manager thread, and the executing thread
struct shared_duk_worker_state
{
    void set_realtime();
    void disable_realtime();
    bool is_realtime();

    void set_output_data(const std::string& str);
    std::string consume_output_data();
    bool has_output_data_available();

    void set_close_window_on_exit();
    bool close_window_on_exit();

    void set_width_height(int width, int height);
    std::pair<int, int> get_width_height();

    void set_key_state(const std::map<std::string, bool>& key_state);
    bool is_key_down(const std::string& str);

private:
    std::atomic_int is_realtime_script{0};
    std::string realtime_output_data;
    std::atomic_bool has_output_data{false};
    std::mutex lck;
    std::mutex whguard;
    std::atomic_bool should_close_window_on_exit{false};

    int width = 10;
    int height = 10;

    std::map<std::string, bool> ikey_state;
    std::mutex key_lock;
};

#endif // SHARED_DUK_WORKER_STATE_HPP_INCLUDED
