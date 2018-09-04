#ifndef UNIFIED_SCRIPTS_HPP_INCLUDED
#define UNIFIED_SCRIPTS_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <libncclient/nc_util.hpp>
#include "item.hpp"
#include "script_util.hpp"
#include "user.hpp"

inline std::map<std::string, duk_ret_t (*)(duk_context*)> c_shim_map;
inline std::mutex shim_lock;

using shim_map_t = std::map<std::string, duk_ret_t (*)(duk_context*)>;

struct unified_script_info
{
    bool valid = false;
    std::string parsed_source;
    int seclevel = 0;
    bool in_public = false;
    std::string owner;

    std::vector<std::string> args;
    std::vector<std::string> params;

    bool is_c_shim = false;
    std::string c_shim_name;

    script_metadata metadata;

    void make_from(item& t)
    {
        valid = t.get_prop("valid") == "1";
        parsed_source = t.get_prop("parsed_source");
        seclevel = t.get_prop_as_integer("seclevel");
        in_public = t.get_prop_as_integer("in_public");
        owner = t.get_prop_as_integer("owner");

        args = t.get_prop_as_array("args");
        params = t.get_prop_as_array("params");
        metadata.load_from_string(t.get_prop("metadata"));
    }

    void make_from(script_info& sinfo)
    {
        valid = sinfo.valid;
        parsed_source = sinfo.parsed_source;
        seclevel = sinfo.seclevel;
        in_public = sinfo.in_public;
        owner = sinfo.owner;

        args = sinfo.args;
        params = sinfo.params;
        metadata = sinfo.metadata;
    }
};

unified_script_info unified_script_loading(int thread_id, const std::string& full_scriptname, std::string& err, shim_map_t shim_map = shim_map_t());

#endif // UNIFIED_SCRIPTS_HPP_INCLUDED
