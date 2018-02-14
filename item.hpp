#ifndef ITEMS_HPP_INCLUDED
#define ITEMS_HPP_INCLUDED

#include <string>
#include <vector>
#include "mongo.hpp"

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
    /*int32_t id = -1;
    int32_t rarity = 0;

    item_types::item_type type = item_types::ERR;

    ///Hmm. Not normalising descriptions is going to cost a lot of space
    ///It allows for very custom one off items
    ///and potentially even custom player items... which is actually necessary for scripts, so we can't just use a predefined table
    ///would have to do db stuff to fix this
    std::string name;
    std::string description;*/

    std::map<std::string, std::string> item_properties;

    std::string get_prop(const std::string& str)
    {
        return item_properties[str];
    }

    int32_t get_prop_as_integer(const std::string& str)
    {
        std::string prop = item_properties[str];

        long long val = atoll(prop.c_str());

        return val;
    }

    ///WARNING NOT THREAD SAFE AT ALL RACE CONDITION
    ///NEED TO USE SOME SORT OF MONGO LOCKING WHEN/IF WE HAVE MULTIPLE SERVERS (!!!)
    int32_t get_new_id()
    {
        mongo_context* ctx = get_global_mongo_global_properties_context();

        bson_t* to_find = BCON_NEW("items_id", "{", "$exists", BCON_BOOL(true), "}");

        std::vector<std::string> ret = ctx->find_bson("global_properties", to_find, nullptr);

        int32_t next_id = -1;

        if(ret.size() == 0)
        {
            ///insert items_id into db
            bson_t* to_insert = BCON_NEW("items_id", BCON_INT32(0));

            ctx->insert_bson_1("global_properties", to_insert);

            bson_destroy(to_insert);

            next_id = 0;
        }
        else
        {
            const std::string& str = ret[0];

            bson_t* next = bson_new_from_json((const uint8_t*)str.c_str(), str.size(), nullptr);

            if(next == nullptr)
            {
                printf("fatal id error, should be literally impossible\n");
                next_id = -2;
            }

            bson_iter_t iter;
            bson_iter_init(&iter, next);

            while (bson_iter_next (&iter))
            {
                std::string key = bson_iter_key(&iter);

                if(key == "items_id")
                {
                    int32_t found = bson_iter_int32(&iter);

                    next_id = found + 1;
                }
            }
        }

        bson_t* to_update = BCON_NEW("$set", "{", "items_id", BCON_INT32(next_id), "}");

        ctx->update_bson_many("global_properties", to_find, to_update);

        bson_destroy(to_update);
        bson_destroy(to_find);

        if(next_id < 0)
            return next_id;

        return next_id;
    }
};

namespace item_types
{
    //item get_default_of(item)
}

#endif // ITEMS_HPP_INCLUDED
