#ifndef SCRIPT_UTIL_HPP_INCLUDED
#define SCRIPT_UTIL_HPP_INCLUDED

#include <string_view>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <js/js_interop.hpp>

#include "script_util_shared.hpp"

inline
std::string base_scripts_string = "./scripts/";


inline
bool expand_to_from_scriptname(std::string_view& view, std::string& in, int& offset, std::string from, std::string to)
{
    std::string srch = from;

    if(view.substr(0, srch.size()) != srch)
        return false;

    //std::cout << "expand1\n";

    std::string found = "";
    int found_loc = -1;

    for(int i=srch.length(); i < (int)view.size(); i++)
    {
        char c = view[i];

        ///to disable extended syntax of
        ///var x = #fs.i20k.whatever; x(), remove the semicolon
        if(c == '(' || c == ';')
        {
            found_loc = i;
            found = std::string(view.begin() + srch.length(), view.begin() + i);
            break;
        }
    }

    /*if(found_loc != -1)
    {
        std::cout << "fnd " << found << std::endl;
    }*/

    bool valid = is_valid_full_name_string(found);

    //std::cout << found << "\n fnd\n";

    if(valid)
    {
        int start = offset;
        int finish = offset + found_loc;

        in.replace(offset, finish - start, to + "(\"" + found + "\")");
    }

    ///increase offset?

    return valid;
}

inline
bool expand_to_from_nochecks(std::string_view& view, std::string& in, int& offset, std::string from, std::string to)
{
    std::string srch = from;

    if(view.substr(0, srch.size()) != srch)
        return false;

    //std::cout << "expand2\n";

    std::string found = "";
    int found_loc = -1;

    for(int i=srch.length(); i < (int)view.size(); i++)
    {
        char c = view[i];

        if(c == '(')
        {
            found_loc = i;
            found = std::string(view.begin() + srch.length(), view.begin() + i);
            break;
        }
    }

    int start = offset;
    int finish = offset + found_loc;

    in.replace(offset, finish - start, to);

    ///increase offset?

    return true;
}

inline
bool expand(std::string_view& view, std::string& in, int& offset, int& found_seclevel)
{
    std::vector<std::string> froms{"#fs.", "#hs.", "#ms.", "#ls.", "#ns.",
                                   "#4s.", "#3s.", "#2s.", "#1s.", "#0s.",
                                   "#s."};

    std::vector<std::string> tos  {"fs_call", "hs_call", "ms_call", "ls_call", "ns_call",
                                   "fs_call", "hs_call", "ms_call", "ls_call", "ns_call",
                                   "ns_call"};

    std::vector<int> sec_levels = {4, 3, 2, 1, 0,
                                   4, 3, 2, 1, 0,
                                   0};

    for(int i=0; i < (int)tos.size(); i++)
    {
        bool success = expand_to_from_scriptname(view, in, offset, froms[i], tos[i]);

        if(success)
        {
            found_seclevel = std::min(found_seclevel, sec_levels[i]);

            return true;
        }
    }

    std::vector<std::string> froms_unchecked{"#D",
                                             "#db.i", "#db.r", "#db.f", "#db.u", "#db.u1", "#db.us"};

    std::vector<std::string> tos_unchecked  {"hash_d",
                                             "db_insert", "db_remove", "db_find", "db_update", "db_update1", "db_upsert"};

    for(int i=0; i < (int)tos_unchecked.size(); i++)
    {
        bool success = expand_to_from_nochecks(view, in, offset, froms_unchecked[i], tos_unchecked[i]);

        if(success)
            return true;
    }

    return false;
}

struct mongo_lock_proxy;

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

    void load_from_db(mongo_lock_proxy& ctx);
    void overwrite_in_db(mongo_lock_proxy& ctx);

    bool exists_in_db(mongo_lock_proxy& ctx);
};

inline
std::string get_hash_d(duk_context* ctx)
{
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "HASH_D");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

inline
bool script_compiles(duk_context* ctx, script_info& script, std::string& err_out)
{
    std::string prologue = "function INTERNAL_TEST(context, args)\n{'use strict'\nvar IVAR = ";
    std::string endlogue = "\n\nreturn IVAR(context, args);\n\n}\n";

    std::string wrapper = prologue + script.parsed_source + endlogue;

    duk_push_string(ctx, wrapper.c_str());
    duk_push_string(ctx, "test-name");

    if(duk_pcompile(ctx, DUK_COMPILE_FUNCTION | DUK_COMPILE_STRICT) != 0)
    {
        std::string ret = duk_safe_to_string(ctx, -1);

        err_out = ret;

        printf("scompile failed: %s\n", ret.c_str());

        #ifdef DEBUG_SOURCE
        std::cout << script.parsed_source << std::endl;
        #endif // DEBUG_SOURCE


        duk_pop(ctx);

        return false;
    }
    else
    {
        duk_pop(ctx);

        return true;
    }

    err_out = "";
}

inline
std::string attach_wrapper(const std::string& data_in, bool stringify, bool direct)
{
    std::string prologue = "function INTERNAL_TEST(context, args)\n{'use strict'\nvar IVAR = ";
    std::string endlogue = "\n\nreturn IVAR(context, args);\n\n}\n";

    if(stringify)
    {
        endlogue = "\n\nreturn JSON.stringify(IVAR(context, args));\n\n}\n";
    }

    if(direct && stringify)
    {
        endlogue = "\n\nreturn JSON.stringify(IVAR);\n\n}\n";
    }

    if(direct && !stringify)
    {
        endlogue = "\n\n return IVAR }";
    }

    return prologue + data_in + endlogue;
}

void register_funcs(duk_context* ctx, int seclevel);

///#db.f({[col_key]: {$exists : true}});
///$where and $query both need to be disabled, $inspect as well
inline
std::string compile_and_call(stack_duk& sd, const std::string& data, bool called_internally, std::string caller, bool is_conargs_function, bool stringify, int seclevel)
{
    if(data.size() == 0)
    {
        return "Script not found";
    }

    register_funcs(sd.ctx, seclevel);

    std::string wrapper = attach_wrapper(data, !called_internally || stringify, false);

    //std::cout << wrapper << std::endl;

    std::string ret;

    duk_push_string(sd.ctx, wrapper.c_str());
    duk_push_string(sd.ctx, "test-name");

    //bool success = false;

    //DUK_COMPILE_FUNCTION
    if(duk_pcompile(sd.ctx, DUK_COMPILE_FUNCTION | DUK_COMPILE_STRICT) != 0)
    {
        ret = duk_safe_to_string(sd.ctx, -1);

        printf("compile failed: %s\n", ret.c_str());

        if(called_internally)
            duk_push_undefined(sd.ctx);

        //success = false;
    }
    else
    {
        ///need to push caller, and then args
        if(is_conargs_function)
        {
            duk_push_global_stash(sd.ctx);
            duk_push_int(sd.ctx, seclevel);
            duk_put_prop_string(sd.ctx, -2, "last_seclevel");
            duk_pop_n(sd.ctx, 1);

            duk_push_global_object(sd.ctx); //[glob]

            duk_idx_t id = duk_push_object(sd.ctx); ///context //[glob -> obj]
            duk_push_string(sd.ctx, caller.c_str()); ///caller //[glob -> obj -> string]
            duk_put_prop_string(sd.ctx, id, "caller"); //[glob -> obj]

            duk_put_prop_string(sd.ctx, -2, "context"); //[glob]

            duk_pop_n(sd.ctx, 1); //empty stack, has function at -1

            duk_get_global_string(sd.ctx, "context"); //[context]
            //duk_push_object(sd.ctx); ///push empty args, no forwarding*/

            /*duk_push_object(sd.ctx); ///context object

            duk_push_string(sd.ctx, caller.c_str());
            duk_put_prop_string(sd.ctx, -2, "caller");*/

            int nargs = 2;

            if(called_internally)
            {
                if(duk_is_undefined(sd.ctx, -3))
                {
                    nargs = 1;
                }
                else
                {
                    duk_dup(sd.ctx, -3); //[args]
                }
            }
            else
            {
                duk_push_undefined(sd.ctx);

                //duk_push_object(sd.ctx);
            }

            duk_pcall(sd.ctx, nargs);
        }
        else
        {
            duk_pcall(sd.ctx, 0);
        }

        if(!called_internally)
        {
            ret = duk_safe_to_string(sd.ctx, -1);
            //printf("program result: %s\n", ret.c_str());
        }

        //success = true;
    }

    std::string str = get_hash_d(sd.ctx);

    ///only should do this if the caller is owner of script
    if(str != "")
    {
        ret = str;
    }

    if(!called_internally)
        duk_pop(sd.ctx);

    return ret;
}

inline
std::string get_global_string(duk_context* ctx, const std::string& name)
{
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, name.c_str());

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

inline
int32_t get_global_int(duk_context* ctx, const std::string& name)
{
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, name.c_str());

    int32_t ret = duk_get_int(ctx, -1);

    duk_pop_n(ctx, 2);

    return ret;
}

inline
void set_global_int(duk_context* ctx, const std::string& name, int32_t val)
{
    duk_push_global_stash(ctx);

    duk_push_int(ctx, val);
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop_n(ctx, 1);
}

inline
void set_global_string(duk_context* ctx, const std::string& name, const std::string& val)
{
    duk_push_global_stash(ctx);

    duk_push_string(ctx, val.c_str());
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop_n(ctx, 1);
}

inline
std::string get_caller(duk_context* ctx)
{
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "caller");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

inline
std::string get_script_host(duk_context* ctx)
{
    return get_global_string(ctx, "script_host");
}

inline
std::string get_script_ending(duk_context* ctx)
{
    return get_global_string(ctx, "script_ending");
}

inline
void set_script_info(duk_context* ctx, const std::string& full_script_name)
{
    std::vector<std::string> strings = no_ss_split(full_script_name, ".");

    //assert(strings.size() == 2);

    set_global_string(ctx, "script_host", strings[0]);
    set_global_string(ctx, "script_ending", strings[1]);
}

#endif // SCRIPT_UTIL_HPP_INCLUDED
