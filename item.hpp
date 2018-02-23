#ifndef ITEMS_HPP_INCLUDED
#define ITEMS_HPP_INCLUDED

#include <string>
#include <vector>
#include <utility>
#include "mongo.hpp"

struct mongo_lock_proxy;

namespace item_types
{
    enum item_type
    {
        SCRIPT, ///expose access logs
        LOCK,
        CHAR_COUNT,
        SCRIPT_SLOT,
        PUBLIC_SCRIPT_SLOT,
        MISC,
        ERR,
    };

    static std::vector<std::string> quick_names
    {
        "Script",
        "Lock",
        "Char Count",
        "Script Slot",
        "Public Script Slot",
        "Misc",
        "Error not found",
    };
}

struct item
{
    std::map<std::string, std::string> properties;

    template<typename T>
    void set_prop(const std::string& str, const T& t)
    {
        properties[str] = stringify_hack(t);
    }

    std::string get_prop(const std::string& str)
    {
        return properties[str];
    }

    int32_t get_prop_as_integer(const std::string& str)
    {
        return atoll(properties[str].c_str());
    }

    void generate_set_id(mongo_lock_proxy& global_props_context)
    {
        int32_t id = get_new_id(global_props_context);

        set_prop("item_id", id);
    }

    int32_t get_new_id(mongo_lock_proxy& global_props_context);

    bool exists_in_db(mongo_lock_proxy&, const std::string& item_id);
    void overwrite_in_db(mongo_lock_proxy&);
    void create_in_db(mongo_lock_proxy&);
    void load_from_db(mongo_lock_proxy&, const std::string& item_id);
};

namespace item_types
{
/*item get_default_of(item_types::item_type type)
{
    item new_item;
    new_item.generate_set_id();


}*/
}

#endif // ITEMS_HPP_INCLUDED
