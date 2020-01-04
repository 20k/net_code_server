#ifndef ARGUMENT_OBJECT_HPP_INCLUDED
#define ARGUMENT_OBJECT_HPP_INCLUDED

#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <functional>

struct duk_context;

using context_t = duk_context;

struct value_object
{
    std::variant<std::string,
    int64_t,
    double,
    std::vector<value_object>,
    std::map<value_object, value_object>,
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

#endif // ARGUMENT_OBJECT_HPP_INCLUDED
