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

struct item : db_interfaceable<item, true, MACRO_GET_STR("item_id")>
{
    template<typename T>
    void set_prop(const std::string& str, const T& t)
    {
        set_stringify_as(str, t);
    }

    void set_prop_int(const std::string& str, int t)
    {
        set_as(str, t);
    }

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

    std::vector<std::string> get_prop_as_array(const std::string& str)
    {
        if(!has(str))
            return std::vector<std::string>();

        try
        {
            return get_as<std::vector<std::string>>(str);
        }
        catch(...)
        {
            return std::vector<std::string>();
        }
    }

    int32_t get_prop_as_integer(const std::string& str)
    {
        return (int32_t)get_prop_as_long(str);
    }

    int64_t get_prop_as_long(const std::string& str)
    {
        if(!has(str))
            return int64_t();

        try
        {
            std::string prop = get_stringify(str);

            if(prop == "")
                return 0;

            long long val = atoll(prop.c_str());

            return val;
        }
        catch(...)
        {
            return 0;
        }
    }

    double get_prop_as_double(const std::string& str)
    {
        if(!has(str))
            return double();

        try
        {
            std::string prop = get_as<std::string>(str);

            if(prop == "")
                return 0;

            auto val = atof(prop.c_str());

            return val;
        }
        catch(...)
        {
            return 0.;
        }
    }

    void set_prop_array(const std::string& key, const std::vector<std::string>& vals)
    {
        std::vector<std::string> strs;

        for(auto& i : vals)
        {
            strs.push_back(stringify_hack(i));
        }

        set_as(key, vals);
    }

    void generate_set_id(mongo_lock_proxy& global_props_context)
    {
        int32_t id = get_new_id(global_props_context);

        set_prop("item_id", id);
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
};

extern
double get_wall_time_s();

template<typename T>
void for_each_item(T t)
{
    std::vector<item> id;

    {
        mongo_lock_proxy ctx = get_global_mongo_user_items_context(-2);

        mongo_requester req;
        req.exists_check["item_id"] = 1;

        auto all = req.fetch_from_db(ctx);

        for(auto& i : all)
        {
            item n;
            n.load_from_db(ctx, i.get_prop("item_id"));

            id.push_back(n);
        }
    }

    for(auto& i : id)
    {
        t(i);
    }
}

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
