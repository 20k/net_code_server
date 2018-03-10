#ifndef SCRIPT_UTIL_HPP_INCLUDED
#define SCRIPT_UTIL_HPP_INCLUDED

#include <string_view>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <js/js_interop.hpp>

#include "script_util_shared.hpp"
#include "duk_object_functions.hpp"

inline
std::string base_scripts_string = "./scripts/";

struct mongo_lock_proxy;

using autos_t = std::vector<std::pair<std::string, std::string>>;

struct script_info
{
    std::string name;
    std::string unparsed_source;
    std::string parsed_source;
    std::string owner;

    int seclevel = 0;
    bool valid = false;
    bool in_public = false;

    //void load_from_disk_with_db_metadata(const std::string& name);

    std::string load_from_unparsed_source(duk_context* ctx, const std::string& unparsed, const std::string& name);

    bool load_from_db(mongo_lock_proxy& ctx);
    void overwrite_in_db(mongo_lock_proxy& ctx);

    bool exists_in_db(mongo_lock_proxy& ctx);
};

bool script_compiles(duk_context* ctx, script_info& script, std::string& err_out);
std::string attach_wrapper(const std::string& data_in, bool stringify, bool direct);

void register_funcs(duk_context* ctx, int seclevel);

struct script_data
{
    std::string parsed_source;
    int seclevel = 0;
    bool valid = false;
};

script_data parse_script(std::string in);

///#db.f({[col_key]: {$exists : true}});
///$where and $query both need to be disabled, $inspect as well

inline
void set_script_info(duk_context* ctx, const std::string& full_script_name)
{
    std::vector<std::string> strings = no_ss_split(full_script_name, ".");

    //assert(strings.size() == 2);

    set_global_string(ctx, "script_host", strings[0]);
    set_global_string(ctx, "script_ending", strings[1]);
}

#endif // SCRIPT_UTIL_HPP_INCLUDED
