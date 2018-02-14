#ifndef USER_HPP_INCLUDED
#define USER_HPP_INCLUDED

#include "mongo.hpp"
#include "script_util.hpp"

///how do handle upgrades
///vector of ids?
struct user
{
    std::string name;
    double cash = 0;

    void overwrite_user_in_db()
    {
        mongo_context* ctx = get_global_mongo_user_info_context();

        bson_t b;

        bson_init(&b);

        BSON_APPEND_UTF8(&b, "name", name.c_str());
        BSON_APPEND_BOOL(&b, "cash", cash);

        bson_destroy(&b);
    }

    bool construct_new_user(const std::string name_)
    {
        name = name_;

        if(!is_valid_string(name))
            return false;

        return true;
    }
};

#endif // USER_HPP_INCLUDED
