#include "argument_object.hpp"
#include "duktape.h"

#if 0
#include <assert.h>

value_object& value_object::operator=(const std::string& v)
{
    var = v;
    return *this;
}

value_object& value_object::operator=(const char* v)
{
    if(v != nullptr)
        var = std::string(v);
    else
        var = "";

    return *this;
}

value_object& value_object::operator=(int64_t v)
{
    var = v;
    return *this;
}

value_object& value_object::operator=(double v)
{
    var = v;
    return *this;
}

value_object& value_object::operator=(const std::vector<value_object>& v)
{
    var = v;
    return *this;
}

value_object& value_object::operator=(const std::map<value_object, value_object>& v)
{
    var = v;
    return *this;
}

value_object& value_object::operator=(std::function<value_object(value_object&)> v)
{
    var = v;
    return *this;
}

value_object& value_object::operator[](int64_t val)
{
    if(std::vector<value_object>* pval = std::get_if<std::vector<value_object>>(&var))
    {
        assert(pval);

        if(val >= (int64_t)pval->size() || val < 0)
            throw std::runtime_error("Out of bounds");

        return (*pval)[val];
    }

    if(std::map<value_object, value_object>* pval = std::get_if<std::map<value_object, value_object>>(&var))
    {
        assert(pval);

        value_object vobj;
        vobj.var = val;

        return pval->at(vobj);
    }

    throw std::runtime_error("Not map or vector");
}

value_object& value_object::operator[](const std::string& str)
{
    if(std::map<value_object, value_object>* pval = std::get_if<std::map<value_object, value_object>>(&var))
    {
        value_object vobj;
        vobj.var = str;

        return pval->at(vobj);
    }

    throw std::runtime_error("Not map");
}

value_object::operator std::string()
{
    return std::get<0>(var);
}

value_object::operator int64_t()
{
    return std::get<1>(var);
}

value_object::operator double()
{
    return std::get<2>(var);
}

value_object::operator std::vector<value_object>()
{
    return std::get<3>(var);
}

value_object::operator std::map<value_object, value_object>()
{
    return std::get<4>(var);
}

template<typename T, typename U>
size_t get_address(std::function<U(T&)> f)
{
    typedef U(function_type)(T&);
    function_type** fptr = f.template target<function_type*>();
    return (size_t)*fptr;
}

#define CHECK_INDEX(i) if(l.var.index() == i) {return (std::get<i>(l.var) < std::get<i>(r.var));}

bool operator<(const value_object& l, const value_object& r)
{
    if(l.var.index() < r.var.index())
        return true;

    if(l.var.index() > r.var.index())
        return false;

    if(l.var.index() != 6)
    {
        CHECK_INDEX(0);
        CHECK_INDEX(1);
        CHECK_INDEX(2);
        CHECK_INDEX(3);
        CHECK_INDEX(4);
        CHECK_INDEX(5);

        throw std::runtime_error("Error in < for value object");
    }
    else
    {
        return get_address(std::get<6>(l.var)) < get_address(std::get<6>(r.var));
    }

    //return l.var < r.var;
}

argument_object::argument_object(context_t* in) : ctx(in)
{

}

struct tester
{
    tester()
    {
        printf("Value object tests\n");

        value_object vobj = (int64_t)5;
        value_object v2 = (int64_t)6;

        assert(vobj < v2);

        int64_t val = vobj;

        assert(val == 5);
        assert(val != 6);

        int64_t val2 = v2;

        assert(val2 == 6);

        value_object sobj = "hello";

        std::string found = sobj;

        assert(found == "hello");

        printf("Finished value object tests\n");
    }
};

namespace
{
    tester t;
}
#endif // 0

/*js::value::value(duk_context* _ctx, int _idx)
{
    duk_idx_t thr_idx = duk_push_thread(_ctx);
    ctx = duk_get_context(_ctx, thr_idx);

    idx = 0;

    duk_dup(ctx, -1 + _idx);
    duk_xmove_top(ctx, _ctx, 1);
}*/

stack_manage::stack_manage(js::value& in) : sh(in)
{
    if(sh.indices.index() == 0)
    {
        ///nothing
    }
    else
    {
        if(sh.indices.index() == 1)
            duk_push_int(sh.ctx, std::get<1>(sh.indices));
        else
            duk_push_string(sh.ctx, std::get<2>(sh.indices).c_str());
    }
}

stack_manage::~stack_manage()
{
    if(sh.indices.index() == 0)
    {
        if(sh.idx != -1)
            duk_replace(sh.ctx, sh.idx);
        else
            sh.idx = duk_get_top_index(sh.ctx);
    }
    else
    {
        ///replace property on parent
        duk_put_prop(sh.ctx, sh.parent_idx);

        ///update stack object as well
        if(sh.indices.index() == 1)
            duk_push_int(sh.ctx, std::get<1>(sh.indices));
        else
            duk_push_string(sh.ctx, std::get<2>(sh.indices).c_str());

        duk_get_prop(sh.ctx, sh.parent_idx);

        if(sh.idx != -1)
            duk_replace(sh.ctx, sh.idx);
        else
            sh.idx = duk_get_top_index(sh.ctx);
    }
}

/*void dukx_push(duk_context* ctx, const js::value& v)
{
    if(v.indices.index() == 0)
        duk_dup(ctx, v.idx);
    else
    {
        if(v.indices.index() == 1)
            duk_push_int(ctx, std::get<1>(v.indices));
        else
            duk_push_string(ctx, std::get<2>(v.indices).c_str());

        duk_get_prop(ctx, v.idx);
    }
}*/

js::value& js::value::operator=(const char* v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js::value& js::value::operator=(const std::string& v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js::value& js::value::operator=(int64_t v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js::value& js::value::operator=(double v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js::value& js::value::operator=(std::nullopt_t t)
{
    if(idx == -1)
        return *this;

    if(indices.index() == 0)
    {
        duk_remove(ctx, idx);
    }
    else
    {
        if(indices.index() == 1)
        {
            duk_del_prop_index(ctx, idx, std::get<int>(indices));
        }
        else
        {
            duk_del_prop_lstring(ctx, idx, std::get<std::string>(indices).c_str(), std::get<std::string>(indices).size());
        }
    }

    idx = -1;
    return *this;
}

js::value::value(duk_context* _ctx) : ctx(_ctx)
{
    idx = duk_push_object(ctx);
}

js::value::value(duk_context* _ctx, int _idx) : ctx(_ctx), idx(_idx)
{
    if(idx < 0)
    {
        idx = duk_get_top_index(ctx);

        if(idx < 0)
            throw std::runtime_error("bad idx < 0");
    }
}

js::value::value(duk_context* _ctx, js::value& base, const std::string& key) : ctx(_ctx)
{
    parent_idx = base.idx;
    indices = key;

    if(parent_idx == -1)
        throw std::runtime_error("Empty parent");

    if(!base.has(key))
        return;

    duk_get_prop_lstring(ctx, parent_idx, key.c_str(), key.size());

    idx = duk_get_top_index(ctx);

    if(idx < 0)
        throw std::runtime_error("bad idx < 0");
}

js::value::value(duk_context* _ctx, js::value& base, int key) : ctx(_ctx)
{
    parent_idx = base.idx;
    indices = key;

    if(parent_idx == -1)
        throw std::runtime_error("Empty parent");

    if(!base.has(key))
        return;

    duk_get_prop_index(ctx, parent_idx, key);

    idx = duk_get_top_index(ctx);

    if(idx < 0)
        throw std::runtime_error("bad idx < 0");
}

js::value::value(duk_context* _ctx, js::value& base, const char* key) : ctx(_ctx)
{
    parent_idx = base.idx;
    indices = std::string(key);

    if(parent_idx == -1)
        throw std::runtime_error("Empty parent");

    if(!base.has(key))
        return;

    duk_get_prop_string(ctx, parent_idx, key);

    idx = duk_get_top_index(ctx);

    if(idx < 0)
        throw std::runtime_error("bad idx < 0");
}

js::value::~value()
{
    if(idx != -1)
        duk_remove(ctx, idx);
}

js::value js::value::operator[](int64_t val)
{
    return js::value(ctx, *this, val);
}

js::value js::value::operator[](const std::string& val)
{
    return js::value(ctx, *this, val);
}

js::value js::value::operator[](const char* val)
{
    return js::value(ctx, *this, val);
}

bool js::value::has(const std::string& key)
{
    if(idx == -1)
        return false;

    return duk_has_prop_lstring(ctx, idx, key.c_str(), key.size());
}

bool js::value::has(int key)
{
    if(idx == -1)
        return false;

    return duk_has_prop_index(ctx, idx, key);
}

bool js::value::has(const char* key)
{
    if(idx == -1)
        return false;

    return duk_has_prop_string(ctx, idx, key);
}

bool js::value::is_string()
{
    if(idx == -1)
        return false;

    return duk_is_string(ctx, idx);
}

bool js::value::is_number()
{
    if(idx == -1)
        return false;

    return duk_is_number(ctx, idx);
}

bool js::value::is_array()
{
    if(idx == -1)
        return false;

    return duk_is_array(ctx, idx);
}

bool js::value::is_map()
{
    if(idx == -1)
        return false;

    return duk_is_object(ctx, idx);
}

bool js::value::is_empty()
{
    return idx == -1;
}

struct js_val_tester
{
    js_val_tester()
    {
        duk_context* ctx = duk_create_heap_default();

        js::value val(ctx);
        val = (int64_t)53;

        assert((int64_t)val == 53);

        assert(duk_get_top(ctx) == 1);

        js::value root(ctx);
        js::value base(ctx, root, "hello");
        base = (int64_t)53;

        assert((int64_t)root["hello"] == 53);

        assert(duk_get_top(ctx) == 3);

        duk_push_object(ctx);

        duk_push_string(ctx, "key");
        duk_push_string(ctx, "value");

        duk_put_prop(ctx, -3);

        js::value tobj(ctx, -1);

        assert((std::string)tobj["key"] == "value");

        assert(duk_get_top(ctx) == 4);

        printf("Done js val testers\n");
    }
};

namespace
{
    js_val_tester tester;
}
