#ifndef ARGUMENT_OBJECT_QJS_HPP_INCLUDED
#define ARGUMENT_OBJECT_QJS_HPP_INCLUDED

#include "argument_object_common.hpp"

#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <tuple>
#include <assert.h>
#include <nlohmann/json.hpp>
#include <quickjs/quickjs.h>
#include "duktape.h"
#include "argument_object_common.hpp"

using quick_funcptr_t = JSCFunction;

namespace js_quickjs
{
    struct value;

    struct value_context
    {
        std::vector<value> this_stack;

        JSRuntime* heap = nullptr;
        JSContext* ctx = nullptr;
        bool runtime_owner = false;
        bool context_owner = false;

        value_context(JSContext* ctx);
        value_context(value_context&);
        value_context();
        ~value_context();

        void push_this(const value& val);
        void pop_this();
        value get_current_this();
    };
}

namespace qarg
{
    inline
    JSValue push(JSContext* ctx, const char* v)
    {
        return JS_NewString(ctx, v);
    }

    inline
    JSValue push(JSContext* ctx, const std::string& v)
    {
        return JS_NewStringLen(ctx, v.c_str(), v.size());
    }

    inline
    JSValue push(JSContext* ctx, int64_t v)
    {
        return JS_NewInt64(ctx, v);
    }

    inline
    JSValue push(JSContext* ctx, int v)
    {
        return JS_NewInt32(ctx, v);
    }

    inline
    JSValue push(JSContext* ctx, double v)
    {
        return JS_NewFloat64(ctx, v);
    }

    inline
    JSValue push(JSContext* ctx, bool v)
    {
        return JS_NewBool(ctx,v);
    }

    template<typename T>
    inline
    JSValue push(JSContext* ctx, const std::vector<T>& v)
    {
        JSValue val = JS_NewArray(ctx);

        for(int i=0; i < v.size(); i++)
        {
            JSValue found = push(ctx, v[i]);
            JS_SetPropertyUint32(ctx, val, i, found);
        }

        return val;
    }

    template<typename T, typename U>
    inline
    JSValue push(JSContext* ctx, const std::map<T, U>& v)
    {
        JSValue obj = JS_NewObject(ctx);

        for(auto& i : v)
        {
            JSValue key = push(ctx, i.first);
            JSValue val = push(ctx, i.second);

            JSAtom key_atom = JS_ValueToAtom(ctx, key);

            JS_SetProperty(ctx, obj, key_atom, val);

            JS_FreeAtom(ctx, key_atom);
            JS_FreeValue(ctx, key);
        }

        return obj;
    }

    JSValue push(JSContext* ctx, quick_funcptr_t fptr);

    inline
    JSValue push(JSContext* ctx, const js::undefined_t&)
    {
        return JS_UNDEFINED;
    }

    inline
    JSValue push(JSContext* ctx, const nlohmann::json& in)
    {
        std::string str = in.dump();

        return JS_ParseJSON(ctx, str.c_str(), str.size(), nullptr);
    }

    template<typename T>
    inline
    JSValue push(JSContext* ctx, T* in)
    {
        return JS_MKPTR(0, in);
    }

    inline
    JSValue push(JSContext* ctx, std::nullptr_t in)
    {
        return JS_MKPTR(0, 0);
    }

    JSValue push(JSContext* ctx, const js_quickjs::value& in);

    inline
    JSValue push(JSContext* ctx, quick_funcptr_t in)
    {
        return JS_NewCFunction(ctx, in, "", 0);
    }

    #define UNDEF() if(JS_IsUndefined(val)){out = std::remove_reference_t<decltype(out)>(); return;}

    void get(js_quickjs::value_context& vctx, const JSValue& val, std::string& out);
    void get(js_quickjs::value_context& vctx, const JSValue& val, int64_t& out);
    void get(js_quickjs::value_context& vctx, const JSValue& val, int& out);
    void get(js_quickjs::value_context& vctx, const JSValue& val, double& out);
    void get(js_quickjs::value_context& vctx, const JSValue& val, bool& out);
    template<typename T, typename U>
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::map<T, U>& out);
    template<typename T>
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::vector<T>& out);
    template<typename T>
    void get(js_quickjs::value_context& vctx, const JSValue& val, T*& out);
    void get(js_quickjs::value_context& vctx, const JSValue& val, js_quickjs::value& out);
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::vector<std::pair<js_quickjs::value, js_quickjs::value>>& out); ///equivalent to std::map
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::vector<js_quickjs::value>& out); ///equivalent to std::map

    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::string& out)
    {
        UNDEF();

        size_t len = 0;
        const char* str = JS_ToCStringLen(vctx.ctx, &len, val);

        if(str == nullptr)
        {
            out = "";
            return;
        }

        out = std::string(str, str + len);

        JS_FreeCString(vctx.ctx, str);
    }

    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, int64_t& out)
    {
        UNDEF();

        JS_ToInt64(vctx.ctx, &out, val);
    }

    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, int& out)
    {
        UNDEF();

        int32_t ival = 0;

        JS_ToInt32(vctx.ctx, &ival, val);
        out = ival;
    }

    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, double& out)
    {
        UNDEF();

        JS_ToFloat64(vctx.ctx, &out, val);
    }

    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, bool& out)
    {
        UNDEF();

        out = JS_ToBool(vctx.ctx, val) > 0;
    }

    template<typename T, typename U>
    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::map<T, U>& out)
    {
        UNDEF();

        JSPropertyEnum* names = nullptr;
        uint32_t len = 0;

        JS_GetOwnPropertyNames(vctx.ctx, &names, &len, val, JS_GPN_STRING_MASK|JS_GPN_SYMBOL_MASK);

        if(names == nullptr)
        {
            out.clear();
            return;
        }

        for(int i=0; i < len; i++)
        {
            JSAtom atom = names[i].atom;

            JSValue found = JS_GetProperty(vctx.ctx, val, atom);
            JSValue key = JS_AtomToValue(vctx.ctx, atom);

            T out_key;
            get(vctx, key, out_key);
            U out_value;
            get(vctx, found, out_value);

            out[out_key] = out_value;

            JS_FreeValue(vctx.ctx, found);
            JS_FreeValue(vctx.ctx, key);
        }

        for(int i=0; i < len; i++)
        {
            JS_FreeAtom(vctx.ctx, names[i].atom);
        }

        js_free(vctx.ctx, names);
    }

    template<typename T>
    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::vector<T>& out)
    {
        UNDEF();

        out.clear();

        int len = 0;
        JS_GetPropertyStr(vctx.ctx, val, "length");

        out.reserve(len);

        for(int i=0; i < len; i++)
        {
            JSValue found = JS_GetPropertyUint32(vctx.ctx, val, i);

            T next;
            get(vctx, found, next);

            out.push_back(next);

            JS_FreeValue(vctx.ctx, found);
        }
    }

    template<typename T>
    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, T*& out)
    {
        UNDEF();

        out = JS_VALUE_GET_PTR(val);
    }

    inline
    void get(js_quickjs::value_context& vctx, const JSValue& val, std::vector<std::pair<js_quickjs::value, js_quickjs::value>>& out);
}

namespace js_quickjs
{
    struct value;

    struct qstack_manager
    {
        value& val;

        qstack_manager(value& _val);
        ~qstack_manager();
    };

    struct value
    {
        value_context* vctx = nullptr;
        JSContext* ctx = nullptr;
        JSValue val = {};
        JSValue parent_value = {};
        bool has_value = false;
        bool has_parent = false;
        bool released = false;

        std::variant<std::monostate, int, std::string> indices;

        value(const value& other);
        //value(value&& other);
        ///pushes a fresh object
        value(value_context& ctx);
        value(value_context& ctx, const value& other);
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

        bool is_string();
        bool is_number();
        bool is_array();
        bool is_map();
        bool is_empty();
        bool is_function();
        bool is_boolean();
        bool is_undefined() const;
        bool is_truthy();
        bool is_object_coercible();
        bool is_object();

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
        //value& operator=(value&& right);
        value& operator=(js::undefined_t);
        value& operator=(const nlohmann::json&);

        template<typename T>
        value& operator=(const std::vector<T>& in)
        {
            qstack_manager m(*this);

            val = qarg::push(ctx, in);

            return *this;
        }

        template<typename T, typename U>
        value& operator=(const std::map<T, U>& in)
        {
            qstack_manager m(*this);

            val = qarg::push(ctx, in);

            return *this;
        }

        value& operator=(quick_funcptr_t fptr);
        value& operator=(const JSValue& val);

        value& set_ptr(std::nullptr_t)
        {
            qstack_manager m(*this);

            qarg::push(ctx, nullptr);

            return *this;
        }

        ///seems dangerous to let this be arbitrary, because its extremely rarely what you want
        template<typename T>
        value& set_ptr(T* in)
        {
            qstack_manager m(*this);

            qarg::push(ctx, in);

            return *this;
        }


        template<typename T>
        value& allocate_in_heap(T& in)
        {
            T* val = new T(in);
            return set_ptr(val);
        }

        template<typename T>
        T* get_ptr()
        {
            if(!has_value)
                throw std::runtime_error("No value in trying to get a pointer. This is dangerous");

            T* ret;
            qarg::get(*vctx, val, ret);
            return ret;
        }

        template<typename T>
        void free_in_heap()
        {
            if(!has_value)
                return;

            T* ptr = get_ptr<T>();

            if(ptr == nullptr)
                return;

            delete ptr;

            T* null = nullptr;
            set_ptr(null);
        }

        operator std::string();
        operator int64_t();
        operator int();
        operator double();
        operator bool();

        template<typename T>
        operator std::vector<T>()
        {
            if(!has_value)
                return std::vector<T>();

            std::vector<T> ret;
            qarg::get(*vctx, val, ret);
            return ret;
        }

        template<typename T, typename U>
        operator std::map<T, U>()
        {
            if(!has_value)
                return std::map<T, U>();

            std::map<T, U> ret;
            qarg::get(*vctx, val, ret);
            return ret;
        }

        template<typename T>
        operator T*()
        {
            if(!has_value)
                return nullptr;

            static_assert(!std::is_same_v<T, char const>, "Trying to get a const char* pointer out is almost certainly not what you want");

            T* ret;
            qarg::get(*vctx, val, ret);
            return ret;
        }

        value operator[](int64_t val);
        value operator[](const std::string& str);
        value operator[](const char* str);

        void pack(){}
        void stringify_parse();
        std::string to_json();
        nlohmann::json to_nlohmann(int stack_depth = 0);
    };

    template<typename T, typename... U>
    constexpr bool is_first_context()
    {
        return std::is_same_v<T, js_quickjs::value_context*>;
    }

    template<typename T, typename... U>
    constexpr int num_args(T(*fptr)(U...))
    {
        if constexpr(is_first_context<U...>())
            return sizeof...(U) - 1;
        else
            return sizeof...(U);
    }

    template<typename T, typename... U>
    constexpr int num_rets(T(*fptr)(U...))
    {
        return !std::is_same_v<void, T>;
    }

    template<typename T, int N>
    inline
    T get_element(js_quickjs::value_context& vctx, JSValueConst* argv)
    {
        constexpr bool is_first_value = std::is_same_v<T, js_quickjs::value_context*> && N == 0;

        if constexpr(is_first_value)
        {
            return &vctx;
        }

        if constexpr(!is_first_value)
        {
            return argv[N];
        }
    }

    template<typename... U, std::size_t... Is>
    inline
    std::tuple<U...> get_args(js_quickjs::value_context& vctx, std::index_sequence<Is...>, JSValueConst* argv)
    {
        return std::make_tuple(get_element<U, Is>(vctx, argv)...);
    }

    template<typename T, typename... U>
    inline
    JSValue js_safe_function_decomposed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst *argv, T(*func)(U...))
    {
        if(argc != num_args(func))
            throw std::runtime_error("Bad quickjs function");

        js_quickjs::value_context vctx(ctx);

        js_quickjs::value func_this(vctx);
        func_this = this_val;

        vctx.push_this(func_this);

        std::index_sequence_for<U...> iseq;

        auto tup = get_args<U...>(vctx, iseq, argv);

        if constexpr(std::is_same_v<void, T>)
        {
            std::apply(func, tup);

            js_quickjs::value val(vctx);
            val = js::undefined;

            val.release();

            vctx.pop_this();

            return val.val;
        }
        else
        {
            auto rval = std::apply(func, tup);

            if constexpr(std::is_same_v<rval, js_quickjs::value>)
            {
                rval.release();

                vctx.pop_this();

                return rval.val;
            }
            else
            {
                js_quickjs::value val(vctx);
                val = rval;
                val.release();

                vctx.pop_this();

                return val.val;
            }
        }
    }

    template<auto func>
    inline
    JSValue function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        return js_safe_function_decomposed(ctx, this_val, argc, argv, func);
    }

    //still need call

    js_quickjs::value get_global(value_context& vctx);
    void set_global(value_context& vctx, const js_quickjs::value& val);
    js_quickjs::value get_current_function(value_context& vctx);
    js_quickjs::value get_this(value_context& vctx);
    js_quickjs::value get_heap_stash(value_context& vctx);
    js_quickjs::value get_global_stash(value_context& vctx);
    void* get_sandbox_data_impl(value_context& vctx);

    template<typename T>
    inline
    T* get_sandbox_data(value_context& vctx)
    {
        return (T*)get_sandbox_data_impl(vctx);
    }

    value add_getter(value& base, const std::string& key, quick_funcptr_t func);
    value add_setter(value& base, const std::string& key, quick_funcptr_t func);

    std::pair<bool, value> compile(value_context& vctx, const std::string& data);
    std::pair<bool, value> compile(value_context& vctx, const std::string& name, const std::string& data);

    std::string dump_function(value& val);
    value eval(value_context& vctx, const std::string& data);
    value xfer_between_contexts(value_context& destination, const value& val);

    value make_proxy(value& target, value& handle);
    value from_cbor(value_context& vctx, const std::vector<uint8_t>& cb);

    void dump_stack(value_context& vctx);
}

#endif // ARGUMENT_OBJECT_QJS_HPP_INCLUDED
