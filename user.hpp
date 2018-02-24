#ifndef USER_HPP_INCLUDED
#define USER_HPP_INCLUDED

#include "mongo.hpp"
#include "script_util.hpp"
#include "item.hpp"

///how do handle upgrades
///vector of ids?

///ok. Need to fetch users out of the db
struct user
{
    std::string name;
    double cash = 0;
    std::string auth;
    int32_t last_message_uid = 0;
    std::string upgr_idx;

    void overwrite_user_in_db(mongo_lock_proxy& ctx)
    {
        ctx->change_collection(name);

        mongo_requester filter;
        filter.set_prop("name", name);

        mongo_requester to_set;
        to_set.set_prop("name", name);
        to_set.set_prop_double("cash", cash);
        to_set.set_prop_int("last_message_uid", last_message_uid);
        to_set.set_prop("upgr_idx", upgr_idx);

        filter.update_in_db_if_exists(ctx, to_set);
    }

    bool exists(mongo_lock_proxy& ctx, const std::string& name_)
    {
        ctx->change_collection(name_);

        bson_t* to_find = BCON_NEW("name", "{", "$exists", BCON_BOOL(true), "}");

        std::vector<std::string> ret = ctx->find_bson(name_, to_find, nullptr);

        bson_destroy(to_find);

         return ret.size() != 0;
    }

    bool load_from_db(mongo_lock_proxy& ctx, const std::string& name_)
    {
        ctx->change_collection(name_);

        if(!exists(ctx, name_))
            return false;

        mongo_requester request;
        request.set_prop("name", name_);

        auto found = request.fetch_from_db(ctx);

        for(mongo_requester& req : found)
        {
            if(req.has_prop("name"))
                name = req.get_prop("name");
            if(req.has_prop("cash"))
                cash = req.get_prop_as_double("cash");
            if(req.has_prop("auth"))
                auth = req.get_prop("auth");
            if(req.has_prop("last_message_uid"))
                last_message_uid = req.get_prop_as_integer("last_message_uid");
            if(req.has_prop("upgr_idx"))
                upgr_idx = req.get_prop("upgr_idx");
        }

        return true;
    }

    bool construct_new_user(mongo_lock_proxy& ctx, const std::string& name_, const std::string& auth, int last_message_uid_)
    {
        name = name_;
        last_message_uid = last_message_uid_;

        if(!is_valid_string(name))
            return false;

        if(exists(ctx, name))
            return false;

        ctx->change_collection(name);

        mongo_requester request;
        request.set_prop("name", name);
        request.set_prop_bin("auth", auth);
        request.set_prop_int("last_message_uid", last_message_uid);
        request.set_prop("upgr_idx", "");

        request.insert_in_db(ctx);

        return true;
    }

    void append_item(const std::string& id)
    {
        std::vector<std::string> items = str_to_array(upgr_idx);

        items.push_back(id);

        upgr_idx = array_to_str(items);
    }

    bool has_item(const std::string& id)
    {
        std::vector<std::string> items = str_to_array(upgr_idx);

        for(auto& i : items)
        {
            if(i == id)
                return true;
        }

        return false;
    }

    void remove_item(const std::string& id)
    {
        std::vector<std::string> items = str_to_array(upgr_idx);

        auto it = std::find(items.begin(), items.end(), id);

        if(it == items.end())
            return;

        items.erase(it);
    }
};

#endif // USER_HPP_INCLUDED
