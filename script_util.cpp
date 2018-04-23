#include "script_util.hpp"
#include <assert.h>
#include "item.hpp"
#include <libncclient/nc_util.hpp>
#include "duk_object_functions.hpp"
#include <memory>
#include "logging.hpp"

///new api
///we need a function to upload it to the server
///since we're the server, we need a function to accept a string which is the script
///check that its valid
///then stash it in the db with metadata

///then, when we want to load, we look from the db

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
        err_out = "";

        duk_pop(ctx);

        return true;
    }
}

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

bool string_is_in(const std::string& str, const std::vector<std::string>& in)
{
    for(auto& i : in)
    {
        if(i == str)
            return true;
    }

    return false;
}

bool expand_to_from_scriptname(std::string_view& view, std::string& in, int& offset, const std::string& from, const std::string& to, const std::vector<std::string>& parse_exclusion)
{
    std::string srch = from;

    if(view.size() < srch.size())
        return false;

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
            if(in[i + offset] == ';')
                in[i + offset] = ' ';

            found_loc = i;
            found = std::string(view.begin() + srch.length(), view.begin() + i);

            if(string_is_in(found, parse_exclusion))
            {
                found = "";
                found_loc = -1;
                break;
            }

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

bool expand_to_from_nochecks(std::string_view& view, std::string& in, int& offset, const std::string& from, const std::string& to)
{
    std::string srch = from;

    if(view.size() < srch.size())
        return false;

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

void get_autocompletes(std::string_view& view, std::string& in, int& offset, autos_t& autos)
{
    std::string srch = "#autos(";

    if(!starts_with(view, "#autos("))
        return;

    int start = offset;

    int cur_idx = offset;
    cur_idx += srch.size();

    while(cur_idx < (int)in.size() && in[cur_idx] == ' ')
        cur_idx++;

    auto fit = in.find(");");

    if(fit == std::string::npos)
        return;

    std::string found;

    for(; cur_idx != (int)fit; cur_idx++)
    {
        found += in[cur_idx];

        if(in[cur_idx] == '\n')
            return;
    }

    for(int i = start; i < cur_idx + 2; i++)
    {
        in[i] = ' ';
    }

    //std::cout << "fnd " << found << std::endl;

    std::vector<std::string> strings = no_ss_split(found, ",");

    for(auto& i : strings)
    {
        i = strip_whitespace(i);
    }

    for(std::string& str : strings)
    {
        std::vector<std::string> strs = no_ss_split(str, ":");

        if(strs.size() != 2)
            continue;

        std::string arg = strs[0];
        std::string param = strs[1];

        autos.push_back({arg, param});
    }
}

bool expand(std::string_view& view, std::string& in, int& offset, int& found_seclevel, autos_t& autocompletes)
{
    std::vector<std::string> froms{"#fs.", "#hs.", "#ms.", "#ls.", "#ns.",
                                   "#4s.", "#3s.", "#2s.", "#1s.", "#0s.",
                                   "#s.", "#"};

    std::vector<std::string> tos  {"fs_call", "hs_call", "ms_call", "ls_call", "ns_call",
                                   "fs_call", "hs_call", "ms_call", "ls_call", "ns_call",
                                   "ns_call", "ns_call"};

    std::vector<int> sec_levels = {4, 3, 2, 1, 0,
                                   4, 3, 2, 1, 0,
                                   0, 0};

    ///won't find D, but seems prudent to exclude it anyway
    std::vector<std::string> parse_exclusion{"D",
                                             "db.i", "db.r", "db.f", "db.u", "db.u1", "db.us",
                                             "autos"};

    for(int i=0; i < (int)tos.size(); i++)
    {
        ///this checks if something starts with fs_call etc
        ///and says that the script is that sec level
        ///should be secure
        bool is_raw_var = starts_with(view, tos[i]);

        if(is_raw_var)
        {
            found_seclevel = std::min(found_seclevel, sec_levels[i]);

            return true;
        }

        bool success = expand_to_from_scriptname(view, in, offset, froms[i], tos[i], parse_exclusion);

        if(success)
        {
            found_seclevel = std::min(found_seclevel, sec_levels[i]);

            return true;
        }
    }

    get_autocompletes(view, in, offset, autocompletes);

    std::vector<std::string> froms_unchecked{"#D",
                                             "#db.i", "#db.r", "#db.f", "#db.u", "#db.u1", "#db.us",
                                             };

    std::vector<std::string> tos_unchecked  {"hash_d",
                                             "db_insert", "db_remove", "db_find", "db_update", "db_update1", "db_upsert",
                                             };

    for(int i=0; i < (int)tos_unchecked.size(); i++)
    {
        bool success = expand_to_from_nochecks(view, in, offset, froms_unchecked[i], tos_unchecked[i]);

        if(success)
            return true;
    }

    //std::cout << in << std::endl;

    return false;
}

//https://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
std::string capture_exec(const std::string& cmd)
{
    std::array<char, 128> buffer;
    std::string result;

    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe)
        throw std::runtime_error("popen() failed!");

    while(!feof(pipe.get()))
    {
        if(fgets(buffer.data(), 128, pipe.get()) != nullptr)
        {
            result += buffer.data();
        }
    }

    return result;
}

std::string make_fill_es6(const std::string& file_name, const std::string& in)
{
    std::string compiler_dir = "compile/";

    write_all_bin(compiler_dir + file_name + ".ts", in);

    std::string res = capture_exec("C:\\Stuff\\nodejs\\node.exe transpile.js " + compiler_dir + file_name + ".ts");

    //std::cout << "es6 " << res << std::endl;

    std::string found = read_file(compiler_dir + file_name + ".ts.ts");

    //std::cout << "found " << found << std::endl;

    std::remove((compiler_dir + file_name + ".ts.ts").c_str());
    std::remove((compiler_dir + file_name + ".ts").c_str());

    return found;
}

script_data parse_script(const std::string& file_name, std::string in, bool enable_typescript)
{
    if(in.size() == 0)
        return script_data();

    int found_seclevel = 4;

    autos_t autocompletes;

    for(int i=0; i < (int)in.size(); i++)
    {
        std::string_view strview(&in[i]);

        expand(strview, in, i, found_seclevel, autocompletes);
    }

    if(enable_typescript)
    {
        in = make_fill_es6(file_name, in);
    }

    script_data script;
    script.autocompletes = autocompletes;
    script.parsed_source = in;
    script.seclevel = found_seclevel;
    script.valid = true;

    return script;
}

///WARNING NEED TO VALIDATE
std::string script_info::load_from_unparsed_source(duk_context* ctx, const std::string& source, const std::string& name_, bool enable_typescript)
{
    name = name_;

    int max_size = 32 * 1024;

    if(source.size() >= (size_t)max_size)
    {
        valid = false;
        return "Script " + name + " too large";
    }

    if(!is_valid_full_name_string(name))
    {
        valid = false;
        return "Invalid Name " + name;
    }

    owner = no_ss_split(name, ".")[0];

    unparsed_source = source;

    script_data sdata = parse_script(name, unparsed_source, enable_typescript);

    args = decltype(args)();
    params = decltype(params)();

    for(auto& i : sdata.autocompletes)
    {
        args.push_back(i.first);
        params.push_back(i.second);
    }

    parsed_source = sdata.parsed_source;
    seclevel = sdata.seclevel;
    valid = sdata.valid;

    std::string err;

    if(!script_compiles(ctx, *this, err))
    {
        valid = false;

        return err;

        //printf("failed compilation in early stage\n");
    }

    return err;
}

bool script_info::load_from_db(mongo_lock_proxy& ctx)
{
    if(!exists_in_db(ctx))
        return false;

    item my_script;

    my_script.set_prop("item_id", name);
    my_script.set_prop("in_public", 0);
    my_script.set_prop("trust", 0);
    my_script.set_prop("owner", owner);
    my_script.set_prop("is_script", 1);

    my_script.load_from_db(ctx, name);

    name = my_script.get_prop("item_id");
    in_public = my_script.get_prop_as_integer("in_public");
    //name = my_script.get_prop("item_id");
    owner = my_script.get_prop("owner");
    //is_script = my_script.get_prop("is_script");

    unparsed_source = my_script.get_prop("unparsed_source");

    args = decltype(args)();
    params = decltype(params)();

    args = my_script.get_prop_as_array("args");
    params = my_script.get_prop_as_array("params");

    parsed_source = my_script.get_prop("parsed_source");
    seclevel = my_script.get_prop_as_integer("seclevel");
    valid = my_script.get_prop_as_integer("valid");

    if(!valid || parsed_source.size() == 0)
    {
        args = decltype(args)();
        params = decltype(params)();

        script_data sdata = parse_script(name, unparsed_source, true);

        for(auto& i : sdata.autocompletes)
        {
            args.push_back(i.first);
            params.push_back(i.second);
        }

        parsed_source = sdata.parsed_source;
        seclevel = sdata.seclevel;
        valid = sdata.valid;

        std::cout << "fallback_parse" << std::endl;
        lg::log("fallback_parse");
    }

    return true;
}

void script_info::overwrite_in_db(mongo_lock_proxy& ctx)
{
    if(!valid)
        return;

    item my_script;

    my_script.set_prop("item_id", name);
    my_script.set_prop("in_public", in_public);
    my_script.set_prop("owner", owner);
    my_script.set_prop("is_script", 1);
    my_script.set_prop("unparsed_source", unparsed_source);

    my_script.set_prop_array("args", args);
    my_script.set_prop_array("params", params);

    my_script.set_prop("parsed_source", parsed_source);
    my_script.set_prop_int("seclevel", seclevel);
    my_script.set_prop_int("valid", valid);

    //mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context();

    if(my_script.exists_in_db(ctx, name))
        my_script.overwrite_in_db(ctx);
    else
    {
        my_script.set_prop("trust", 0);
        my_script.create_in_db(ctx);
    }
}

void script_info::fill_as_bundle_compatible_item(item& my_script)
{
    //my_script.set_prop("item_id", name);
    //my_script.set_prop("in_public", in_public);
    //my_script.set_prop("owner", owner);
    //my_script.set_prop("is_script", 1);
    my_script.set_prop("unparsed_source", unparsed_source);

    my_script.set_prop_array("args", args);
    my_script.set_prop_array("params", params);

    my_script.set_prop("parsed_source", parsed_source);
    my_script.set_prop_int("seclevel", seclevel);
    my_script.set_prop_int("valid", valid);
}

bool script_info::exists_in_db(mongo_lock_proxy& ctx)
{
    item my_script;

    my_script.set_prop("item_id", name);

    //mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context();

    return my_script.exists_in_db(ctx, name);
}

#if 0
void script_info::load_from_disk_with_db_metadata(const std::string& name_)
{
    std::string base = base_scripts_string;

    unparsed_source = get_script_from_name_string(base, name_);

    script_data sdata = parse_script(unparsed_source);

    parsed_source = sdata.parsed_source;
    seclevel = sdata.seclevel;
    valid = sdata.valid;

    if(!valid)
        return;

    name = name_;

    std::vector<std::string> splits = no_ss_split(name, ".");

    assert(splits.size() == 2);

    std::string owner = splits[0];

    item my_script;
    my_script.set_prop("item_id", name);
    my_script.set_prop("in_public", 0);
    my_script.set_prop("trust", 0);
    my_script.set_prop("owner", owner);
    my_script.set_prop("is_script", 1);
    my_script.set_prop("unparsed_source", unparsed_source);

    mongo_lock_proxy mongo_ctx = get_global_mongo_user_items_context();

    if(!my_script.exists_in_db(mongo_ctx))
    {
        my_script.create_in_db(mongo_ctx);
    }

    #define OVERWRITE
    #ifdef OVERWRITE
    else
    {
        my_script.update_in_db(mongo_ctx);
    }
    #endif // OVERWRITE

    my_script.load_from_db(mongo_ctx);
}
#endif

void set_script_info(duk_context* ctx, const std::string& full_script_name)
{
    std::vector<std::string> strings = no_ss_split(full_script_name, ".");

    //assert(strings.size() == 2);

    set_global_string(ctx, "script_host", strings[0]);
    set_global_string(ctx, "script_ending", strings[1]);
}
