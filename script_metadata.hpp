#ifndef SCRIPT_METADATA_HPP_INCLUDED
#define SCRIPT_METADATA_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>

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
        ARRAY = 512,
        SCRIPTABLE = 1024,
        CHANNEL = 2048,
        BOOLEAN = 4096,
        NODE_IDX = 8192,
        NODE_STRING = 16384,
    };

    static inline std::map<arg_type, std::string> enum_examples
    {
        {UNKNOWN, ""},
        {STRING, "some_string"},
        {INTEGER, "5"},
        {USER, "\"core\""},
        {NUMERIC, "12.7"},
        {CASH, "12.7"},
        {ITEM_IDX, "2"},
        {OK, "{ok:true}"},
        {SCRIPT, "\"scripts.core\""},
        {SECURITY_LEVEL, "3"},
        {ARRAY, "[]"},
        {SCRIPTABLE, "true"},
        {CHANNEL, "\"global\""},
        {BOOLEAN, "true"},
        {NODE_IDX, "-1"},
        {NODE_STRING, "\"F\""},
    };

    std::string key_name;
    std::string val_text;

    arg_type type = arg_type::UNKNOWN;

    std::string get_example();
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
