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

stack_helper::stack_helper(duk_context* _ctx, int _idx)
{
    duk_idx_t thr_idx = duk_push_thread(_ctx);
    ctx = duk_get_context(_ctx, thr_idx);

    idx = 0;

    duk_dup(ctx, -1 + _idx);
    duk_xmove_top(ctx, _ctx, 1);
}

struct stack_manage
{
    stack_helper& sh;

    stack_manage(stack_helper& in) : sh(in)
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

    ~stack_manage()
    {
        if(sh.indices.index() == 0)
        {
            duk_replace(sh.ctx, sh.idx);
        }
        else
        {
            duk_put_prop(sh.ctx, sh.idx);
        }
    }
};

stack_helper& stack_helper::operator=(const char* v)
{
    stack_manage m(*this);

    duk_push_string(ctx, v);

    return *this;
}

stack_helper& stack_helper::operator=(const std::string& v);
stack_helper& stack_helper::operator=(int64_t v);
stack_helper& stack_helper::operator=(double v);
stack_helper& stack_helper::operator=(const std::vector<stack_helper>& v);
stack_helper& stack_helper::operator=(const std::map<stack_helper, stack_helper>& v);

stack_helper::~stack_helper()
{
    duk_pop_n(ctx, duk_get_top(ctx));
}
