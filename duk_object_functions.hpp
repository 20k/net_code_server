#ifndef DUK_OBJECT_FUNCTIONS_HPP_INCLUDED
#define DUK_OBJECT_FUNCTIONS_HPP_INCLUDED

using duk_func_t = duk_ret_t (*)(duk_context*);


inline
void push_duk_val(duk_context* ctx, duk_func_t& func)
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
void put_duk_keyvalue(duk_context* ctx, const std::string& key, const T& value)
{
    push_duk_val(ctx, value);
    duk_put_prop_string(ctx, -2, key.c_str());
}

template<typename T>
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

#endif // DUK_OBJECT_FUNCTIONS_HPP_INCLUDED
