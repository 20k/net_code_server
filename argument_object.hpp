#ifndef ARGUMENT_OBJECT_HPP_INCLUDED
#define ARGUMENT_OBJECT_HPP_INCLUDED

#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <tuple>
#include "duktape.h"
#include <assert.h>

#include "scripting_api_fwrd.hpp"

using context_t = duk_context;

#if 0
struct value_object
{
    std::variant<std::string,
    int64_t,
    double,
    std::vector<value_object>,
    std::map<value_object, value_object>,
    std::tuple<context_t*, int>,
    std::function<value_object(value_object&)>> var;

    value_object(){}

    template<typename T>
    value_object(T&& t)
    {
        *this = t;
    }

    value_object& operator=(const char* v);
    value_object& operator=(const std::string& v);
    value_object& operator=(int64_t v);
    value_object& operator=(double v);
    value_object& operator=(const std::vector<value_object>& v);
    value_object& operator=(const std::map<value_object, value_object>& v);
    value_object& operator=(std::function<value_object(value_object&)> v);
    //value_object& operator=(context_t* v, int idx);

    operator std::string();
    operator int64_t();
    operator double();
    operator std::vector<value_object>();
    operator std::map<value_object, value_object>();

    value_object& operator[](int64_t val);
    value_object& operator[](const std::string& str);

    /*friend bool operator<(const value_object& l, const value_object& r)
    {
        return l.var < r.var;
    }*/

    friend bool operator<(const value_object& l, const value_object& r);
};

struct argument_object
{
    context_t* ctx = nullptr;

    argument_object(context_t* in);
};
#endif // 0

namespace js
{
    struct value;
}

struct stack_manage
{
    js::value& sh;
    stack_manage(js::value& in);
    ~stack_manage();
};

struct stack_dupper
{
    duk_context* ctx;
    int idx;
    duk_idx_t tidx;

    stack_dupper(duk_context* _ctx, int _idx) : ctx(_ctx), idx(_idx)
    {
        duk_dup(ctx, idx);
        tidx = duk_get_top_index(ctx);
    }

    ~stack_dupper()
    {
        if(tidx != duk_get_top_index(ctx))
            throw std::runtime_error("BAD STACK DUP");

        duk_remove(ctx, tidx);
    }
};

namespace arg
{
    inline
    void dukx_push(duk_context* ctx, const char* v)
    {
        duk_push_string(ctx, v);
    }

    inline
    void dukx_push(duk_context* ctx, const std::string& v)
    {
        duk_push_lstring(ctx, v.c_str(), v.size());
    }

    inline
    void dukx_push(duk_context* ctx, int64_t v)
    {
        if(v <= INT_MAX && v >= INT_MIN)
            duk_push_int(ctx, v);
        else
            duk_push_number(ctx, v);
    }

    inline
    void dukx_push(duk_context* ctx, int v)
    {
        duk_push_int(ctx, v);
    }

    inline
    void dukx_push(duk_context* ctx, double v)
    {
        duk_push_number(ctx, v);
    }

    template<typename T>
    inline
    void dukx_push(duk_context* ctx, const std::vector<T>& v)
    {
        duk_idx_t tidx = duk_push_array(ctx);

        for(int i=0; i < (int)v.size(); i++)
        {
            dukx_push(ctx, v[i]);
            duk_put_prop_index(ctx, tidx, i);
        }
    }

    template<typename T, typename U>
    inline
    void dukx_push(duk_context* ctx, const std::map<T, U>& v)
    {
        duk_idx_t tidx = duk_push_object(ctx);

        for(const auto& i : v)
        {
            dukx_push(ctx, i.first);
            dukx_push(ctx, i.second);

            duk_put_prop(ctx, tidx);
        }
    }

    inline
    void dukx_get(duk_context* ctx, int idx, std::string& out)
    {
        stack_dupper sdup(ctx, idx);

        duk_dup(ctx, sdup.tidx);

        duk_size_t flen = 0;
        const char* ptr = duk_safe_to_lstring(ctx, -1, &flen);

        if(ptr == nullptr || flen == 0)
        {
            duk_pop(ctx);
            return;
        }

        out = std::string(ptr, flen);
        duk_pop(ctx);
    }

    inline
    void dukx_get(duk_context* ctx, int idx, int64_t& out)
    {
        stack_dupper sdup(ctx, idx);
        out = duk_get_number(ctx, sdup.tidx);
    }

    inline
    void dukx_get(duk_context* ctx, int idx, int& out)
    {
        stack_dupper sdup(ctx, idx);
        out = duk_get_int(ctx, sdup.tidx);
    }

    inline
    void dukx_get(duk_context* ctx, int idx, double& out)
    {
        stack_dupper sdup(ctx, idx);
        out = duk_get_number(ctx, sdup.tidx);
    }

    template<typename T>
    inline
    void dukx_get(duk_context* ctx, int idx, std::vector<T>& out)
    {
        stack_dupper sdup(ctx, idx);

        duk_size_t arrsizet = duk_get_length(ctx, sdup.tidx);

        out.resize(arrsizet);

        for(int i=0; i < arrsizet; i++)
        {
            duk_get_prop_index(ctx, sdup.tidx, i);

            out[i] = dukx_get<T>(ctx, -1);

            duk_pop(ctx);
        }
    }

    template<typename T, typename U>
    inline
    void dukx_get(duk_context* ctx, int idx, std::map<T, U>& out)
    {
        stack_dupper sdup(ctx, idx);

        duk_enum(ctx, sdup.tidx, 0);

        while(duk_next(ctx, -1, 1))
        {
            T key;
            dukx_get(ctx, -2, key);
            U val;
            dukx_get(ctx, -1, val);

            out[key] = val;

            duk_pop_2(ctx);
        }

        duk_pop(ctx);
    }
}

namespace js
{
    struct value_context
    {
        std::vector<int> free_stack;
        context_t* ctx = nullptr;

        value_context(context_t* ctx);

        void free(int idx);
        //bool is_free(int idx);
    };

    ///ok so
    ///kind of fucked this entirely, because using absolute stack indices
    ///problem is... they shuffle around
    struct value
    {
        value_context* vctx = nullptr;
        context_t* ctx = nullptr;
        int idx = -1;
        int parent_idx = -1;
        ///parent index
        std::variant<std::monostate, int, std::string> indices;

        ///pushes a fresh object
        value(const value& other);
        value(value_context& ctx);
        value(value_context& ctx, int idx);
        value(value_context& ctx, value& base, const std::string& key);
        value(value_context& ctx, value& base, int key);
        value(value_context& ctx, value& base, const char* key);
        ~value();

        bool has(const std::string& key);
        bool has(int key);
        bool has(const char* key);

        bool is_string();
        bool is_number();
        bool is_array();
        bool is_map();
        bool is_empty();
        bool is_function();

        value& operator=(const char* v);
        value& operator=(const std::string& v);
        value& operator=(int64_t v);
        value& operator=(int v);
        value& operator=(double v);
        value& operator=(std::nullopt_t v);
        value& operator=(const value& right);

        template<typename T>
        value& operator=(const std::vector<T>& in)
        {
            stack_manage m(*this);

            arg::dukx_push(ctx, in);

            return *this;
        }

        template<typename T, typename U>
        value& operator=(const std::map<T, U>& in)
        {
            stack_manage m(*this);

            arg::dukx_push(ctx, in);

            return *this;
        }

        operator std::string()
        {
            std::string ret;
            arg::dukx_get(ctx, idx, ret);
            return ret;
        }

        operator int64_t()
        {
            int64_t ret;
            arg::dukx_get(ctx, idx, ret);
            return ret;
        }

        operator int()
        {
            int ret;
            arg::dukx_get(ctx, idx, ret);
            return ret;
        }

        operator double()
        {
            double ret;
            arg::dukx_get(ctx, idx, ret);
            return ret;
        }

        template<typename T>
        operator std::vector<T>()
        {
            std::vector<T> ret;
            arg::dukx_get(ctx, idx, ret);
            return ret;
        }

        template<typename T, typename U>
        operator std::map<T, U>()
        {
            std::map<T, U> ret;
            arg::dukx_get(ctx, idx, ret);
            return ret;
        }

        value operator[](int64_t val);
        value operator[](const std::string& str);
        value operator[](const char* str);

        friend bool operator==(const value& v1, const value& v2)
        {
            return duk_equals(v1.ctx, v1.idx, v2.idx);
        }
    };

    template<typename... T>
    inline
    std::pair<bool, value> call(value& func, T&&... vals)
    {
        duk_dup(func.ctx, func.idx);

        (duk_dup(func.ctx, vals.idx), ...);

        int num = sizeof...(vals);

        ///== 0 is success
        if(duk_pcall(func.ctx, num) == 0)
        {
            return {true, js::value(*func.vctx, -1)};
        }

        return {false, js::value(*func.vctx, -1)};
    }

    template<typename I, typename... T>
    inline
    std::pair<bool, value> call_prop(value& obj, const I& key, T&&... vals)
    {
        js::value func = obj[key];

        return call(func, std::forward<T>(vals)...);
    }
}
#endif // ARGUMENT_OBJECT_HPP_INCLUDED
