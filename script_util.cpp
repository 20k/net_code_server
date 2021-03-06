#include "script_util.hpp"
#include <assert.h>
#include "item.hpp"
#include <libncclient/nc_util.hpp>
#include "duk_object_functions.hpp"
#include <memory>
#include "logging.hpp"
#ifdef __WIN32__
#include <shellapi.h>
#else
#include <sys/stat.h>
#endif
#include <dirent.h>
#include "source_maps.hpp"

#include "argument_object.hpp"

///new api
///we need a function to upload it to the server
///since we're the server, we need a function to accept a string which is the script
///check that its valid
///then stash it in the db with metadata

///then, when we want to load, we look from the db

/*std::string attach_cli_wrapper(const std::string& data_in)
{
    return "require(\"@babel/polyfill\");\n" + data_in;
}*/

std::string attach_cli_wrapper(const std::string& str)
{
    return str;
    //return "(function clifunc(){return (" + str + ");})";
}

std::string attach_unparsed_wrapper(std::string str)
{
    while(str.size() > 0 && str.back() != '}')
        str.pop_back();

    return "(function mainfunc(){var func = (" + str + "); return func(context, args);})";
}

bool script_compiles(js::value_context& vctx, script_info& script, std::string& err_out)
{
    std::string wrapper = script.parsed_source;

    auto [success, result] = js::compile(vctx, wrapper);

    //#define DEBUG_REAL
    #ifdef DEBUG_REAL
    std::cout << wrapper << std::endl;
    #endif // DEBUG_REAL

    if(!success)
    {
        err_out = result.to_error_message();

        printf("scompile failed: %s\n", err_out.c_str());

        #ifdef DEBUG_SOURCE
        std::cout << script.parsed_source << std::endl;
        #endif // DEBUG_SOURCE
    }
    else
    {
        err_out = "";
    }

    return success;
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
                                   "#os.",
                                   "#ofs.", "#ohs.", "#oms.", "#ols.", "#ons.",
                                   "#o4s.", "#o3s.", "#o2s.", "#o1s.", "#o0s.",
                                   "#s.", "#"};

    std::vector<std::string> tos  {"fs_call", "hs_call", "ms_call", "ls_call", "ns_call",
                                   "fs_call", "hs_call", "ms_call", "ls_call", "ns_call",
                                   "os_call",
                                   "ofs_call", "ohs_call", "oms_call", "ols_call", "ons_call",
                                   "ofs_call", "ohs_call", "oms_call", "ols_call", "ons_call",
                                   "ns_call", "ns_call"};

    std::vector<int> sec_levels = {4, 3, 2, 1, 0,
                                   4, 3, 2, 1, 0,
                                   4,
                                   4, 4, 4, 4, 4,
                                   4, 4, 4, 4, 4,
                                   0, 0};

    assert(froms.size() == tos.size());
    assert(froms.size() == sec_levels.size());

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
                                             "$db",
                                             };

    std::vector<std::string> tos_unchecked  {"hash_d",
                                             "db_insert", "db_remove", "db_find", "db_update", "db_update1", "db_upsert",
                                             "$db",
                                             };

    std::vector<int> sec_levels_unchecked   {4,
                                             3, 3, 3, 3, 3, 3,
                                             3};

    assert(sec_levels_unchecked.size() == tos_unchecked.size());
    assert(froms_unchecked.size() == tos_unchecked.size());

    for(int i=0; i < (int)tos_unchecked.size(); i++)
    {
        bool is_raw_var = starts_with(view, tos_unchecked[i]);

        if(is_raw_var)
        {
            found_seclevel = std::min(found_seclevel, sec_levels_unchecked[i]);

            return true;
        }

        bool success = expand_to_from_nochecks(view, in, offset, froms_unchecked[i], tos_unchecked[i]);

        if(success)
        {
            found_seclevel = std::min(found_seclevel, sec_levels_unchecked[i]);

            return true;
        }
    }

    //std::cout << in << std::endl;

    return false;
}

#ifdef USE_DUKTAPE
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

void crappy_exec(const std::string& cmd, const std::string& params)
{
    system((cmd + params).c_str());

    //ShellExecute(NULL, "open", cmd.c_str(), params.c_str(), nullptr, SW_SHOWDEFAULT);
}

std::string process_resulting_code(std::string in)
{
    std::string all = "\"use strict\";";

    auto found = in.find(all);

    if(found != std::string::npos)
    {
        for(int i=0; i < (int)all.size(); i++, found++)
        {
            in[found] = ' ';
        }
    }

    return in;
}

std::pair<std::string, std::string> make_fill_es6(const std::string& file_name, const std::string& in)
{
    std::string compiler_dir = "./compile/";

    #ifdef __WIN32__
    mkdir(compiler_dir.c_str());
    #else
    mkdir(compiler_dir.c_str(), 0777);
    #endif

    std::string phase_1 = compiler_dir + file_name + ".out1.js";

    write_all_bin(phase_1, in);

    #ifdef __WIN32__
    std::string res = capture_exec("c:/stuff/nodejs/node.exe script_compile/transpile.js " + phase_1);
    #else
    std::string res = capture_exec("node script_compile/transpile.js " + phase_1);
    #endif // __WIN32__

    nlohmann::json data;

    try
    {
        data = nlohmann::json::parse(read_file(phase_1 + ".ts"));
    }
    catch(...)
    {
        std::cout << "didn't go well\n";
    }

    remove((phase_1 + ".ts").c_str());
    remove((phase_1).c_str());

    //std::cout << "DATA " << data.dump() << std::endl;

    try
    {
        if(data.count("bable_error") > 0)
        {
            int error_line = 0;
            int error_column = 0;
            std::string code;

            std::string typescript_sourcemap;

            //int error_pos = data["bable_error"]["pos"];
            /*int error_line = (int)data["bable_error"]["loc"]["line"] - 1;
            int error_column = (int)data["bable_error"]["loc"]["column"] - 1;
            std::string code = data["code_posttype"]["outputText"];

            ///js object
            std::string typescript_sourcemap = data["code_posttype"]["sourceMapText"];*/

            /*try
            {
                error_pos = data["babel_error"]["pos"];
            }
            catch(...)
            {
                std::cout << "bad error pos\n";
                throw;
            }*/

            try
            {
                error_line = (int)data["bable_error"]["loc"]["line"] - 1;
            }
            catch(...)
            {
                std::cout << "bad error line\n";

                std::cout << "got res " << res << std::endl;

                throw;
            }
            try
            {
                error_column = (int)data["bable_error"]["loc"]["column"] - 1;
            }
            catch(...)
            {
                std::cout << "bad error col\n";
                throw;
            }

            try
            {
                code = data["code_posttype"]["outputText"];
            }
            catch(...)
            {
                std::cout << "bad error code\n";
                throw;
            }

            try
            {
                typescript_sourcemap = data["code_posttype"]["sourceMapText"];
            }
            catch(...)
            {
                std::cout << "bad error map\n";
                throw;
            }

            source_map src_map;
            src_map.decode(in, code, typescript_sourcemap);

            source_position mapped = src_map.map({error_line, error_column});

            std::string formatted_error = src_map.get_caret_text_of(mapped);

            //data["bable_error"]["context"] = line;

            return {"", formatted_error};
        }
    }
    catch(const std::exception& e)
    {
        std::cout << "Error trying to get bable_error " << e.what() << std::endl;

        return {"", "Caught exception in checking babel_error: " + data.dump()};
    }

    return {process_resulting_code(data["code_postbabel"]["code"]), ""};
}
#endif // USE_DUKTAPE

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

    script_data script;

    script.valid = true;

    #ifdef USE_DUKTAPE
    if(enable_typescript)
    {
        std::pair<std::string, std::string> result = make_fill_es6(file_name, in);

        in = result.first;

        if(result.second != "")
        {
            script.compile_error = result.second;
            script.valid = false;
        }
    }
    #endif // USE_DUKTAPE

    script.autocompletes = autocompletes;
    script.parsed_source = in;
    script.seclevel = found_seclevel;

    return script;
}

///WARNING NEED TO VALIDATE
std::string script_info::load_from_unparsed_source(js::value_context& vctx, const std::string& source, const std::string& name_, bool enable_typescript, bool is_cli)
{
    name = name_;

    #ifndef TESTING
    int max_size = 64 * 1024;
    #else
    int max_size = 64 * 1024*1024;
    #endif

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

    auto post_split = no_ss_split(name, ".");

    if(post_split.size() == 0)
        return "Invalid Name " + name;

    owner = post_split[0];
    unparsed_source = source;

    script_data sdata;

    if(!is_cli)
        sdata = parse_script(name, attach_unparsed_wrapper(unparsed_source), enable_typescript);
    else
        sdata = parse_script(name, unparsed_source, enable_typescript);

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
    metadata = decltype(metadata)();

    if(!sdata.valid && sdata.compile_error != "")
        return sdata.compile_error;

    std::string err;

    if(!script_compiles(vctx, *this, err))
    {
        valid = false;

        return err;

        //printf("failed compilation in early stage\n");
    }

    return err;
}

bool script_info::load_from_db(db::read_tx& ctx)
{
    if(!exists_in_db(ctx))
        return false;

    item my_script;

    my_script.set_as("item_id", name);
    my_script.set_as("in_public", 0);
    my_script.set_as("owner", owner);
    my_script.set_as("is_script", 1);

    db_disk_load(ctx, my_script, name);

    //name = my_script.get("item_id");
    in_public = my_script.get_int("in_public");
    //name = my_script.get_prop("item_id");
    owner = my_script.get_string("owner");
    //is_script = my_script.get_prop("is_script");

    unparsed_source = my_script.get_string("unparsed_source");

    args = decltype(args)();
    params = decltype(params)();

    args = (std::vector<std::string>)my_script.get_untyped("args");
    params = (std::vector<std::string>)my_script.get_untyped("params");

    parsed_source = my_script.get_string("parsed_source");
    seclevel = my_script.get_int("seclevel");
    valid = my_script.get_int("valid");

    //std::cout << "valid? " << valid << std::endl;

    metadata.load_from_string(my_script.get_string("metadata"));

    if(!valid || parsed_source.size() == 0)
    {
        args = decltype(args)();
        params = decltype(params)();

        script_data sdata = parse_script(name, attach_unparsed_wrapper(unparsed_source), true);

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

void script_info::overwrite_in_db(db::read_write_tx& ctx)
{
    if(!valid)
        return;

    item my_script;

    my_script.set_as("item_id", name);
    my_script.set_as("in_public", in_public);
    my_script.set_as("owner", owner);
    my_script.set_as("is_script", 1);
    my_script.set_as("unparsed_source", unparsed_source);

    my_script.set_as("args", args);
    my_script.set_as("params", params);

    my_script.set_as("parsed_source", parsed_source);
    my_script.set_as("seclevel", seclevel);
    my_script.set_as("valid", valid);
    my_script.set_as("metadata", metadata.dump());

    db_disk_overwrite(ctx, my_script);
}

void script_info::fill_as_bundle_compatible_item(item& my_script)
{
    //my_script.set_as("item_id", name);
    //my_script.set_as("in_public", in_public);
    //my_script.set_as("owner", owner);
    //my_script.set_as("is_script", 1);
    my_script.set_as("unparsed_source", unparsed_source);

    my_script.set_as("args", args);
    my_script.set_as("params", params);

    my_script.set_as("parsed_source", parsed_source);
    my_script.set_as("seclevel", seclevel);
    my_script.set_as("valid", valid);
    my_script.set_as("metadata", metadata.dump());
}

bool script_info::exists_in_db(db::read_tx& ctx)
{
    item my_script;

    my_script.set_as("item_id", name);

    return db_disk_exists(ctx, my_script);
}

void set_script_info(js::value_context& vctx, const std::string& full_script_name)
{
    std::vector<std::string> strings = no_ss_split(full_script_name, ".");

    js::value heap = js::get_heap_stash(vctx);

    js::add_key_value(heap, "script_host", strings[0]);
    js::add_key_value(heap, "script_ending", strings[1]);
}
