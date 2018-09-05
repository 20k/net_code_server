#ifndef SCRIPT_METADATA_HPP_INCLUDED
#define SCRIPT_METADATA_HPP_INCLUDED

#include <string>
#include <vector>

struct arg_metadata
{
    enum arg_type
    {
        UNKNOWN = 0,
        STRING = 1,
        INTEGER = 2,
        USER = 4,
        NUMERIC = 8,
        CASH = 16,
        ITEM_IDX = 32,
        OK = 64,
        SCRIPT = 128,
        SECURITY_LEVEL = 256,
    };

    std::string key_name;
    std::string val_text;

    arg_type type = arg_type::UNKNOWN;
};

struct script_metadata
{
    std::vector<arg_metadata> param_data;
    std::vector<arg_metadata> return_data;
    std::string description;
    bool requires_breach = false;

    std::string dump();
    void load_from_string(const std::string& str);
};

#endif // SCRIPT_METADATA_HPP_INCLUDED
