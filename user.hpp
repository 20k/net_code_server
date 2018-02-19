#ifndef USER_HPP_INCLUDED
#define USER_HPP_INCLUDED

#include "mongo.hpp"
#include "script_util.hpp"

///how do handle upgrades
///vector of ids?

///ok. Need to fetch users out of the db
struct user
{
    std::string name;
    double cash = 0;
    std::string auth;

    /*bson_t* get_bson_representation()
    {
        bson_t* to_update = BCON_NEW(
                                     //"$set",
                                     //"{",
                                         "name",
                                         BCON_UTF8(name.c_str()),
                                         "cash",
                                         BCON_DOUBLE(cash)
                                     //"}"
                                     );
        return to_update;
    }*/

    void overwrite_user_in_db(mongo_lock_proxy& ctx)
    {
        ctx->change_collection(name);

        bson_t* to_update = BCON_NEW(
                                     "$set",
                                     "{",
                                         "name",
                                         BCON_UTF8(name.c_str()),
                                         "cash",
                                         BCON_DOUBLE(cash),
                                     "}"
                                     );

        bson_t* selection = BCON_NEW(
                                     "name",
                                     "{",
                                         "$exists",
                                         BCON_BOOL(true),
                                     "}"
                                     );

        ctx->update_bson_many(name, selection, to_update);

        bson_destroy(selection);
        bson_destroy(to_update);
    }

    bool exists(mongo_lock_proxy& ctx, const std::string& name_, bool no_lock = false)
    {
        ctx->change_collection(name_);

        bson_t* to_find = BCON_NEW("name", "{", "$exists", BCON_BOOL(true), "}");

        std::vector<std::string> ret = ctx->find_bson(name_, to_find, nullptr);

        bson_destroy(to_find);

        /*for(auto& i : ret)
        {
            std::cout << i << std::endl;
        }*/

        return ret.size() != 0;
    }

    bool load_from_db(mongo_lock_proxy& ctx, const std::string& name_)
    {
        ctx->change_collection(name_);

        //std::cout << "load \n";

        if(!exists(ctx, name_, true))
            return false;

        //bson_t* to_find = BCON_NEW("name", BCON_UTF8(name_.c_str()));

        bson_t* to_find = BCON_NEW("name", "{", "$exists", BCON_BOOL(true), "}");

        std::vector<std::string> json = ctx->find_bson(name_, to_find, nullptr);

        bson_destroy(to_find);

        for(auto& i : json)
        {
            if(i.size() == 0)
                continue;

            bson_t* next = bson_new_from_json((const uint8_t*)i.c_str(), i.size(), nullptr);

            size_t offset = 0;

            if(!bson_validate(next, (bson_validate_flags_t)(BSON_VALIDATE_UTF8 | BSON_VALIDATE_UTF8_ALLOW_NULL), &offset))
            {
                std::cout << "bson invalid in load from db\n";

                bson_destroy(next);
                continue;
            }

            bson_iter_t iter;

            bson_iter_init(&iter, next);

            while (bson_iter_next (&iter))
            {
                std::string key = bson_iter_key(&iter);

                if(key == "name")
                {
                    name = bson_iter_utf8_easy(&iter);
                }

                if(key == "cash")
                {
                    cash = bson_iter_double(&iter);
                }

                if(key == "auth")
                {
                    auth = bson_iter_binary_std_string(&iter);
                }
            }

            bson_destroy(next);
        }

        return true;
    }

    bool construct_new_user(mongo_lock_proxy& ctx, const std::string& name_, const std::string& auth)
    {
        name = name_;

        if(!is_valid_string(name))
            return false;

        if(exists(ctx, name))
            return false;

        ctx->change_collection(name);

        mongo_requester request;
        request.set_prop("name", name);
        request.set_prop_bin("auth", auth);

        request.insert_in_db(ctx);

        return true;
    }
};

#endif // USER_HPP_INCLUDED
