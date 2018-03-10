#ifndef ITEMS_HPP_INCLUDED
#define ITEMS_HPP_INCLUDED

#include <string>
#include <vector>
#include <utility>
#include "mongo.hpp"

#include "rng.hpp"

struct mongo_lock_proxy;

namespace item_types
{
    enum item_type
    {
        SCRIPT = 0, ///expose access logs
        LOCK = 1,
        CHAR_COUNT = 2,
        SCRIPT_SLOT = 3,
        PUBLIC_SCRIPT_SLOT = 4,
        EMPTY_SCRIPT_BUNDLE = 5,
        MISC = 6,
        AUTO_SCRIPT_RUNNER = 7,
        ERR = 8,
    };

    static std::vector<std::string> quick_names
    {
        "script",
        "lock",
        "char_count",
        "script_slot",
        "public_script_slot",
        "script_bundle",
        "misc",
        "auto_script_runner",
        "error_vnf",
    };

    inline
    double rotation_time_s = 60 * 15;
}

bool array_contains(const std::vector<std::string>& arr, const std::string& str);
std::vector<std::string> str_to_array(const std::string& str);
std::string array_to_str(const std::vector<std::string>& arr);

struct item
{
    mongo_requester props;

    template<typename T>
    void set_prop(const std::string& str, const T& t)
    {
        props.set_prop(str, t);
    }

    std::string get_prop(const std::string& str)
    {
        return props.get_prop(str);
    }

    int32_t get_prop_as_integer(const std::string& str)
    {
        return props.get_prop_as_integer(str);
    }

    int64_t get_prop_as_long(const std::string& str)
    {
        return props.get_prop_as_integer(str);
    }

    double get_prop_as_double(const std::string& str)
    {
        return props.get_prop_as_double(str);
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

    bool should_rotate();
    void handle_rotate();
};

extern
double get_wall_time_s();

namespace item_types
{
    inline
    item get_default_of(item_types::item_type type, const std::string& lock_name)
    {
        using namespace item_types;

        item new_item;
        new_item.set_prop("item_type", (int)type);
        new_item.set_prop("rarity", 0);
        new_item.set_prop("native_item", 1); ///identifies this class of item, separates it from built in scripts
        new_item.set_prop("tier", "0");

        if(type < quick_names.size() && type >= 0)
            new_item.set_prop("short_name", quick_names[(int)type]);

        if(type == LOCK)
        {
            new_item.set_prop("lock_type", lock_name);
            new_item.set_prop("short_name", lock_name);
            new_item.set_prop("lock_state", get_random_uint32_t());
            new_item.set_prop("lock_last_rotate_s", get_wall_time_s());
        }

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
            new_item.set_prop("full", 0);
            new_item.set_prop("registered_as", "");
            new_item.set_prop("in_public", "0");
        }

        if(type == MISC)
        {
            new_item.set_prop("misc", 1);
            new_item.set_prop("desc", "???");
        }

        if(type == AUTO_SCRIPT_RUNNER)
        {
            new_item.set_prop("run_every_s", 60*10);
            new_item.set_prop("last_run", 0);
        }

        return new_item;
    }
}

#endif // ITEMS_HPP_INCLUDED
