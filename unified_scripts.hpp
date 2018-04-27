#ifndef UNIFIED_SCRIPTS_HPP_INCLUDED
#define UNIFIED_SCRIPTS_HPP_INCLUDED

#include <string>
#include <vector>

struct unified_script_info
{
    bool valid = false;
    std::string parsed_source;
    int seclevel = 0;
    bool in_public = false;
    std::string owner;

    std::vector<std::string> args;
    std::vector<std::string> params;

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
unified_script_info unified_script_loading(const std::string& caller, int thread_id, const std::string& full_scriptname, std::string& err)
{
    unified_script_info ret;

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
