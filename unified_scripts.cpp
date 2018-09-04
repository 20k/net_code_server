#include "unified_scripts.hpp"
#include "privileged_core_scripts.hpp"

unified_script_info unified_script_loading(int thread_id, const std::string& full_scriptname, std::string& err, shim_map_t shim_map)
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

    if(privileged_functions.find(full_scriptname) != privileged_functions.end())
    {
        ret.c_shim_name = full_scriptname;
        ret.seclevel = privileged_functions[full_scriptname].sec_level;
        ret.owner = get_host_from_fullname(full_scriptname);
        ret.is_c_shim = false;
        ret.valid = true;
        ret.parsed_source = "function(context, args){\n    return \"This script is a fake shim to c++ and this source is fake, sorry <3\";\n}";
        ret.in_public = true;

        auto it = privileged_args.find(full_scriptname);

        if(it != privileged_args.end())
        {
            std::vector<script_arg> args = it->second;

            for(script_arg& arg : args)
            {
                ret.args.push_back(arg.key);
                ret.params.push_back(arg.val);
            }
        }

        return ret;
    }

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
        mongo_nolock_proxy mongo_ctx = get_global_mongo_user_items_context(thread_id);

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
                mongo_nolock_proxy mongo_ctx = get_global_mongo_user_info_context(thread_id);

                if(!current_user.load_from_db(mongo_ctx, target))
                    return unified_script_info();
            }

            mongo_nolock_proxy item_ctx = get_global_mongo_user_items_context(thread_id);

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

