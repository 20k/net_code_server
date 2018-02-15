#ifndef ITEMS_HPP_INCLUDED
#define ITEMS_HPP_INCLUDED

#include <string>
#include <vector>
#include <utility>
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

//https://stackoverflow.com/questions/30166706/c-convert-simple-values-to-string
template<typename T>
inline
typename std::enable_if<std::is_fundamental<T>::value, std::string>::type stringify_hack(const T& t)
{
    return std::to_string(t);
}

template<typename T>
inline
typename std::enable_if<!std::is_fundamental<T>::value, std::string>::type  stringify_hack(const T& t)
{
    return std::string(t);
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

    template<typename T>
    void set_prop(const std::string& key, const T& value)
    {
        item_properties[key] = stringify_hack(value);
    }

    void generate_set_id()
    {
        int32_t id = get_new_id();

        set_prop("item_id", id);
    }

    bool has_id()
    {
        return item_properties.find("item_id") != item_properties.end();
    }

    bool exists_in_db()
    {
        if(!has_id())
            return true;

        std::string prop = get_prop("item_id");
        bson_t* to_find = BCON_NEW("item_id", BCON_UTF8(prop.c_str()));

        mongo_context* ctx = get_global_mongo_user_items_context();

        bool exists = ctx->find_bson("all_items", to_find, nullptr).size() > 0;

        bson_destroy(to_find);

        return exists;
    }

    ///kinda need to upsert this
    void create_in_db()
    {
        if(!has_id())
            return;

        bson_t* to_insert = bson_new();

        for(auto& i : item_properties)
        {
            BSON_APPEND_UTF8(to_insert, i.first.c_str(), i.second.c_str());
        }

        mongo_context* ctx = get_global_mongo_user_items_context();

        ctx->insert_bson_1("all_items", to_insert);

        bson_destroy(to_insert);
    }

    void update_in_db()
    {
        if(!has_id())
            return;

        bson_t* to_insert = bson_new();

        {
            bson_t child;

            BSON_APPEND_DOCUMENT_BEGIN(to_insert, "$set", &child);

            for(auto& i : item_properties)
            {
                bson_append_utf8(&child, i.first.c_str(), i.first.size(), i.second.c_str(), i.second.size());
            }

            bson_append_document_end(to_insert, &child);
        }

        mongo_context* ctx = get_global_mongo_user_items_context();

        std::string prop = get_prop("item_id");
        bson_t* to_find = BCON_NEW("item_id", BCON_UTF8(prop.c_str()));

        ctx->update_bson_many("all_items", to_find, to_insert);

        bson_destroy(to_find);
        bson_destroy(to_insert);
    }

    void load_from_db()
    {
        if(!has_id())
            return;

        std::string prop = get_prop("item_id");
        bson_t* to_find = BCON_NEW("item_id", BCON_UTF8(prop.c_str()));

        mongo_context* ctx = get_global_mongo_user_items_context();

        std::vector<std::string> strs = ctx->find_bson("all_items", to_find, nullptr);

        bson_destroy(to_find);

        for(auto& i : strs)
        {
            bson_t* next = bson_new_from_json((const uint8_t*)i.c_str(), i.size(), nullptr);

            bson_iter_t iter;
            bson_iter_init(&iter, next);

            while (bson_iter_next (&iter))
            {
                std::string key = bson_iter_key(&iter);

                if(!BSON_ITER_HOLDS_UTF8(&iter))
                    continue;

                uint32_t len = bson_iter_utf8_len_unsafe(&iter);

                std::string value = bson_iter_utf8(&iter, &len);

                set_prop(key, value);
            }
        }
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

        return next_id;
    }
};

/*template<>
void item::set_prop<std::string>(const std::string& key, const std::string& value)
{
    item_properties[key] = value;
}

template<>
void item::set_prop<char const*>(const std::string& key, const char* const& value)
{
    item_properties[key] = value;
}*/

/*template<>
void item::set_prop<char*>(const std::string& key, const (char*)& value)
{
    item_properties[key] = value;
}*/

namespace item_types
{
/*item get_default_of(item_types::item_type type)
{
    item new_item;
    new_item.generate_set_id();


}*/
}

#endif // ITEMS_HPP_INCLUDED
