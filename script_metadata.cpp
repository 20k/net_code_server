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
        {"a", p.data},
        {"d", p.description},
        };
}

void from_json(const nlohmann::json& j, script_metadata& p)
{
    p.data = j.at("a").get<decltype(p.data)>();
    p.description = j.at("d");
}


std::string script_metadata::dump()
{
    return nlohmann::json(*this).dump();
}

void script_metadata::load_from_string(const std::string& in)
{
    *this = nlohmann::json::parse(in);
}
