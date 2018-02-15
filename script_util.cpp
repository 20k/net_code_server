#include "script_util.hpp"

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
    script.valid = true;

    return script;
}

void script_info::load_from_disk(const std::string& name)
{
    std::string base = base_scripts_string;

    *this = parse_script(get_script_from_name_string(base, name));
}
