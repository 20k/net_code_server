#include "unified_scripts.hpp"
#include "privileged_core_scripts.hpp"
#include <secret/special_user_scripts.hpp>

unified_script_info unified_script_loading(int thread_id, const std::string& full_scriptname, std::string& err)
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

        ret.name = full_scriptname;

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

        auto it2 = privileged_metadata.find(full_scriptname);

        if(it2 != privileged_metadata.end())
        {
            ret.metadata = it2->second;
        }

        ret.type = unified_script_info::script_type::PRIVILEGED;

        return ret;
    }

    std::string c_shim_name = full_scriptname;

    if(special_scripts::has_special_user_function(c_shim_name))
    {
        ret.c_shim_name = c_shim_name;
        ret.seclevel = 4;
        ret.owner = get_host_from_fullname(c_shim_name);
        ret.is_c_shim = true;
        ret.valid = true;
        ret.parsed_source = "function(context, args){\n    return \"This script is a fake shim to c++ and this source is fake, sorry <3\";\n}";
        ret.name = c_shim_name;
        ret.type = unified_script_info::script_type::C_SHIM;

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

            if(!fnd.has("valid") || (int)fnd.get("valid") != 1)
            {
                err = "Invalid Script";
                return unified_script_info();
            }

            ret.make_from(fnd, full_scriptname);

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

///colourise me
std::string slevel_to_str(int sl)
{
    if(sl == 4)
        return "Fullsec";
    if(sl == 3)
        return "Highsec";
    if(sl == 2)
        return "Midsec";
    if(sl == 1)
        return "Lowsec";
    if(sl == 0)
        return "Nullsec";

    return "Nullsec";
}

std::string slevel_to_short(int sl)
{
    if(sl == 4)
        return "fs";
    if(sl == 3)
        return "hs";
    if(sl == 2)
        return "ms";
    if(sl == 1)
        return "ls";
    if(sl == 0)
        return "ns";

    return "ns";
}

std::string unified_script_info::get_autogenerated_documentation(bool with_examples)
{
    std::string ret;

    if(!valid)
        return "Invalid Script";

    std::string fname = "#" + name + "() -> ";

    if(metadata.description.size() > 0)
    {
        fname += metadata.description + ". ";
    }

    std::string slevel = colour_string(slevel_to_str(seclevel));

    fname += slevel;

    if(metadata.requires_breach)
    {
        fname += ". Target must be " + make_error_col("breached");
    }

    if(metadata.param_data.size() > 0)
    {
        fname += "\n    Args:\n";

        for(arg_metadata& arg : metadata.param_data)
        {
            fname += "        ";
            fname += make_key_col(arg.key_name) + " -> " + arg.val_text;

            if((arg.type & arg_metadata::OPT) > 0)
                fname += ". " + make_success_col("<optional>");

            fname += "\n";
        }
    }

    if(fname.size() > 0 && fname.back() == '\n')
    {
        fname.pop_back();
    }

    if(with_examples)
    {
        fname += "\n    Example:\n        #" + slevel_to_short(seclevel) + "." + name + "({";



        //for(arg_metadata& arg : metadata.param_data)
        for(int i=0; i < (int)metadata.param_data.size(); i++)
        {
            if((metadata.param_data[i].type & arg_metadata::OPT) > 0)
                continue;

            std::string ex_str = metadata.param_data[i].key_name + ":" + metadata.param_data[i].get_example();

            //if(i != (int)metadata.param_data.size()-1)
                ex_str += ", ";

            fname += ex_str;
        }

        if(fname.size() > 2 && fname.back() == ' ' && fname[(int)fname.size()-2] == ',')
        {
            fname.pop_back();
            fname.pop_back();
        }


        fname += "})";
    }

    if(fname.size() > 0 && fname.back() == '\n')
    {
        fname.pop_back();
    }

    return fname;
}
