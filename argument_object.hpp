#ifndef ARGUMENT_OBJECT_HPP_INCLUDED
#define ARGUMENT_OBJECT_HPP_INCLUDED

#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <tuple>

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

struct stack_helper
{
    context_t* ctx = nullptr;
    int idx = -1;
    std::variant<std::monostate, int, std::string> indices;

    stack_helper(){}

    template<typename T>
    stack_helper(T&& t)
    {
        *this = t;
    }

    stack_helper& operator=(const char* v);
    stack_helper& operator=(const std::string& v);
    stack_helper& operator=(int64_t v);
    stack_helper& operator=(double v);
    stack_helper& operator=(const std::vector<stack_helper>& v);
    stack_helper& operator=(const std::map<stack_helper, stack_helper>& v);
    stack_helper& operator=(std::function<stack_helper(stack_helper&)> v);

    operator std::string();
    operator int64_t();
    operator double();
    operator std::vector<stack_helper>();
    operator std::map<stack_helper, stack_helper>();

    stack_helper& operator[](int64_t val);
    stack_helper& operator[](const std::string& str);

    stack_helper(duk_context* _ctx, int _idx = -1);
    ~stack_helper();
};

#endif // ARGUMENT_OBJECT_HPP_INCLUDED
