#ifndef UNIFIED_SCRIPTS_HPP_INCLUDED
#define UNIFIED_SCRIPTS_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <libncclient/nc_util.hpp>

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

    void make_from(item& t)
    {
        valid = t.get_prop("valid") == "1";
        parsed_source = t.get_prop("parsed_source");
        seclevel = t.get_prop_as_integer("seclevel");
        in_public = t.get_prop_as_integer("in_public");
        owner = t.get_prop_as_integer("owner");

        args = t.get_prop_as_array("args");
        params = t.get_prop_as_array("params");
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
    }
};

inline
unified_script_info unified_script_loading(int thread_id, const std::string& full_scriptname, std::string& err, shim_map_t shim_map = shim_map_t())
{
    unified_script_info ret;

    #define USE_C_SHIMS
    #ifdef USE_C_SHIMS
    ///check C hooks
    /*{
        mongo_requester req;
        req.set_prop("item_id", full_scriptname);
        req.set_prop("c_shim", 1);

        mongo_lock_proxy items_ctx = get_global_mongo_user_items_context(thread_id);

        auto found = req.fetch_from_db(items_ctx);

        if(found.size() == 1)
        {
            std::string c_shim_name = found[0].get_prop("c_shim_name");

            if(shim_map.find(c_shim_name) != shim_map.end())
            {
                ret.c_shim_name = c_shim_name;
                ret.seclevel = 4;
                ret.owner = found[0].get_prop("owner");
                ret.is_c_shim = true;
                ret.valid = true;
                ret.parsed_source = "function(context, args){\n    return \"This script is a fake shim to c++ and this source is fake, sorry <3\";\n}";

                return ret;
            }
        }
    }*/

    std::string c_shim_name = full_scriptname;

    if(shim_map.find(c_shim_name) != shim_map.end())
    {
        ret.c_shim_name = c_shim_name;
        ret.seclevel = 4;
        ret.owner = get_host_from_fullname(c_shim_name);
        ret.is_c_shim = true;
        ret.valid = true;
        ret.parsed_source = "function(context, args){\n    return \"This script is a fake shim to c++ and this source is fake, sorry <3\";\n}";

        return ret;
    }

    #endif // 0

    script_info script;

    {
        mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context(thread_id);

        //script.load_from_disk_with_db_metadata(str);
        script.name = full_scriptname;
        script.load_from_db(mongo_ctx);
    }

    if(!script.valid)
    {
        {
            std::string target = get_host_from_fullname(full_scriptname);

            user current_user;

            {
                mongo_lock_proxy mongo_ctx = get_global_mongo_user_info_context(thread_id);

                if(!current_user.load_from_db(mongo_ctx, target))
                    return unified_script_info();
            }

            mongo_lock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

            item fnd = current_user.get_loaded_callable_scriptname_item(item_ctx, full_scriptname);

            if(fnd.get_prop("valid") != "1")
            {
                err = "Invalid Script";
                return unified_script_info();
            }

            ret.make_from(fnd);

            return ret;
        }

        if(!script.valid)
        {
            err = "Script not found";
            return unified_script_info();
        }
    }

    ret.make_from(script);

    return ret;
}

#endif // UNIFIED_SCRIPTS_HPP_INCLUDED
