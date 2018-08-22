#ifndef DUK_OBJECT_FUNCTIONS_HPP_INCLUDED
#define DUK_OBJECT_FUNCTIONS_HPP_INCLUDED

//#include <utility>
//#include <variant>
//#include <map>
#include <string>
#include <vector>

#include "scripting_api.hpp"
#include <json/json.hpp>

using duk_func_t = duk_ret_t (*)(duk_context*);
//using duk_placeholder_t = void*;
//using duk_variant_t = std::variant<bool, int, double, std::string, std::vector<std::string>, duk_placeholder_t, std::vector<duk_placeholder_t>>;
//using duk_object_t = std::map<std::string, duk_variant_t>;

void push_duk_val(duk_context* ctx, const duk_func_t& func);
void push_duk_val(duk_context* ctx, const bool& t);
void push_duk_val(duk_context* ctx, const int& t);
void push_duk_val(duk_context* ctx, const double& t);
void push_duk_val(duk_context* ctx, const std::string& t);
//void push_duk_val(duk_context* ctx, const duk_variant_t& t);
//void push_duk_val(duk_context* ctx, const duk_object_t& obj);
//void push_duk_val(duk_context* ctx, const duk_placeholder_t& t);

inline
void push_duk_val(duk_context* ctx, const duk_func_t& func)
{
    duk_push_c_function(ctx, func, 1);
}

template<int N>
inline
void push_duk_val(duk_context* ctx, const char (&arr)[N])
{
    duk_push_string(ctx, arr);
}

inline
void push_duk_val(duk_context* ctx, const bool& t)
{
    duk_push_boolean(ctx, t);
}

inline
void push_duk_val(duk_context* ctx, const int& t)
{
    duk_push_int(ctx, t);
}

inline
void push_duk_val(duk_context* ctx, const double& t)
{
    duk_push_number(ctx, t);
}

inline
void push_duk_val(duk_context* ctx, const std::string& t)
{
    duk_push_string(ctx, t.c_str());
}

inline
void push_duk_val(duk_context* ctx, const nlohmann::json& j)
{
    duk_push_string(ctx, j.dump().c_str());
    duk_json_decode(ctx, -1);
}

template<typename T>
inline
void push_duk_val(duk_context* ctx, const std::vector<T>& t)
{
    duk_idx_t arr_idx = duk_push_array(ctx);

    int index = 0;

    for(const auto& i : t)
    {
        push_duk_val(ctx, i);
        duk_put_prop_index(ctx, arr_idx, index);

        index++;
    }
}

/*inline
void push_duk_val(duk_context* ctx, const duk_variant_t& t)
{
    if(std::holds_alternative<bool>(t))
        return push_duk_val(ctx, std::get<bool>(t));

    if(std::holds_alternative<int>(t))
        return push_duk_val(ctx, std::get<int>(t));

    if(std::holds_alternative<double>(t))
        return push_duk_val(ctx, std::get<double>(t));

    if(std::holds_alternative<std::string>(t))
        return push_duk_val(ctx, std::get<std::string>(t));

    if(std::holds_alternative<std::vector<std::string>>(t))
        return push_duk_val(ctx, std::get<std::vector<std::string>>(t));

    if(std::holds_alternative<duk_placeholder_t>(t))
        return push_duk_val(ctx, std::get<duk_placeholder_t>(t));

    if(std::holds_alternative<std::vector<duk_placeholder_t>>(t))
        return push_duk_val(ctx, std::get<std::vector<duk_placeholder_t>>(t));
}*/

/*inline
void push_duk_val(duk_context* ctx, const duk_object_t& obj)
{
    duk_push_object(ctx);

    for(auto& prop : obj)
    {
        push_duk_val(ctx, prop.second);
        duk_put_prop_string(ctx, -2, prop.first.c_str());
    }
}*/

/*inline
void push_duk_val(duk_context* ctx, const duk_placeholder_t& t)
{
    ///found an object

    duk_object_t* obj = (duk_object_t*)t;

    push_duk_val(ctx, *obj);
}*/

template<typename T>
inline
T get_duk_val(duk_context* ctx)
{
    //static_assert(false);
}

template<>
inline
bool get_duk_val<bool>(duk_context* ctx)
{
    return duk_get_boolean(ctx, -1);
}

template<>
inline
int32_t get_duk_val<int32_t>(duk_context* ctx)
{
    return duk_get_int(ctx, -1);
}

template<>
inline
double get_duk_val<double>(duk_context* ctx)
{
    return duk_get_number(ctx, -1);
}

template<>
inline
std::string get_duk_val<std::string>(duk_context* ctx)
{
    return duk_get_string(ctx, -1);
}

template<typename T>
inline
std::vector<T> get_duk_val_arr(duk_context* ctx)
{
    std::vector<T> ret;

    int n = duk_get_length(ctx, -1);

    for(int i=0; i < n; i++)
    {
        duk_get_prop_index(ctx, -1, i);

        auto val = get_duk_val<T>(ctx);
        ret.push_back(val);

        duk_pop(ctx);
    }

    return ret;
}

template<typename U>
inline
void push_dukobject_impl(duk_context* ctx, const std::string& key, const U& u)
{
    push_duk_val(ctx, key);
    push_duk_val(ctx, u);

    duk_put_prop(ctx, -3);
}

inline
void push_dukobject_r(duk_context* ctx)
{

}

template<typename X, typename Y, typename... T>
inline
void push_dukobject_r(duk_context* ctx, const X& x, const Y& y, T&&... args)
{
    push_dukobject_impl(ctx, x, y);

    push_dukobject_r(ctx, args...);
}

template<typename... T>
inline
void push_dukobject(duk_context* ctx, T&&... args)
{
    duk_push_object(ctx);

    push_dukobject_r(ctx, args...);
}

template<typename T>
inline
void put_duk_keyvalue(duk_context* ctx, const std::string& key, const T& value)
{
    push_duk_val(ctx, value);
    duk_put_prop_string(ctx, -2, key.c_str());
}

template<typename T>
inline
bool get_duk_keyvalue(duk_context* ctx, const std::string& key, T& value)
{
    if(!duk_has_prop_string(ctx, -1, key.c_str()))
        return false;

    duk_get_prop_string(ctx, -1, key.c_str());
    ///value now on stack

    value = get_duk_val<T>(ctx);

    duk_pop(ctx);

    return true;
}

inline
duk_ret_t push_error(duk_context* ctx, const std::string& msg)
{
    push_dukobject(ctx, "ok", false, "msg", msg);
    return 1;
}

inline
duk_ret_t push_success(duk_context* ctx)
{
    push_dukobject(ctx, "ok", true);
    return 1;
}

inline
duk_ret_t push_success(duk_context* ctx, const std::string& msg)
{
    push_dukobject(ctx, "ok", true, "msg", msg);
    return 1;
}

inline
void freeze_duk(duk_context* ctx)
{
    duk_freeze(ctx, -1);
}

inline
std::string get_global_string(duk_context* ctx, const std::string& name)
{
    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, name.c_str());

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);

    return str;
}

inline
int32_t get_global_int(duk_context* ctx, const std::string& name)
{
    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, name.c_str());

    int32_t ret = duk_get_int(ctx, -1);

    duk_pop_n(ctx, 2);

    return ret;
}

inline
void set_global_int(duk_context* ctx, const std::string& name, int32_t val)
{
    duk_push_heap_stash(ctx);

    duk_push_int(ctx, val);
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop_n(ctx, 1);
}

inline
void set_global_number(duk_context* ctx, const std::string& name, double val)
{
    duk_push_heap_stash(ctx);

    duk_push_number(ctx, val);
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop_n(ctx, 1);
}

inline
double get_global_number(duk_context* ctx, const std::string& name)
{
    duk_push_heap_stash(ctx);

    duk_get_prop_string(ctx, -1, name.c_str());

    double val = duk_require_number(ctx, -1);

    duk_pop_n(ctx, 2);

    return val;
}

inline
void set_global_string(duk_context* ctx, const std::string& name, const std::string& val)
{
    duk_push_heap_stash(ctx);

    duk_push_string(ctx, val.c_str());
    duk_put_prop_string(ctx, -2, name.c_str());

    duk_pop_n(ctx, 1);
}

inline
int32_t get_thread_id(duk_context* ctx)
{
    return get_global_int(ctx, "thread_id");
}

inline
std::string get_caller(duk_context* ctx)
{
    duk_push_heap_stash(ctx);
    /*duk_get_prop_string(ctx, -1, "caller");

    std::string str = duk_safe_to_string(ctx, -1);

    duk_pop_n(ctx, 2);*/

    duk_get_prop_string(ctx, -1, "caller_stack");
    std::vector<std::string> ret = get_duk_val_arr<std::string>(ctx);

    duk_pop_n(ctx, 2);

    if(ret.size() == 0)
        return "";

    return ret.back();
}

inline
std::vector<std::string> get_caller_stack(duk_context* ctx)
{
    duk_push_heap_stash(ctx);

    duk_get_prop_string(ctx, -1, "caller_stack");
    std::vector<std::string> ret = get_duk_val_arr<std::string>(ctx);

    duk_pop_n(ctx, 2);

    return ret;
}

inline
std::string get_script_host(duk_context* ctx)
{
    return get_global_string(ctx, "script_host");
}

inline
std::string get_script_ending(duk_context* ctx)
{
    return get_global_string(ctx, "script_ending");
}

template<typename T>
inline
void dukx_put_pointer(duk_context* ctx, T* ptr, const std::string& key)
{
    duk_push_heap_stash(ctx); ///[stash]

    duk_push_pointer(ctx, (void*)ptr); ///[stash, pointer]

    duk_put_prop_string(ctx, -2, key.c_str());

    duk_pop(ctx);
}

inline
void dukx_put_pointer(duk_context* ctx, std::nullptr_t, const std::string& key)
{
    duk_push_heap_stash(ctx); ///[stash]

    duk_push_pointer(ctx, nullptr); ///[stash, pointer]

    duk_put_prop_string(ctx, -2, key.c_str());

    duk_pop(ctx);
}

template<typename T>
T* dukx_get_pointer(duk_context* ctx, const std::string& key)
{
    duk_push_heap_stash(ctx);

    duk_get_prop_string(ctx, -1, key.c_str());

    T* ptr = (T*)duk_get_pointer(ctx, -1);

    duk_pop_n(ctx, 2);

    return ptr;
}

template<typename T>
T* get_shim_pointer(duk_context* ctx)
{
    duk_push_heap_stash(ctx);

    duk_get_prop_string(ctx, -1, "c_shim_ptr");

    T* ptr = (T*)duk_get_pointer(ctx, -1);

    duk_pop_n(ctx, 2);

    return ptr;
}

template<typename T>
void set_copy_allocate_shim_pointer(duk_context* ctx, const T& t)
{
    T* new_shim = new T(t);

    duk_push_heap_stash(ctx); ///[stash]

    duk_push_pointer(ctx, (void*)new_shim); ///[stash, pointer]

    duk_put_prop_string(ctx, -2, "c_shim_ptr"); ///[stash]

    duk_pop(ctx); ///[]
}

template<typename T>
void free_shim_pointer(duk_context* ctx)
{
    T* ptr = get_shim_pointer<T>(ctx);

    delete ptr;
}

template<typename T>
T* get_shared_worker_state_ptr(duk_context* ctx)
{
    return dukx_get_pointer<T>(ctx, "shared_caller_state");
}

inline
std::string duk_safe_to_std_string(duk_context* ctx, duk_idx_t idx)
{
    duk_size_t out = 0;
    const char* ptr = duk_safe_to_lstring(ctx, idx, &out);

    if(ptr == nullptr || out == 0)
        return std::string();

    return std::string(ptr, out);
}

inline
std::string duk_safe_get_prop_string(duk_context* ctx, duk_idx_t idx, const std::string& key)
{
    if(duk_get_top(ctx) <= 0)
        return std::string();

    if(duk_is_undefined(ctx, idx))
        return std::string();

    if(!duk_has_prop_string(ctx, idx, key.c_str()))
        return std::string();

    duk_get_prop_string(ctx, idx, key.c_str());

    auto ret = duk_safe_to_std_string(ctx, -1);

    duk_pop(ctx);

    return ret;
}

inline
bool duk_safe_prop_valid(duk_context* ctx, duk_idx_t idx, const std::string& key)
{
    if(duk_get_top(ctx) <= 0)
        return false;

    if(duk_is_undefined(ctx, idx))
        return false;

    return true;
}

inline
int duk_get_prop_string_as_int(duk_context* ctx, duk_idx_t idx, const std::string& key, int def = 0)
{
    if(duk_get_top(ctx) <= 0)
        return def;

    if(duk_is_undefined(ctx, idx))
        return def;

    if(!duk_has_prop_string(ctx, idx, key.c_str()))
        return def;

    duk_get_prop_string(ctx, idx, key.c_str());

    auto ret = duk_get_int(ctx, -1);

    duk_pop(ctx);

    return ret;
}

template<typename T, typename U>
inline
auto duk_safe_get_generic(const U& func, duk_context* ctx, duk_idx_t idx, const std::string& key, const T& def = T()) -> decltype(func(ctx, -1))
{
    if(duk_get_top(ctx) <= 0)
        return def;

    if(duk_is_undefined(ctx, idx))
        return def;

    if(!duk_has_prop_string(ctx, idx, key.c_str()))
        return def;

    duk_get_prop_string(ctx, idx, key.c_str());

    auto ret = func(ctx, -1);

    duk_pop(ctx);

    return ret;
}

template<typename T, typename U, typename V>
inline
auto duk_safe_get_generic_with_guard(const U& func, const V& guard, duk_context* ctx, duk_idx_t idx, const std::string& key, const T& def = T()) -> decltype(func(ctx, -1))
{
    if(duk_get_top(ctx) <= 0)
        return def;

    if(duk_is_undefined(ctx, idx))
        return def;

    if(!duk_has_prop_string(ctx, idx, key.c_str()))
        return def;

    duk_get_prop_string(ctx, idx, key.c_str());

    if(!guard(ctx, -1))
    {
        duk_pop(ctx);
        return def;
    }

    auto ret = func(ctx, -1);

    duk_pop(ctx);

    return ret;
}

///so, the correct thing to do off the bat would have been to use a proper namespace
///or dukx
template<typename T>
inline
std::string dukx_json_get(const T& rep)
{
    duk_context* ctx = duk_create_heap_default();

    push_duk_val(ctx, rep);

    const char* ptr = duk_json_encode(ctx, -1);

    std::string str;

    if(ptr != nullptr)
    {
        str = std::string(ptr);
    }

    duk_pop(ctx);

    duk_destroy_heap(ctx);

    return str;
}

inline
nlohmann::json dukx_get_as_json(duk_context* ctx, int offset)
{
    duk_dup(ctx, offset);

    const char* ptr = duk_json_encode(ctx, -1);

    std::string str;

    if(ptr != nullptr)
        str = std::string(ptr);

    duk_pop(ctx);

    nlohmann::json j;

    try
    {
        j = nlohmann::json::parse(str);
    }
    catch(...)
    {

    }

    return j;
}

template<typename T>
inline
T dukx_get_as(duk_context* ctx, duk_idx_t idx)
{
    duk_dup(ctx, idx);

    const char* ptr = duk_json_encode(ctx, -1);

    if(ptr == nullptr)
    {
        duk_pop(ctx);
        return T();
    }

    nlohmann::json j = nlohmann::json::parse(ptr);

    return (T)j;
}

template<typename T>
inline
T dukx_get_prop_as(duk_context* ctx, duk_idx_t idx, const std::string& prop)
{
    duk_get_prop_string(ctx, idx, prop.c_str());

    const char* ptr = duk_json_encode(ctx, -1);

    if(ptr == nullptr)
    {
        duk_pop(ctx);
        return T();
    }

    nlohmann::json j = nlohmann::json::parse(ptr);

    return j;
}

inline
void quick_register(duk_context* ctx, const std::string& key, const std::string& value)
{
    duk_push_string(ctx, value.c_str());
    duk_put_prop_string(ctx, -2, key.c_str());
}

template<typename T>
inline
void quick_register_generic(duk_context* ctx, const std::string& key, const T& value)
{
    push_duk_val(ctx, value);
    duk_put_prop_string(ctx, -2, key.c_str());
}

inline
bool dukx_is_truthy(duk_context* ctx, duk_idx_t idx)
{
    duk_dup(ctx, idx);
    bool success = duk_to_boolean(ctx, -1);

    duk_pop(ctx);

    return success;
}

inline
bool dukx_is_prop_truthy(duk_context* ctx, duk_idx_t idx, const std::string& key)
{
    return duk_safe_get_generic(dukx_is_truthy, ctx, idx, key, false);
}

#define DUKX_HIDDEN_SYMBOL(x) (std::string("\xFF") + x)

inline
std::string dukx_get_hidden_prop_on_this(duk_context* ctx, const std::string& key)
{
    duk_push_current_function(ctx);
    //duk_push_this(ctx);

    duk_get_prop_string(ctx, -1, DUKX_HIDDEN_SYMBOL("HIDDEN_OBJ").c_str());
    duk_get_prop_string(ctx, -1,  key.c_str());

    std::string str = duk_safe_to_std_string(ctx, -1);

    duk_pop_n(ctx, 3);

    return str;
}

template<typename T>
inline
void dukx_put_hidden_prop(duk_context* ctx, int offset, const std::string& key, const T& val)
{
    push_dukobject(ctx, key, val);
    duk_put_prop_string(ctx, -1 + offset, DUKX_HIDDEN_SYMBOL("HIDDEN_OBJ").c_str());
}

template<typename T, typename... U>
inline
void dukx_push_c_function_with_hidden(duk_context* ctx, T& t, int nargs, U... u)
{
    duk_push_c_function(ctx, &t, nargs);
	push_dukobject(ctx, u...);
    duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("HIDDEN_OBJ").c_str());
}

void dukx_sanitise_move_value(duk_context* ctx, duk_context* dst_ctx, duk_idx_t idx);
void dukx_sanitise_in_place(duk_context* ctx, duk_idx_t idx);

inline
duk_ret_t dukx_proxy_get_prototype_of(duk_context* ctx)
{
    //printf("gproto\n");

    duk_get_prototype(ctx, 0);

    //duk_push_undefined(ctx);

    return 1;
}

inline
duk_ret_t dukx_proxy_set_prototype_of(duk_context* ctx)
{
    //printf("sproto\n");

    duk_set_prototype(ctx, 0);

    duk_push_true(ctx);

    return 1;
}

///uuh
inline
duk_ret_t dukx_proxy_is_extensible(duk_context* ctx)
{
    //printf("ext\n");

    duk_push_true(ctx);
    return 1;
}

inline
duk_ret_t dukx_proxy_prevent_extension(duk_context* ctx)
{
    //printf("pext\n");

    return 0;
}

inline
duk_ret_t dukx_proxy_get_own_property(duk_context* ctx)
{
    //printf("gprop\n");

    duk_get_prop_desc(ctx, 0, 0);
    return 1;
}

inline
duk_ret_t dukx_proxy_define_property(duk_context* ctx)
{
    //printf("dprop\n");

    duk_push_true(ctx);
    return 1;
}

inline
duk_ret_t dukx_proxy_has(duk_context* ctx)
{
    //printf("hprop\n");

    duk_push_boolean(ctx, duk_has_prop(ctx, 0));
    return 1;
}

inline
duk_ret_t dukx_stringify_parse(duk_context* ctx)
{
    duk_push_current_function(ctx);
    duk_get_prop_string(ctx, -1, DUKX_HIDDEN_SYMBOL("json_me_harder").c_str());
    duk_dup(ctx, -1);
    duk_json_encode(ctx, -1);
    duk_json_decode(ctx, -1);

    return 1;
}

///HEY
///THIS FUNCTION IS AN EXCEPTION
///MUST HANDLE ITS OWN SANITISATION
///AS SOMETIMES IT LETS SLIP SOMETHING THAT ISNT SANITISED ON PURPOSE
inline
duk_ret_t dukx_proxy_get(duk_context* ctx)
{
    //printf("get\n");

    //duk_pop(ctx);

    /*int top = duk_get_top(ctx);

    for(int i=0; i < top; i++)
    {
        duk_dup(ctx, i);

        //printf("stack top: %i\n", duk_get_top(ctx));

        printf("%i val %s\n", i, duk_safe_to_string(ctx, -1));

        duk_pop(ctx);
    }*/

    duk_dup(ctx, 1);

    std::string str = duk_safe_to_std_string(ctx, -1);

    duk_pop(ctx);

    ///return target

    ///NO SANITISE PASTH
    if(str == "toJSON")
    {
        duk_pop(ctx);
        duk_pop(ctx);

        duk_push_c_function(ctx, dukx_stringify_parse, 0);
        duk_dup(ctx, -2);
        duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("json_me_harder").c_str());

        //duk_push_true(ctx);
        //duk_put_prop_string(ctx, -2, DUKX_HIDDEN_SYMBOL("no_proxy").c_str());

        /*duk_def_prop(ctx, -1,
             DUK_DEFPROP_HAVE_WRITABLE |
             DUK_DEFPROP_HAVE_ENUMERABLE | DUK_DEFPROP_ENUMERABLE |
             DUK_DEFPROP_HAVE_CONFIGURABLE | DUK_DEFPROP_FORCE);*/

        duk_freeze(ctx, -1);

        //printf("stringify\n");

        //duk_pop(ctx);
        return 1;
    }

    /*if(str == "valueOf")
    {
        duk_pop(ctx);
        duk_pop(ctx);
        ///needs to be a function
        duk_push_object(ctx);
        return 1;
    }*/

    duk_pop(ctx);

    if(!duk_get_prop(ctx, 0))
        duk_push_undefined(ctx);

    dukx_sanitise_in_place(ctx, -1);

    return 1;
}

inline
duk_ret_t dukx_proxy_set(duk_context* ctx)
{
    //printf("set\n");

    ///remove receiver
    duk_remove(ctx, 3);

    duk_push_boolean(ctx, duk_put_prop(ctx, 0));

    return 1;
}

inline
duk_ret_t dukx_proxy_delete_property(duk_context* ctx)
{
    //printf("del\n");

    duk_push_boolean(ctx, duk_del_prop(ctx, 0));

    return 1;
}

inline
std::vector<std::string> dukx_get_keys(duk_context* ctx)
{
    std::vector<std::string> keys;

    duk_enum(ctx, -1, 0);
    while(duk_next(ctx, -1, 0))
    {
        keys.push_back(duk_safe_to_std_string(ctx, -1));
        duk_pop(ctx);
    }

    //std::cout << "fnum keys " << keys.size() << std::endl;

    duk_pop(ctx);

    return keys;
}

inline
void dukx_hack_in_keys(duk_context* ctx, duk_idx_t idx, const std::vector<std::string>& keys)
{
    for(auto& i : keys)
    {
        duk_push_number(ctx, 0.f);
        duk_put_prop_string(ctx, -1 + idx, i.c_str());
    }
}

inline
duk_ret_t dukx_proxy_own_keys(duk_context* ctx)
{
    auto keys = dukx_get_keys(ctx);

    //duk_push_array(ctx);

    ///duk_proxy_ownkeys_postprocess
    ///seems to filter out keys not in the underlying object

    push_duk_val(ctx, keys);

    return 1;
}

duk_ret_t dukx_proxy_apply(duk_context* ctx);

inline
duk_ret_t dukx_proxy_construct(duk_context* ctx)
{
    //printf("pcst\n");

    duk_push_true(ctx);

    return 1;
}

inline
duk_ret_t dukx_dummy(duk_context* ctx)
{
    return 0;
}

template<duk_c_function t>
inline
duk_ret_t dukx_wrap_ctx(duk_context* ctx)
{
    ///[arg1, arg2... argtop]
    int top = duk_get_top(ctx);

    ///[argstop, thread]
    duk_push_thread(ctx);
    //duk_push_thread_new_globalenv(ctx);

    duk_context* new_ctx = duk_get_context(ctx, -1);

    duk_push_object(new_ctx);
    duk_set_global_object(new_ctx);

    ///[thread, argstop]
    duk_insert(ctx, 0);

    duk_require_stack(new_ctx, top+1);

    duk_push_current_function(ctx);
    duk_xmove_top(new_ctx, ctx, 1);
    duk_get_prop_string(new_ctx, -1, DUKX_HIDDEN_SYMBOL("WRAPPED").c_str());
    duk_remove(new_ctx, -2);

    duk_xmove_top(ctx, new_ctx, 1);

    ///[thread, argstop, new_arg]

    ///replaces this to [thread, new_arg, argstopexcept0]
    duk_replace(ctx, -1 - top);

    ///ok so we have [thread, argstop]
    ///we want to replace the first arg with the hidden body
    ///[new -> cfunc]
    duk_push_c_function(new_ctx, t, top);
    ///[old -> thread]
    ///[new -> cfunc, argstop]
    duk_xmove_top(new_ctx, ctx, top);

    ///[new -> return]
    duk_int_t rc = duk_pcall(new_ctx, top);

    ///[new -> empty]
    ///[old -> thread, return]
    //duk_xmove_top(ctx, new_ctx, 1);

    ///get is special cased because
    ///it can let unsanitised values out
    if(t != dukx_proxy_get && t != dukx_proxy_own_keys)
        dukx_sanitise_move_value(new_ctx, ctx, -1);
    else
        duk_xmove_top(ctx, new_ctx, 1);

    ///remove thread
    ///[old -> return]
    duk_remove(ctx, 0);

    if(rc != DUK_EXEC_SUCCESS)
    {
        return duk_throw(ctx);
    }

    return 1;
}

#define DUKX_HIDE() duk_dup(dst_ctx, -3 + idx);\
                    duk_put_prop_string(dst_ctx, -1 + idx, DUKX_HIDDEN_SYMBOL("WRAPPED").c_str());

void dukx_sanitise_in_place(duk_context* dst_ctx, duk_idx_t idx);

void dukx_sanitise_move_value(duk_context* ctx, duk_context* dst_ctx, duk_idx_t idx);

#endif // DUK_OBJECT_FUNCTIONS_HPP_INCLUDED
