#ifndef SHARED_DUK_WORKER_STATE_HPP_INCLUDED
#define SHARED_DUK_WORKER_STATE_HPP_INCLUDED

#include <atomic>
#include <mutex>
#include <string>

///shared between the manager thread, and the executing thread
struct shared_duk_worker_state
{
    void set_realtime();
    void disable_realtime();
    bool is_realtime();

    void set_output_data(const std::string& str);
    std::string consume_output_data();
    bool has_output_data_available();

private:
    std::atomic_int is_realtime_script{0};
    std::string realtime_output_data;
    std::atomic_bool has_output_data{false};
    std::mutex lck;
};

#endif // SHARED_DUK_WORKER_STATE_HPP_INCLUDED
