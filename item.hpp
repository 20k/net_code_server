#ifndef ITEMS_HPP_INCLUDED
#define ITEMS_HPP_INCLUDED

#include <string>
#include <vector>
#include <utility>
#include "mongo.hpp"
#include "db_interfaceable.hpp"

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
        ON_BREACH = 8,
        ERR = 9,
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
        "on_breach",
        "error_vnf",
    };

    inline
    double rotation_time_s = 60 * 15;
}

bool array_contains(const std::vector<std::string>& arr, const std::string& str);
std::vector<std::string> str_to_array(const std::string& str);
std::string array_to_str(const std::vector<std::string>& arr);

#define MAX_ITEMS 48

struct item : db_interfaceable<item, MACRO_GET_STR("item_id")>
{
    std::string get_prop(const std::string& str)
    {
        if(!has(str))
            return std::string();

        try
        {
            return get_stringify(str);
        }
        catch(...)
        {
            return std::string();
        }
    }

    void generate_set_id(mongo_lock_proxy& global_props_context)
    {
        int32_t id = get_new_id(global_props_context);

        set_as("item_id", id);
    }

    int32_t get_new_id(mongo_lock_proxy& global_props_context);

    ///manages lock proxies internally
    bool transfer_to_user(const std::string& name, int thread_id);
    bool remove_from_user(const std::string& name, int thread_id);

    bool transfer_from_to(const std::string& from, const std::string& to, int thread_id);

    bool transfer_from_to_by_index(int index, const std::string& from, const std::string& to, int thread_id);

    bool should_rotate();
    void handle_rotate();
    void force_rotate();
    void breach();
    bool is_breached();
};

extern
double get_wall_time_s();

template<typename T>
void for_each_item(T t)
{
    std::vector<item> id;

    {
        mongo_lock_proxy ctx = get_global_mongo_user_items_context(-2);

        id = item::fetch_all_from_db(ctx);
    }

    for(auto& i : id)
    {
        t(i);
    }
}

std::vector<item> load_items(mongo_lock_proxy& items_ctx, const std::vector<std::string>& ids);

///migrate a better version of this into secret
///pass in a probability variable
namespace item_types
{
    item get_default_of(item_types::item_type type, const std::string& lock_name);
    item get_default(item_types::item_type type);
    item get_named_describer(const std::string& short_name, const std::string& description);
    void give_item_to(item& new_item, const std::string& to, int thread_id);
}

#endif // ITEMS_HPP_INCLUDED
