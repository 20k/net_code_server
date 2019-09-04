#ifndef DB_INTERFACEABLE_HPP_INCLUDED
#define DB_INTERFACEABLE_HPP_INCLUDED

#include <map>
#include <nlohmann/json.hpp>
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

template<typename T>
struct db_val
{
    std::string key;
    T val = T();

    db_val(const std::string& _key) : key(_key) {}

    template<typename U = T>
    void serialise(json& j, bool ser)
    {
        if(ser)
        {
            j[key] = (U)val;
        }
        else
        {
            if(j.find(key) == j.end())
                j[key] = (U)val;

            try
            {
                val = j[key].template get<U>();
            }
            catch(const std::exception& err)
            {
                std::cout << "RUH ROH " << err.what() << std::endl;

                j[key] = (U)val;
            }
        }
    }

    template<typename V>
    db_val<T>& operator=(const V& other)
    {
        val = other;

        return *this;
    }

    T& value() {return val;};
    T value() const {return val;};

    operator T&() {return val;};
    //operator T&() = delete;
    operator T() const { return val; }
    //operator T() const { return u->template get_as<T>(key); }

    T* operator->()
    {
        return &val;
    }

    T& operator*()
    {
        return val;
    }

    T& get()
    {
        return val;
    }
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

template<typename concrete, char... name>
struct db_interfaceable
{
    std::string key_name;
    json data;

    db_interfaceable()
    {
        stringify_params(key_name, name...);
        //std::cout << "key " << key_name << " len " << key_name.size() << std::endl;
    }

    virtual void serialise(bool ser){}

    bool has(const std::string& key)
    {
        return data.count(key) > 0;
    }

    template<typename T>
    T get_as(const std::string& key)
    {
        return data[key].template get<T>();
    }

    template<typename T>
    void set_as(const std::string& key, const T& t)
    {
        if constexpr(std::is_same_v<bool, T>)
            data[key] = (int)t;
        else
            data[key] = t;
    }

    nlohmann::json get(const std::string& key)
    {
        if(!has(key))
            return nlohmann::json();

        return data[key];
    }

    std::string get_stringify(const std::string& key)
    {
        json j = data[key];

        if(j.type() == json::value_t::string)
        {
            return j.get<std::string>();
        }

        return j.dump();
    }

    void set_key_data(const std::string& val)
    {
        data[key_name] = val;
    }

    std::string get_key_data()
    {
        return data[key_name].template get<std::string>();
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

        auto found = ctx->find_json_new(to_find, nlohmann::json());

        std::vector<concrete> ret;

        for(auto& js : found)
        {
            try
            {
                concrete val;
                val.data = js;

                val.handle_serialise(val.data, false);

                ret.push_back(val);
            }
            catch(...){}
        }

        return ret;
    }

    static
    std::vector<concrete> convert_all_from(const std::vector<nlohmann::json>& found)
    {
        std::vector<concrete> ret;

        for(auto& js : found)
        {
            try
            {
                concrete val;
                val.data = js;

                val.handle_serialise(val.data, false);

                ret.push_back(val);
            }
            catch(...){}
        }

        return ret;
    }

    static
    void remove_all_from_db(mongo_lock_proxy& ctx)
    {
        std::string static_key;
        stringify_params(static_key, name...);

        json exist;
        exist["$exists"] = true;

        json to_find;
        to_find[static_key] = exist;

        ctx->remove_json_many_new(to_find);
    }

    ///need a fetch all from db
    ///just returns a vector, and checks for $exists : id
    bool load_from_db(mongo_lock_proxy& ctx, const std::string& id)
    {
        data[key_name] = id;

        if(!exists(ctx, id))
            return false;

        json to_find;
        to_find[key_name] = id;

        try
        {
            auto found = ctx->find_json_new(to_find, nlohmann::json());

            if(found.size() != 1)
                return false;

            data = found[0];

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

        try{

        if(!exists(ctx, data[key_name].template get<std::string>()))
        {
            ///insert
            ctx->insert_json_one_new(data);
        }
        else
        {
            ///overwrite
            json selector;
            selector[key_name] = data[key_name];

            //std::cout << "selector " << selector.dump() << " data " << data.dump() << std::endl;

            json to_set;
            to_set["$set"] = data;

            ctx->update_json_one_new(selector, to_set);
        }
        }
        catch(...)
        {
            std::cout << "Oopsie\n";

            std::cout << data.dump() << std::endl;
        }
    }

    void remove_from_db(mongo_lock_proxy& ctx)
    {
        handle_serialise(data, false);

        json j;
        j[key_name] = data[key_name];

        ctx->remove_json_many_new(j);
    }

    static
    void remove_from_db(mongo_lock_proxy& ctx, const std::string& id)
    {
        std::string static_string;

        stringify_params(static_string, name...);

        json j;
        j[static_string] = id;

        ctx->remove_json_many_new(j);
    }

    bool exists(mongo_lock_proxy& ctx, const std::string& id)
    {
        json to_find;

        to_find[key_name] = id;

        auto found = ctx->find_json_new(to_find, nlohmann::json());

        return found.size() >= 1;
    }

    virtual ~db_interfaceable(){}
};

#endif // DB_INTERFACEABLE_HPP_INCLUDED
