#ifndef DUK_OBJECT_FUNCTIONS_HPP_INCLUDED
#define DUK_OBJECT_FUNCTIONS_HPP_INCLUDED

//#include <utility>
//#include <variant>
//#include <map>
#include <string>
#include <vector>
#include <string_view>
#include <nlohmann/json.hpp>

namespace js
{
    struct value_context;
}

int32_t get_thread_id(js::value_context& vctx);

std::string get_caller(js::value_context& vctx);

std::vector<std::string> get_caller_stack(js::value_context& vctx);

std::string get_script_host(js::value_context& vctx);

std::string get_script_ending(js::value_context& vctx);

std::string get_base_caller(js::value_context& vctx);

void dukx_setup_db_proxy(js::value_context& vctx);

#endif // DUK_OBJECT_FUNCTIONS_HPP_INCLUDED
