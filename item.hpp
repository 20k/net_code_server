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
        EMPTY_SCRIPT_BUNDLE,
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

    ///manages lock proxies internally
    bool transfer_to_user(const std::string& name, int thread_id);
    bool remove_from_user(const std::string& name, int thread_id);

    bool transfer_from_to(const std::string& from, const std::string& to, int thread_id);

    bool transfer_from_to_by_index(int index, const std::string& from, const std::string& to, int thread_id);
};

namespace item_types
{
    inline
    item get_default_of(item_types::item_type type)
    {
        using namespace item_types;

        item new_item;
        new_item.set_prop("item_type", (int)type);
        new_item.set_prop("rarity", 0);
        new_item.set_prop("native_item", 1); ///identifies this class of item, separates it from built in scripts

        if(type < quick_names.size() && type >= 0)
            new_item.set_prop("short_name", quick_names[(int)type]);

        if(type == CHAR_COUNT)
        {
            new_item.set_prop("char_count", 500);
            new_item.set_prop("desc", "Increases the max number of chars you can have in a script");
        }

        if(type == SCRIPT_SLOT)
        {
            new_item.set_prop("script_slots", 1);
            new_item.set_prop("desc", "Increases the number of scripts you can have uploaded");
        }

        if(type == PUBLIC_SCRIPT_SLOT)
        {
            new_item.set_prop("public_script_slots", 1);
            new_item.set_prop("desc", "Increases the number of public scripts you can have uploaded");
        }

        if(type == EMPTY_SCRIPT_BUNDLE)
        {
            new_item.set_prop("max_script_size", 500);
            new_item.set_prop("open_source", 0);
            new_item.set_prop("desc", "Container for a tradeable script");
        }

        if(type == MISC)
        {
            new_item.set_prop("misc", 1);
            new_item.set_prop("desc", "???");
        }

        return new_item;
    }
}

#endif // ITEMS_HPP_INCLUDED
