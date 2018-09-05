#include "script_metadata.hpp"
#include <json/json.hpp>

void to_json(nlohmann::json& j, const arg_metadata& p)
{
    j = nlohmann::json{
        {"k", p.key_name},
        {"v", p.val_text},
        {"t", (int)p.type},
        };
}

void from_json(const nlohmann::json& j, arg_metadata& p)
{
    p.key_name = j.at("k");
    p.val_text = j.at("v");
    p.type = (arg_metadata::arg_type)j.at("t");
}


void to_json(nlohmann::json& j, const script_metadata& p)
{
    j = nlohmann::json{
        {"p", p.param_data},
        {"r", p.return_data},
        {"d", p.description},
        {"b", p.requires_breach},
        };
}

void from_json(const nlohmann::json& j, script_metadata& p)
{
    p.param_data = j.at("p").get<decltype(p.param_data)>();
    p.return_data = j.at("r").get<decltype(p.return_data)>();
    p.description = j.at("d");
    p.requires_breach = j.at("b");
}


std::string script_metadata::dump()
{
    return nlohmann::json(*this).dump();
}

void script_metadata::load_from_string(const std::string& in)
{
    *this = nlohmann::json::parse(in);
}

std::string arg_metadata::get_example()
{
    if((type & NODE_IDX) > 0)
        return enum_examples[NODE_IDX];
    if((type & USER) > 0)
        return enum_examples[USER];
    if((type & CASH) > 0)
        return enum_examples[CASH];

    if((type & OK) > 0)
        return enum_examples[OK];
    if((type & SCRIPT) > 0)
        return enum_examples[SCRIPT];
    if((type & SECURITY_LEVEL) > 0)
        return enum_examples[SECURITY_LEVEL];

    if((type & CHANNEL) > 0)
        return enum_examples[CHANNEL];
    if((type & ITEM_IDX) > 0)
        return enum_examples[ITEM_IDX];
    if((type & SCRIPTABLE) > 0)
        return enum_examples[SCRIPTABLE];

    if((type & INTEGER) > 0)
        return enum_examples[INTEGER];
    if((type & NUMERIC) > 0)
        return enum_examples[NUMERIC];
    if((type & ARRAY) > 0)
        return enum_examples[ARRAY];

    if((type & STRING) > 0)
        return enum_examples[STRING];
    if((type & BOOLEAN) > 0)
        return enum_examples[BOOLEAN];

    return enum_examples[UNKNOWN];
}
