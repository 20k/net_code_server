#ifndef DB_INTERFACEABLE_HPP_INCLUDED
#define DB_INTERFACEABLE_HPP_INCLUDED

#include <map>
#include <json/json.hpp>
#include "mongo.hpp"
#include "global_caching.hpp"

using json = nlohmann::json;

///ok
///lets handle this like the serialisation lib i have
///except we need it to also be able to do something like on_first_initialise a variable
///so we can handle changing schemas
///and we also need the ability to handle variable schemas
///aka where an item might simple load and store a whole datadump

///so this gives us two classes of objects
///strictly typed, same every time
///and the rest of everything we found
///when we write to the db, we need to update both
///so in practice, the strictly typed variables should pass through to the key/value store

struct db_common
{
    std::string key;

    db_common(const std::string& _key) : key(_key) {}

    //db_common(const std::string& fkey) : key(fkey){}

    //virtual void extract_from(json& j){}

    virtual void serialise(json& j, bool ser) {}

    virtual ~db_common(){}
};

/*template<typename T>
struct db_val : db_common
{
    T val;

    template<typename U>
    bool handle_serialise(json& j, U on_create, bool ser)
    {
        ///if this doesnt exist
        ///return dirty, and do on create
        ///and then update json
        if(ser)
        {
            j[key] = val;
        }
        else
        {
            if(j.count(key) == 0)
            {
                val = on_create(j);
                j[key] = val;
                return true;
            }
            ///if it does exist, get it from the key location
            else
            {
                val = j[key].get<T>();
                return false;
            }
        }
    }

    virtual ~db_val(){}
};*/

template<typename T>
struct db_val : db_common
{
    T val = T();
    //U* u = nullptr;

    db_val(const std::string& _key) : db_common(_key) {}

    //void init_impl_ptr(U* ptr) {u = ptr;}

    /*void operator=(const db_val<T>& other)
    {
        if(this == &other)
            return;

        *this = other;

        //u->template set_as<T>(key, other.u->template get_as<T>(other.key));
    }*/

    virtual void serialise(json& j, bool ser) override
    {
        if(ser)
        {
            j[key] = val;
        }
        else
        {
            val = j[key].get<T>();
        }
    }

    template<typename V>
    void operator=(const V& other)
    {
        //u->template set_as<T>(key, other);
        val = other;
    }

    operator T&() {return val;};
    //operator T&() = delete;
    operator T() const { return val; }
    //operator T() const { return u->template get_as<T>(key); }
};

#define DB_VAL(type, name) db_val<type> name{#name}

#define MACRO_GET_1(str, i) \
    (sizeof(str) > (i) ? str[(i)] : 0)

#define MACRO_GET_4(str, i) \
    MACRO_GET_1(str, i+0),  \
    MACRO_GET_1(str, i+1),  \
    MACRO_GET_1(str, i+2),  \
    MACRO_GET_1(str, i+3)

#define MACRO_GET_16(str, i) \
    MACRO_GET_4(str, i+0),   \
    MACRO_GET_4(str, i+4),   \
    MACRO_GET_4(str, i+8),   \
    MACRO_GET_4(str, i+12)

#define MACRO_GET_64(str, i) \
    MACRO_GET_16(str, i+0),  \
    MACRO_GET_16(str, i+16), \
    MACRO_GET_16(str, i+32), \
    MACRO_GET_16(str, i+48)

#define MACRO_GET_STR(str) MACRO_GET_64(str, 0), 0 //guard for longer strings

inline
void stringify_params(std::string& in)
{

}

template<typename U, typename... T>
inline
void stringify_params(std::string& in, U first, T... name)
{
    if(first == '\0')
        return;

    in += first;

    stringify_params(in, name...);
}

struct mongo_lock_proxy;

template<typename concrete, bool cacheable, char... name>
struct db_interfaceable
{
    std::string key_name;
    json data;

    static inline global_generic_cache<concrete> this_cache;

    db_interfaceable()
    {
        stringify_params(key_name, name...);
    }

    virtual void serialise(bool ser){}

    template<typename T, typename U>
    bool force_conversion(const std::string& key, U cv)
    {
        try
        {
            auto found = data[key].get<T>();

            data[key] = cv(found);

            return true;
        }
        catch(...)
        {
            return false;
        }
    }

    bool has(const std::string& key)
    {
        return data.count(key) > 0;
    }

    template<typename T>
    T get_as(const std::string& key)
    {
        return data[key].get<T>();
    }

    template<typename T>
    void set_as(const std::string& key, const T& t)
    {
        data[key] = t;
    }

    void set_key_data(const std::string& val)
    {
        data[key_name] = val;
    }

    std::string get_key_data()
    {
        return data[key_name].get<std::string>();
    }

    virtual bool handle_serialise(json& j, bool ser) {return false;}

    static
    std::vector<concrete> fetch_all_from_db(mongo_lock_proxy& ctx)
    {
        std::string static_key;
        stringify_params(static_key, name...);

        json exist;
        exist["$exists"] = true;

        json to_find;
        to_find[static_key] = exist;

        auto found = ctx->find_json(ctx->last_collection, to_find.dump(), "{}");

        std::vector<concrete> ret;

        for(auto& js : found)
        {
            try
            {
                concrete val;
                val.data = json::parse(js);

                val.handle_serialise(val.data, false);

                ret.push_back(val);
            }
            catch(...){}
        }

        return ret;
    }

    ///need a fetch all from db
    ///just returns a vector, and checks for $exists : id
    bool load_from_db(mongo_lock_proxy& ctx, const std::string& id)
    {
        data[key_name] = id;

        if(!exists(ctx, id))
            return false;

        if(cacheable && this_cache.exists_in_cache(id))
        {
            *this = this_cache.load_from_cache(id);

            if(cacheable)
            {
                this_cache.overwrite_in_cache(id, *(concrete*)this);
            }

            if(handle_serialise(data, false))
            {
                overwrite_in_db(ctx);
            }

            return true;
        }

        json to_find;
        to_find[key_name] = id;

        try
        {
            auto found = ctx->find_json(ctx->last_collection, to_find.dump(), "{}");

            if(found.size() != 1)
                return false;

            std::string js = found[0];

            data = json::parse(js);

            if(cacheable)
            {
                this_cache.overwrite_in_cache(id, *(concrete*)this);
            }

            if(handle_serialise(data, false))
            {
                overwrite_in_db(ctx);
            }

            return true;
        }
        catch(...)
        {
            return false;
        }

        return false;
    }

    void overwrite_in_db(mongo_lock_proxy& ctx)
    {
        handle_serialise(data, true);

        if(!exists(ctx, data[key_name].get<std::string>()))
        {
            if(cacheable)
            {
                this_cache.overwrite_in_cache(data[key_name].get<std::string>(), *(concrete*)this);
            }

            ///insert
            ctx->insert_json_1(ctx->last_collection, data.dump());
        }
        else
        {
            if(cacheable)
            {
                this_cache.overwrite_in_cache(data[key_name].get<std::string>(), *(concrete*)this);
            }

            ///overwrite
            json selector;
            selector[key_name] = data[key_name];

            //std::cout << "selector " << selector.dump() << " data " << data.dump() << std::endl;

            json to_set;
            to_set["$set"] = data;

            ctx->update_json_many(ctx->last_collection, selector.dump(), to_set.dump());
        }
    }

    void remove_from_db(mongo_lock_proxy& ctx)
    {
        handle_serialise(data, false);

        json j;
        j[key_name] = data[key_name];

        ctx->remove_json(ctx->last_collection, j.dump());

        if(cacheable)
        {
            this_cache.delete_from_cache(data[key_name].get<std::string>());
        }
    }

    bool exists(mongo_lock_proxy& ctx, const std::string& id)
    {
        if(cacheable && this_cache.exists_in_cache(id))
            return true;

        json to_find;
        to_find[key_name] = id;

        auto found = ctx->find_json(ctx->last_collection, to_find.dump(), "{}");

        return found.size() >= 1;
    }

    virtual bool is_valid(){return true;}

    virtual ~db_interfaceable(){}
};

template<typename T>
std::vector<T> from_string(const std::vector<std::string>& in)
{
    std::vector<T> ret;

    for(auto& i : in)
    {
        ret.push_back(atof(i.c_str()));
    }

    return ret;
}

struct test_dbable : db_interfaceable<test_dbable, false, MACRO_GET_STR("test_key")>
{

};

#endif // DB_INTERFACEABLE_HPP_INCLUDED
