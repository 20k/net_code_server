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

js::value_context::value_context(context_t* _ctx) : ctx(_ctx)
{

}

void js::value_context::free(int idx)
{
    if((int)free_stack.size() < idx + 1)
    {
        free_stack.resize(idx + 1);
    }

    free_stack[idx] = 1;

    int duk_top = duk_get_top(ctx);
    int my_top = free_stack.size();

    if(duk_top > my_top)
        return;

    free_stack.resize(duk_top);

    int num_poppable = 0;

    for(int i=duk_top - 1; i >= 0; i--)
    {
        if(free_stack[i] == 1)
            num_poppable++;
        else
            break;
    }

    if(num_poppable == 0)
        return;

    duk_pop_n(ctx, num_poppable);
    free_stack.resize(duk_get_top(ctx));
}

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

js::value& js::value::operator=(int v)
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
        vctx->free(idx);
        //duk_remove(ctx, idx);
    }
    else
    {
        if(indices.index() == 1)
        {
            duk_del_prop_index(ctx, parent_idx, std::get<int>(indices));
        }
        else
        {
            duk_del_prop_lstring(ctx, parent_idx, std::get<std::string>(indices).c_str(), std::get<std::string>(indices).size());
        }

        vctx->free(idx);
        //duk_remove(ctx, idx);
    }

    idx = -1;
    return *this;
}

js::value& js::value::operator=(const value& right)
{
    if(idx == -1 && right.idx == -1)
        return *this;

    if(idx == -1)
    {
        duk_dup(ctx, right.idx);
        idx = duk_get_top_index(ctx);
        parent_idx = right.parent_idx;
        indices = right.indices;
    }
    else
    {
        vctx->free(idx);
        //duk_remove(ctx, idx);
        duk_dup(ctx, right.idx);
        idx = duk_get_top_index(ctx);
        parent_idx = right.parent_idx;
        indices = right.indices;
    }

    return *this;
}

js::value& js::value::operator=(js_funcptr_t fptr)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, fptr);

    return *this;
}


js::value::value(const js::value& value)
{
    vctx = value.vctx;
    ctx = value.ctx;
    parent_idx = value.parent_idx;
    indices = value.indices;

    duk_dup(ctx, value.idx);
    idx = duk_get_top_index(ctx);
}

js::value::value(js::value&& other)
{
    vctx = other.vctx;
    ctx = other.ctx;
    idx = other.idx;
    parent_idx = other.parent_idx;
    released = other.released;
    indices = other.indices;
    other.released = true;
}

js::value& js::value::operator=(js::value&& other)
{
    vctx = other.vctx;
    ctx = other.ctx;
    idx = other.idx;
    parent_idx = other.parent_idx;
    released = other.released;
    indices = other.indices;
    other.released = true;

    return *this;
}

js::value::value(js::value_context& _vctx) : vctx(&_vctx), ctx(_vctx.ctx)
{
    idx = duk_push_object(ctx);
}

js::value::value(js::value_context& _vctx, int _idx) : vctx(&_vctx), ctx(_vctx.ctx), idx(_idx)
{
    if(idx < 0)
    {
        idx = duk_get_top_index(ctx);

        if(idx < 0)
            throw std::runtime_error("bad idx < 0");
    }
}

js::value::value(js::value_context& _vctx, js::value& base, const std::string& key) : vctx(&_vctx), ctx(_vctx.ctx)
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

js::value::value(js::value_context& _vctx, js::value& base, int key) : vctx(&_vctx), ctx(_vctx.ctx)
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

js::value::value(js::value_context& _vctx, js::value& base, const char* key) : vctx(&_vctx), ctx(_vctx.ctx)
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
    if(idx != -1 && !released)
    {
        vctx->free(idx);
    }
}

js::value js::value::operator[](int64_t val)
{
    return js::value(*vctx, *this, val);
}

js::value js::value::operator[](const std::string& val)
{
    return js::value(*vctx, *this, val);
}

js::value js::value::operator[](const char* val)
{
    return js::value(*vctx, *this, val);
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

bool js::value::is_function()
{
    if(idx == -1)
        return false;

    return duk_is_function(ctx, idx);
}

void js::value::release()
{
    released = true;
}

void test_func()
{

}

void test_func_with_args(int one, std::string two)
{

}

std::string test_func_with_return(double one, std::string two)
{
    printf("Called with %lf %s\n", one, two.c_str());

    return "poopy";
}

struct js_val_tester
{
    js_val_tester()
    {
        duk_context* ctx = duk_create_heap_default();

        js::value_context vctx(ctx);

        js::value val(vctx);
        val = (int64_t)53;

        assert((int64_t)val == 53);

        assert(duk_get_top(ctx) == 1);

        js::value root(vctx);
        js::value base(vctx, root, "hello");
        base = (int64_t)53;

        assert((int64_t)root["hello"] == 53);

        assert(duk_get_top(ctx) == 3);

        duk_push_object(ctx);

        duk_push_string(ctx, "key");
        duk_push_string(ctx, "value");

        duk_put_prop(ctx, -3);

        js::value tobj(vctx, -1);

        printf("PRETOP %i\n", duk_get_top(ctx));

        assert((std::string)tobj["key"] == "value");

        printf("TOP %i\n", duk_get_top(ctx));

        assert(duk_get_top(ctx) == 4);

        tobj["key"] = std::nullopt;

        assert(tobj["key"].is_empty());

        tobj["key"] = (int64_t)53;

        assert((int64_t)tobj["key"] == 53);

        tobj = std::nullopt;

        assert(duk_get_top(ctx) == 3);

        std::map<std::string, int> test_object;
        test_object["hithere"] = 12;
        test_object["pooper"] = 55;

        js::value fmap(vctx);
        fmap = test_object;

        assert((int64_t)fmap["hithere"] == 12);
        assert((int64_t)fmap["pooper"] == 55);

        std::map<std::string, int> out_map = fmap;

        assert(out_map.size() == 2);
        assert(out_map["hithere"] == 12);
        assert(out_map["pooper"] == 55);

        assert(duk_get_top(ctx) == 4);

        {
            js::value func(vctx);
            func = js::function<test_func_with_return>;

            js::value a1(vctx);
            a1 = 12;

            js::value some_string(vctx);
            some_string = "poopersdf";

            assert(duk_get_top(ctx) == 7);

            auto [res, retval] = js::call(func, a1, some_string);

            std::string rvals = retval;

            printf("Found %s\n", rvals.c_str());

            assert(rvals == "poopy");

            printf("TOP %i\n", duk_get_top(ctx));

            assert(duk_get_top(ctx) == 8);
        }

        printf("Done js val testers\n");
    }
};

namespace
{
    js_val_tester tester;
}
