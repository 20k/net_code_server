#ifndef COMMAND_HANDLER_HPP_INCLUDED
#define COMMAND_HANDLER_HPP_INCLUDED

#include <string>
#include <optional>

#include "user.hpp"
#include <js/js_interop.hpp>
#include "seccallers.hpp"

struct shared_data;
struct command_handler_state;
struct global_state;

inline
void init_js_interop(stack_duk& sd, const std::string& js_data)
{
    sd.ctx = js_interop_startup();
}

///shared queue used for async responses from servers
std::string run_in_user_context(const std::string& username, const std::string& command, std::optional<shared_data*> shared_queue, std::optional<command_handler_state*> state);
void throwaway_user_thread(const std::string& username, const std::string& command);

struct command_handler_state
{
    std::mutex command_lock;
    std::mutex lock;
    std::mutex key_lock;

    std::map<int, std::vector<std::string>> unprocessed_keystrokes;
    std::map<std::string, bool> key_states;

    std::atomic_bool should_terminate_any_realtime{false};
    std::atomic_int number_of_realtime_scripts{0};
    std::atomic_int number_of_realtime_scripts_terminated{0};

    std::map<int, bool> should_terminate_realtime;

    std::string get_auth()
    {
        std::lock_guard guard(command_lock);

        return auth;
    }

    void set_auth(const std::string& str)
    {
        std::lock_guard guard(command_lock);

        auth = str;
    }

    void set_user(const user& usr)
    {
        std::lock_guard guard(command_lock);

        current_user = usr;
    }

    user get_user()
    {
        std::lock_guard guard(command_lock);

        return current_user;
    }

    void set_key_state(const std::string& str, bool is_down)
    {
        if(str.size() > 10)
            return;

        std::lock_guard guard(key_lock);

        key_states[str] = is_down;
    }

private:
    std::string auth;
    user current_user;
};

///context?
std::string handle_command(command_handler_state& state, const std::string& str, global_state& glob, int64_t my_id, shared_data& shared);

std::string handle_autocompletes_json(const std::string& username, const std::string& in);

std::string binary_to_hex(const std::string& in, bool swap_endianness = false);
std::string hex_to_binary(const std::string& in);
std::string delete_user(command_handler_state& state, const std::string& str, bool cli_force = false);
std::string rename_user_force(const std::string& from_name, const std::string& to_name);

#endif // COMMAND_HANDLER_HPP_INCLUDED
