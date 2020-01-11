#ifndef ARGUMENT_OBJECT_HPP_INCLUDED
#define ARGUMENT_OBJECT_HPP_INCLUDED

#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <tuple>
#include "duktape.h"
#include <assert.h>
#include <nlohmann/json.hpp>

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
            assert(false);

        duk_remove(ctx, tidx);
    }
};

using js_funcptr_t = duk_ret_t(*)(duk_context*);

namespace js
{
    struct undefined_t;
}

namespace arg
{
    void dukx_push(duk_context* ctx, const char* v);
    void dukx_push(duk_context* ctx, const std::string& v);
    void dukx_push(duk_context* ctx, int64_t v);
    void dukx_push(duk_context* ctx, int v);
    void dukx_push(duk_context* ctx, double v);
    void dukx_push(duk_context* ctx, bool v);
    template<typename T>
    void dukx_push(duk_context* ctx, const std::vector<T>& v);
    template<typename T, typename U>
    void dukx_push(duk_context* ctx, const std::map<T, U>& v);
    void dukx_push(duk_context* ctx, js_funcptr_t fptr);
    void dukx_push(duk_context* ctx, const js::undefined_t&);
    void dukx_push(duk_context* ctx, const nlohmann::json& in);

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

    inline
    void dukx_push(duk_context* ctx, bool v)
    {
        duk_push_boolean(ctx, v);
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
    void dukx_push(duk_context* ctx, js_funcptr_t fptr)
    {
        duk_push_c_function(ctx, fptr, DUK_VARARGS);
    }

    inline
    void dukx_push(duk_context* ctx, const js::undefined_t&)
    {
        duk_push_undefined(ctx);
    }

    inline
    void dukx_push(duk_context* ctx, const nlohmann::json& in)
    {
        std::string str = in.dump();

        duk_push_lstring(ctx, str.c_str(), str.size());
        duk_json_decode(ctx, -1);
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

    inline
    void dukx_get(duk_context* ctx, int idx, bool& out)
    {
        stack_dupper sdup(ctx, idx);
        out = duk_get_boolean(ctx, sdup.tidx);
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

            dukx_get(ctx, -1, out[i]);

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
    struct undefined_t{};
    const static inline undefined_t undefined;

    struct value_context
    {
        std::vector<int> free_stack;
        context_t* ctx = nullptr;
        value_context* parent_context = nullptr;
        int parent_idx = -1;

        value_context(context_t* ctx);
        value_context(value_context& in);
        ~value_context();

        void free(int idx);
    };

    struct value
    {
        value_context* vctx = nullptr;
        context_t* ctx = nullptr;
        int idx = -1;
        int parent_idx = -1;
        bool released = false;
        ///parent index
        std::variant<std::monostate, int, std::string> indices;

        ///pushes a fresh object
        value(const value& other);
        value(value&& other);
        value(value_context& ctx);
        value(value_context& ctx, const value& other);
        value(value_context& ctx, int idx);
        value(value_context& ctx, const value& base, const std::string& key);
        value(value_context& ctx, const value& base, int key);
        value(value_context& ctx, const value& base, const char* key);
        ~value();

        bool has(const std::string& key) const;
        bool has(int key) const;
        bool has(const char* key) const;
        bool has_hidden(const std::string& key) const;

        value get(const std::string& key);
        value get(int key);
        value get(const char* key);
        value get_hidden(const std::string& key);

        bool del(const std::string& key);

        template<typename T>
        js::value add(const std::string& key, const T& val)
        {
            auto jval = js::value(*vctx, *this, key);
            jval = val;
            return jval;
        }

        template<typename T>
        js::value add_hidden(const std::string& key, const T& val)
        {
            auto jval = js::value(*vctx, *this, "\xFF" + key);
            jval = val;
            return jval;
        }

        bool is_string();
        bool is_number();
        bool is_array();
        bool is_map();
        bool is_empty();
        bool is_function();
        bool is_boolean();
        bool is_undefined();
        bool is_truthy();
        bool is_object_coercible();

        ///stop managing element
        void release();

        value& operator=(const char* v);
        value& operator=(const std::string& v);
        value& operator=(int64_t v);
        value& operator=(int v);
        value& operator=(double v);
        value& operator=(bool v);
        value& operator=(std::nullopt_t v);
        value& operator=(const value& right);
        value& operator=(value&& right);
        value& operator=(undefined_t);
        value& operator=(const nlohmann::json&);

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

        value& operator=(js_funcptr_t fptr);

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

        operator bool()
        {
            bool ret;
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

        std::string to_json();

        value operator[](int64_t val);
        value operator[](const std::string& str);
        value operator[](const char* str);

        void pack();

        friend bool operator==(const value& v1, const value& v2)
        {
            return duk_equals(v1.ctx, v1.idx, v2.idx);
        }
    };

    template<typename... T>
    inline
    std::pair<bool, value> call(value& func, T&&... vals)
    {
        bool all_same = ((vals.ctx == func.ctx) && ...);

        if(!all_same)
            throw std::runtime_error("Not same contexts");

        duk_dup(func.ctx, func.idx);

        (duk_dup(func.ctx, vals.idx), ...);

        int num = sizeof...(vals);

        bool success = duk_pcall(func.ctx, num) == 0;

        return {success, js::value(*func.vctx, -1)};
    }

    template<typename I, typename... T>
    inline
    std::pair<bool, value> call_prop(value& obj, const I& key, T&&... vals)
    {
        js::value func = obj[key];

        return call(func, std::forward<T>(vals)...);
    }

    template<typename T, typename... U>
    inline
    constexpr bool is_first_context()
    {
        return std::is_same_v<T, js::value_context*>;
    }

    template<typename T, typename... U>
    inline
    constexpr int num_args(T(*fptr)(U...))
    {
        if constexpr(is_first_context<U...>())
            return sizeof...(U) - 1;
        else
            return sizeof...(U);
    }

    template<typename T, typename... U>
    inline
    constexpr int num_rets(T(*fptr)(U...))
    {
        return !std::is_same_v<void, T>;
    }

    template<typename T, int N>
    inline
    T get_element(js::value_context& vctx, int stack_base)
    {
        constexpr bool is_first_value = std::is_same_v<T, js::value_context*> && N == 0;

        if constexpr(is_first_value)
        {
            return &vctx;
        }

        if constexpr(!is_first_value)
        {
            js::value val(vctx, stack_base + N);

            if constexpr(!std::is_same_v<T, js::value>)
                val.release();

            return val;
        }
    }

    template<typename... U, std::size_t... Is>
    inline
    std::tuple<U...> get_args(js::value_context& vctx, std::index_sequence<Is...>, int stack_base)
    {
        return std::make_tuple(get_element<U, Is>(vctx, stack_base)...);
    }

    template<typename T, typename... U>
    inline
    duk_ret_t js_safe_function_decomposed(duk_context* ctx, void* udata, T(*func)(U...))
    {
        js::value_context vctx(ctx);

        int stack_offset = sizeof...(U);

        int stack_base = duk_get_top(ctx) - stack_offset;

        std::index_sequence_for<U...> iseq;

        auto tup = get_args<U...>(vctx, iseq, stack_base);

        if constexpr(std::is_same_v<void, T>)
        {
            std::apply(func, tup);
            return 0;
        }
        else
        {
            auto rval = std::apply(func, tup);

            if constexpr(std::is_same_v<T, js::value>)
            {
                rval.release();
                return 1;
            }
            else
            {
                js::value v(vctx);
                v = rval;

                v.release(); ///intentionally leave on duktape stack

                return 1;
            }
        }
    }

    template<auto func>
    inline
    duk_ret_t js_safe_function(duk_context* ctx, void* udata)
    {
        return js_safe_function_decomposed(ctx, udata, func);
    }

    template<auto func>
    inline
    duk_ret_t js_decompose(duk_context* ctx)
    {
        int nargs = num_args(func);
        int nrets = num_rets(func);

        int top = duk_get_top(ctx);

        for(int i=top; i < nargs; i++)
        {
            duk_push_undefined(ctx);
        }

        if(duk_safe_call(ctx, &js_safe_function<func>, nullptr, nargs, nrets) != DUK_EXEC_SUCCESS)
        {
            js::value_context vctx(ctx);
            js::value vc(vctx, -1);
            vc.release();

            throw std::runtime_error("Bad function call for duktape " + (std::string)vc);
        }

        return nrets;
    }

    template<auto func>
    inline
    duk_ret_t function(duk_context* ctx)
    {
        return js_decompose<func>(ctx);
    }

    js::value get_global(value_context& vctx);
    void set_global(value_context& vctx, const js::value& val);
    js::value get_current_function(value_context& vctx);
    js::value get_this(value_context& vctx);
    js::value get_heap_stash(value_context& vctx);
    js::value get_global_stash(value_context& vctx);
    void* get_sandbox_data_impl(value_context& vctx);

    template<typename T>
    inline
    T* get_sandbox_data(value_context& vctx)
    {
        return (T*)get_sandbox_data_impl(vctx);
    }

    template<typename T>
    inline
    js::value make_value(js::value_context& vctx, const T& t)
    {
        js::value v(vctx);
        v = t;
        return v;
    }

    ///this is a convention, not a formal type
    template<typename T>
    inline
    js::value make_error(js::value_context& vctx, const T& msg)
    {
        js::value v(vctx);
        v["ok"] = false;
        v["msg"] = msg;

        return v;
    }

    inline
    js::value make_success(js::value_context& vctx)
    {
        js::value v(vctx);
        v["ok"] = true;

        return v;
    }

    template<typename T, typename U>
    inline
    js::value add_key_value(js::value& base, const T& key, const U& val)
    {
        assert(base.vctx);

        js::value nval(*base.vctx, base, key);
        nval = val;
        return nval;
    }

    std::pair<bool, js::value> compile(js::value_context& vctx, const std::string& data);
    js::value eval(js::value_context& vctx, const std::string& data);
    js::value xfer_between_contexts(js::value_context& destination, const js::value& val);

    void dump_stack(js::value_context& vctx);
}
#endif // ARGUMENT_OBJECT_HPP_INCLUDED
