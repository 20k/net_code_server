#include "script_util.hpp"
#include <assert.h>
#include "item.hpp"

///new api
///we need a function to upload it to the server
///since we're the server, we need a function to accept a string which is the script
///check that its valid
///then stash it in the db with metadata

///then, when we want to load, we look from the db

struct script_data
{
    std::string parsed_source;
    int seclevel = 0;
    bool valid = false;
};

inline
script_data parse_script(std::string in)
{
    if(in.size() == 0)
        return script_data();

    int found_seclevel = 4;

    for(int i=0; i < (int)in.size(); i++)
    {
        std::string_view strview(&in[i]);

        expand(strview, in, i, found_seclevel);
    }

    script_data script;
    script.parsed_source = in;
    script.seclevel = found_seclevel;
    script.valid = true;

    return script;
}

///WARNING NEED TO VALIDATE
void script_info::load_from_unparsed_source(const std::string& source, const std::string& name_)
{
    name = name_;

    if(!is_valid_full_name_string(name))
    {
        valid = false;
        return;
    }

    owner = no_ss_split(name, ".")[0];

    unparsed_source = source;

    script_data sdata = parse_script(unparsed_source);

    parsed_source = sdata.parsed_source;
    seclevel = sdata.seclevel;
    valid = sdata.valid;
}

void script_info::load_from_db()
{
    item my_script;

    my_script.set_prop("item_id", name);
    my_script.set_prop("in_public", true);
    my_script.set_prop("trust", false);
    my_script.set_prop("owner", owner);
    my_script.set_prop("is_script", true);

    my_script.load_from_db();
}

void script_info::overwrite_in_db()
{
    item my_script;

    my_script.set_prop("item_id", name);
    my_script.set_prop("in_public", in_public);
    my_script.set_prop("owner", owner);
    my_script.set_prop("is_script", true);

    if(my_script.exists_in_db())
        my_script.update_in_db();
    else
    {
        my_script.set_prop("trust", false);
        my_script.create_in_db();
    }
}

void script_info::load_from_disk_with_db_metadata(const std::string& name_)
{
    std::string base = base_scripts_string;

    script_data sdata = parse_script(get_script_from_name_string(base, name_));

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
    my_script.set_prop("in_public", true);
    my_script.set_prop("trust", false);
    my_script.set_prop("owner", owner);
    my_script.set_prop("is_script", true);

    if(!my_script.exists_in_db())
    {
        my_script.create_in_db();
    }

    /*#define OVERWRITE
    #ifdef OVERWRITE
    else
    {
        my_script.update_in_db(owner);
    }
    #endif // OVERWRITE*/

    my_script.load_from_db();
}
