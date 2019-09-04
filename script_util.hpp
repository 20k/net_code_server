#ifndef SCRIPT_UTIL_HPP_INCLUDED
#define SCRIPT_UTIL_HPP_INCLUDED

#include <string_view>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include "scripting_api_fwrd.hpp"
#include "script_metadata.hpp"

//#include <libncclient/nc_util.hpp>
//#include "duk_object_functions.hpp"

inline
std::string base_scripts_string = "./scripts/";

struct mongo_lock_proxy;

using autos_t = std::vector<std::pair<std::string, std::string>>;

struct item;

struct script_info
{
    std::string name;
    std::string unparsed_source;
    std::string parsed_source;
    std::string owner;
    std::vector<std::string> args;
    std::vector<std::string> params;

    int seclevel = 0;
    int valid = false;
    int in_public = false;

    //void load_from_disk_with_db_metadata(const std::string& name);

    script_metadata metadata;

    ///typescript support is heavy, so disable for cli invocation
    std::string load_from_unparsed_source(duk_context* ctx, const std::string& unparsed, const std::string& name, bool enable_typescript, bool is_cli);

    bool load_from_db(mongo_lock_proxy& ctx);
    void overwrite_in_db(mongo_lock_proxy& ctx);

    bool exists_in_db(mongo_lock_proxy& ctx);

    void fill_as_bundle_compatible_item(item& i);
};

bool script_compiles(duk_context* ctx, script_info& script, std::string& err_out);
std::string attach_cli_wrapper(const std::string& data_in);
std::string attach_wrapper(const std::string& data_in, bool stringify, bool direct);
std::string attach_unparsed_wrapper(std::string str);

struct script_data
{
    autos_t autocompletes;
    std::string parsed_source;
    int seclevel = 0;
    int valid = false;

    std::string compile_error;
};

script_data parse_script(const std::string& file_name, std::string in, bool enable_typescript);

///#db.f({[col_key]: {$exists : true}});
///$where and $query both need to be disabled, $inspect as well

void set_script_info(duk_context* ctx, const std::string& full_script_name);

#endif // SCRIPT_UTIL_HPP_INCLUDED
