#ifndef SCRIPT_UTIL_HPP_INCLUDED
#define SCRIPT_UTIL_HPP_INCLUDED

#include <string_view>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <js/ui_util.hpp>
#include <js/js_interop.hpp>

inline
std::string base_scripts_string = "./scripts";

inline
bool is_valid_name_character(char c)
{
    return isalnum(c) || c == '_';
}

///i think something is broken with 7.2s stringstream implementation
///i dont know why the stringstream version crashes
inline
std::vector<std::string> no_ss_split(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

inline
bool is_valid_string(const std::string& to_parse)
{
    if(to_parse.size() >= 15)
        return false;

    if(to_parse.size() == 0)
        return false;

    bool check_digit = true;

    for(char c : to_parse)
    {
        if(check_digit && isdigit(c))
        {
            return false;
        }

        check_digit = false;

        if(!is_valid_name_character(c))
        {
            return false;
        }
    }

    return true;
}

inline
bool is_valid_full_name_string(const std::string& name)
{
    //std::string to_parse = strip_whitespace(name);

    std::string to_parse = name;

    int num_dots = std::count(to_parse.begin(), to_parse.end(), '.');

    if(num_dots != 1)
    {
        return false;
    }

    std::vector<std::string> strings = no_ss_split(to_parse, ".");

    if(strings.size() != 2)
        return false;

    for(auto& str : strings)
    {
        if(!is_valid_string(str))
            return false;
    }

    return true;
}

inline
std::string get_script_from_name_string(const std::string& base_dir, const std::string& name_string)
{
    bool is_valid = is_valid_full_name_string(name_string);

    if(!is_valid)
        return "";

    std::string to_parse = strip_whitespace(name_string);

    std::replace(to_parse.begin(), to_parse.end(), '.', '/');

    std::string file = base_dir + "/" + to_parse + ".js";

    if(!file_exists(file))
    {
        return "";
    }

    return read_file(file);
}

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

struct script_info
{
    std::string data;

    int seclevel = 0;

    void load(const std::string& name);
};

inline
script_info parse_script(std::string in)
{
    if(in.size() == 0)
        return script_info();

    int found_seclevel = 4;

    for(int i=0; i < (int)in.size(); i++)
    {
        std::string_view strview(&in[i]);

        expand(strview, in, i, found_seclevel);
    }

    script_info script;
    script.data = in;
    script.seclevel = found_seclevel;

    return script;
}

inline
std::string get_hash_d(duk_context* ctx)
{
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "HASH_D");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

///#db.f({[col_key]: {$exists : true}});
///$where and $query both need to be disabled, $inspect as well
inline
std::string compile_and_call(stack_duk& sd, const std::string& data, bool called_internally, std::string caller, bool is_conargs_function = true)
{
    if(data.size() == 0)
    {
        return "Script not found";
    }

    std::string prologue = "function INTERNAL_TEST(context, args)\n{'use strict'\nvar IVAR = ";
    std::string endlogue = "\n\nreturn IVAR(context, args);\n\n}\n";

    if(!called_internally)
    {
        endlogue = "\n\nreturn JSON.stringify(IVAR(context, args));\n\n}\n";
    }

    std::string wrapper = prologue + data + endlogue;

    std::string ret;

    duk_push_string(sd.ctx, wrapper.c_str());
    duk_push_string(sd.ctx, "test-name");

    bool success = false;

    //DUK_COMPILE_FUNCTION
    if(duk_pcompile(sd.ctx, DUK_COMPILE_FUNCTION | DUK_COMPILE_STRICT) != 0)
    {
        ret = duk_safe_to_string(sd.ctx, -1);

        printf("compile failed: %s\n", ret.c_str());

        success = false;
    }
    else
    {
        ///need to push caller, and then args
        if(is_conargs_function)
        {
            duk_push_global_object(sd.ctx); //[glob]

            duk_idx_t id = duk_push_object(sd.ctx); ///context //[glob -> obj]
            duk_push_string(sd.ctx, caller.c_str()); ///caller //[glob -> obj -> string]
            duk_put_prop_string(sd.ctx, id, "caller"); //[glob -> obj]

            duk_put_prop_string(sd.ctx, -2, "context"); //[glob]

            duk_pop_n(sd.ctx, 1); //empty stack, has function at -1

            duk_get_global_string(sd.ctx, "context"); //[context]
            //duk_push_object(sd.ctx); ///push empty args, no forwarding

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

        success = true;
    }

    std::string str = get_hash_d(sd.ctx);

    ///only should do this if the caller is owner of script
    if(str != "")
    {
        ret = str;
    }

    if(!called_internally || !success)
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

#endif // SCRIPT_UTIL_HPP_INCLUDED
