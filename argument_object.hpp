#ifndef ARGUMENT_OBJECT_HPP_INCLUDED
#define ARGUMENT_OBJECT_HPP_INCLUDED

#include "argument_object_qjs.hpp"
#include "argument_object_duk.hpp"

///this stuff is features, not implementation dependent
namespace js
{
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

    template<typename T>
    inline
    js::value make_success(js::value_context& vctx, const T& msg)
    {
        js::value v(vctx);
        v["ok"] = true;
        v["msg"] = msg;

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

    inline
    void empty_function(js::value_context*)
    {

    }
}

#endif // ARGUMENT_OBJECT_HPP_INCLUDED
