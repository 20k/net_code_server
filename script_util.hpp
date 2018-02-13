#ifndef SCRIPT_UTIL_HPP_INCLUDED
#define SCRIPT_UTIL_HPP_INCLUDED

#include <string_view>

///i think something is broken with 7.2s stringstream implementation
///i dont know why the stringstream version crashes
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

        if(!isalnum(c))
        {
            return false;
        }
    }

    return true;
}

bool is_valid_full_name_string(const std::string& name)
{
    std::string to_parse = strip_whitespace(name);

    int num_dots = std::count(to_parse.begin(), to_parse.end(), '.');

    if(num_dots != 1)
    {
        return "";
    }

    std::vector<std::string> strings = no_ss_split(to_parse, ".");

    if(strings.size() != 2)
        return false;

    bool all_valid = true;

    for(auto& str : strings)
    {
        if(!is_valid_string(str))
            all_valid = false;
    }

    if(!all_valid)
        return false;

    return true;
}

std::string get_script_from_name_string(const std::string& base_dir, const std::string& name_string)
{
    bool is_valid = is_valid_full_name_string(name_string);

    if(!is_valid)
        return "";

    std::string to_parse = strip_whitespace(name_string);

    std::replace(to_parse.begin(), to_parse.end(), '.', '/');

    return read_file(base_dir + "/" + to_parse + ".js");
}

bool expand(std::string_view& view, std::string& in, int& offset)
{
    std::string srch = "#fs.";

    if(view.substr(0, srch.size()) != srch)
        return false;

    std::cout << "expand1\n";

    bool second_half = false;

    std::string found = "";
    int found_loc = -1;

    for(int i=srch.length(); i < view.size(); i++)
    {
        char c = view[i];

        if(c == '(')
        {
            found_loc = i;
            found = std::string(view.begin() + srch.length(), view.begin() + i);
            break;
        }
    }

    bool valid = is_valid_full_name_string(found);

    //std::cout << found << "\n fnd\n";

    if(valid)
    {
        int start = offset;
        int finish = offset + found_loc;

        in.replace(offset, finish - start, "fs_call(\"" + found + "\")");
    }

    return valid;
}

std::string parse_script(std::string in)
{
    for(int i=0; i < in.size(); i++)
    {
        std::string_view strview(&in[i]);

        expand(strview, in, i);
    }

    return in;
}


#endif // SCRIPT_UTIL_HPP_INCLUDED
