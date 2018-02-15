#include "script_util.hpp"

void script_info::load(const std::string& name)
{
    std::string base = base_scripts_string;

    *this = parse_script(get_script_from_name_string(base, name));
}
