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

    void set_prop_int(const std::string& str, int t)
    {
        props.set_prop_int(str, t);
    }

    std::string get_prop(const std::string& str)
    {
        return props.get_prop(str);
    }

    std::vector<std::string> get_prop_as_array(const std::string& str)
    {
        return props.get_prop_as_array(str);
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

    void set_prop_array(const std::string& key, const std::vector<std::string>& vals)
    {
        props.set_prop_array(key, vals);
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

///migrate a better version of this into secret
///pass in a probability variable
namespace item_types
{
    item get_default_of(item_types::item_type type, const std::string& lock_name);
    item get_default(item_types::item_type type);
    void give_item_to(item& new_item, const std::string& to, int thread_id);
}

#endif // ITEMS_HPP_INCLUDED
