#include "argument_object.hpp"
#include "duktape.h"
#include <iostream>
#include "memory_sandbox.hpp"

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

void arg::dukx_push(duk_context* ctx, const js::value& val)
{
    if(val.idx == -1)
    {
        duk_push_undefined(ctx);
    }
    else
    {
        duk_dup(ctx, val.idx);
    }
}

duk_context* create_sandbox_heap()
{
    ///its easier to simply leak this
    sandbox_data* leaked_data = new sandbox_data;

    return duk_create_heap(sandbox_alloc, sandbox_realloc, sandbox_free, leaked_data, sandbox_fatal);
}

js::value_context::value_context(context_t* _ctx) : ctx(_ctx)
{

}

js::value_context::value_context(js::value_context& octx) : parent_context(&octx)
{
    duk_idx_t fidx = duk_push_thread_new_globalenv(octx.ctx);
    ctx = duk_get_context(octx.ctx, fidx);
    parent_idx = fidx;
}

js::value_context::value_context()
{
    ctx = create_sandbox_heap();
    owner = true;
}

js::value_context::~value_context()
{
    if(parent_context != nullptr)
    {
        parent_context->free(parent_idx);
    }

    if(owner)
    {
        duk_destroy_heap(ctx);
    }
}

void js::value_context::free(int idx)
{
    assert(idx >= 0);

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

js::value& js::value::operator=(bool v)
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

    /*if(idx == -1)
    {
        duk_dup(ctx, right.idx);
        idx = duk_get_top_index(ctx);
        parent_idx = right.parent_idx;
        indices = right.indices;
    }
    else
    {
        if(!released)
            vctx->free(idx);

        //duk_remove(ctx, idx);
        duk_dup(ctx, right.idx);
        idx = duk_get_top_index(ctx);
        parent_idx = right.parent_idx;
        indices = right.indices;
    }

    //released = right.released;

    return *this;*/

    stack_manage m(*this);

    duk_dup(ctx, right.idx);

    return *this;
}

js::value& js::value::operator=(js_funcptr_t fptr)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, fptr);

    return *this;
}

js::value& js::value::operator=(js::undefined_t)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, js::undefined);

    return *this;
}

js::value& js::value::operator=(const nlohmann::json& in)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, in);

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

js::value::value(js::value_context& vctx, const value& other) : js::value::value(other)
{

}

js::value::value(js::value&& other)
{
    if(other.released)
        throw std::runtime_error("Attempted to move from a released value");

    vctx = other.vctx;
    ctx = other.ctx;
    idx = other.idx;
    parent_idx = other.parent_idx;
    //released = other.released;
    indices = other.indices;
    other.released = true;
}

js::value& js::value::operator=(js::value&& other)
{
    if(other.released)
        throw std::runtime_error("Attempted to move from a released value");

    if(idx != -1 && !released)
    {
        vctx->free(idx);
    }

    vctx = other.vctx;
    ctx = other.ctx;
    idx = other.idx;
    parent_idx = other.parent_idx;
    //released = other.released;
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

js::value::value(js::value_context& _vctx, const js::value& base, const std::string& key) : vctx(&_vctx), ctx(_vctx.ctx)
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

js::value::value(js::value_context& _vctx, const js::value& base, int key) : vctx(&_vctx), ctx(_vctx.ctx)
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

js::value::value(js::value_context& _vctx, const js::value& base, const char* key) : vctx(&_vctx), ctx(_vctx.ctx)
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

std::string js::value::to_json()
{
    if(idx == -1)
        return "{}";

    duk_dup(ctx, idx);

    const char* str = duk_json_encode(ctx, -1);

    if(str == nullptr)
    {
        duk_pop(ctx);
        return "";
    }

    std::string ret(str);

    duk_pop(ctx);

    return ret;
}

std::vector<uint8_t> js::value::to_cbor()
{
    if(idx == -1)
        throw std::runtime_error("Empty value for cbor");

    duk_dup(ctx, idx);

    duk_cbor_encode(ctx, -1, 0);
    size_t len = 0;
    const char* ptr = (const char*)duk_require_buffer_data(ctx, -1, &len);

    if(ptr == nullptr || len == 0)
    {
        duk_pop(ctx);
        throw std::runtime_error("Empty return value for cbor");
    }

    std::vector<uint8_t> ret(ptr, ptr + len);

    duk_pop(ctx);

    return ret;
}

void js::value::stringify_parse()
{
    duk_json_encode(ctx, idx);
    duk_json_decode(ctx, idx);
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

bool js::value::has(const std::string& key) const
{
    if(idx == -1)
        return false;

    if(is_undefined())
        return false;

    return duk_has_prop_lstring(ctx, idx, key.c_str(), key.size());
}

bool js::value::has(int key) const
{
    if(idx == -1)
        return false;

    if(is_undefined())
        return false;

    return duk_has_prop_index(ctx, idx, key);
}

bool js::value::has(const char* key) const
{
    if(idx == -1)
        return false;

    if(is_undefined())
        return false;

    return duk_has_prop_string(ctx, idx, key);
}

bool js::value::has_hidden(const std::string& key) const
{
    if(idx == -1)
       return false;

    if(is_undefined())
        return false;

    std::string rkey = "\xFF" + key;

    return has(rkey);
}

js::value js::value::get(const std::string& key)
{
    //if(!has(key))
    //    return js::make_value(*vctx, std::nullopt);

    return js::value(*vctx, *this, key);
}

js::value js::value::get(int key)
{
    //if(!has(key))
    //    return js::make_value(*vctx, std::nullopt);

    return js::value(*vctx, *this, key);
}

js::value js::value::get(const char* key)
{
    //if(!has(key))
    //    return js::make_value(*vctx, std::nullopt);

    return js::value(*vctx, *this, key);
}

js::value js::value::get_hidden(const std::string& key)
{
    //if(!has_hidden(key))
    //    return js::make_value(*vctx, std::nullopt);

    std::string rkey = "\xFF" + key;

    return js::value(*vctx, *this, rkey);
}

bool js::value::del(const std::string& key)
{
    if(!has(key))
        return false;

    js::value val(*vctx, *this, key);
    val = std::nullopt;
    return true;
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

bool js::value::is_boolean()
{
    if(idx == -1)
        return false;

    return duk_is_boolean(ctx, idx);
}

bool js::value::is_undefined() const
{
    if(idx == -1)
        return false;

    return duk_is_undefined(ctx, idx);
}

bool is_truthy(duk_context* ctx, duk_idx_t idx)
{
    duk_dup(ctx, idx);
    bool success = duk_to_boolean(ctx, -1);

    duk_pop(ctx);

    return success;
}

bool js::value::is_truthy()
{
    if(idx == -1)
        return false;

    return ::is_truthy(ctx, idx);
}

bool js::value::is_object_coercible()
{
    if(idx == -1)
        return false;

    return duk_is_object_coercible(ctx, idx);
}

bool js::value::is_object()
{
    if(idx == -1)
        return false;

    return duk_is_object(ctx, idx);
}

void js::value::release()
{
    released = true;
}

js::value js::get_global(js::value_context& vctx)
{
    duk_push_global_object(vctx.ctx);

    return js::value(vctx, -1);
}

void js::set_global(js::value_context& vctx, const js::value& val)
{
    duk_dup(vctx.ctx, val.idx);
    duk_set_global_object(vctx.ctx);
}

js::value js::get_current_function(js::value_context& vctx)
{
    duk_push_current_function(vctx.ctx);

    return js::value(vctx, -1);
}

js::value js::get_this(js::value_context& vctx)
{
    duk_push_this(vctx.ctx);

    return js::value(vctx, -1);
}

js::value js::get_heap_stash(js::value_context& vctx)
{
    duk_push_heap_stash(vctx.ctx);

    return js::value(vctx, -1);
}

js::value js::get_global_stash(js::value_context& vctx)
{
    duk_push_global_stash(vctx.ctx);

    return js::value(vctx, -1);
}

void* js::get_sandbox_data_impl(value_context& vctx)
{
    duk_memory_functions mem_funcs_duk; duk_get_memory_functions(vctx.ctx, &mem_funcs_duk);
    return mem_funcs_duk.udata;
}

void js::dump_stack(js::value_context& vctx)
{
    duk_push_context_dump(vctx.ctx);

    js::value val(vctx, -1);

    std::cout << "GOT " << (std::string)val << std::endl;
}

js::value js::add_setter(js::value& base, const std::string& key, js_funcptr_t func)
{
    js::value val(*base.vctx);
    val = func;

    duk_push_lstring(base.ctx, key.c_str(), key.size());
    duk_dup(base.ctx, val.idx);

    duk_def_prop(base.ctx, base.idx, DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_FORCE);

    return val;
}

js::value js::add_getter(js::value& base, const std::string& key, js_funcptr_t func)
{
    js::value val(*base.vctx);
    val = func;

    duk_push_lstring(base.ctx, key.c_str(), key.size());
    duk_dup(base.ctx, val.idx);

    duk_def_prop(base.ctx, base.idx, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_FORCE);

    return val;
}

std::pair<bool, js::value> js::compile(js::value_context& vctx, const std::string& data)
{
    return compile(vctx, data, "test-name");
}

std::pair<bool, js::value> js::compile(js::value_context& vctx, const std::string& data, const std::string& name)
{
    duk_push_string(vctx.ctx, data.c_str());
    duk_push_string(vctx.ctx, name.c_str());

    bool success = duk_pcompile(vctx.ctx, DUK_COMPILE_EVAL) == 0;

    return {success, js::value(vctx, -1)};
}

std::string js::dump_function(js::value& val)
{
    if(!val.is_function())
        throw std::runtime_error("Tried to dump not a function");

    duk_dup(val.ctx, val.idx);
    duk_dump_function(val.ctx);

    duk_size_t out;
    char* ptr = (char*)duk_get_buffer(val.ctx, -1, &out);

    std::string buf(ptr, out);

    duk_pop(val.ctx);
    return buf;
}

js::value js::eval(js::value_context& vctx, const std::string& data)
{
    duk_eval_string(vctx.ctx, data.c_str());

    return js::value(vctx, -1);
}

js::value js::xfer_between_contexts(js::value_context& destination, const js::value& val)
{
    if(destination.ctx == val.ctx)
        throw std::runtime_error("Bad same contexts");

    duk_dup(val.ctx, val.idx);

    duk_xmove_top(destination.ctx, val.ctx, 1);

    return js::value(destination, -1);
}

void js::value::pack()
{
    if(idx <= 0)
        return;

    if(duk_get_top_index(ctx) != idx)
        return;

    int cidx = idx - 1;

    while(cidx > 0 && vctx->free_stack.at(cidx) == 1)
    {
        cidx--;
    }

    ///cidx occupied, increment to get first free space
    cidx++;

    if(cidx >= idx - 1)
        return;

    if(vctx->free_stack.at(cidx) != 1)
        throw std::runtime_error("Expected 1 in free stack");

    vctx->free_stack.at(cidx) = 0;

    duk_dup(ctx, idx);
    duk_replace(ctx, cidx);

    vctx->free(idx);
    idx = cidx;
}

js::value js::make_proxy(js::value& target, js::value& handle)
{
    duk_dup(target.ctx, target.idx);
    duk_dup(handle.ctx, handle.idx);

    duk_push_proxy(target.ctx, 0);

    return js::value(*target.vctx, -1);
}

js::value js::from_cbor(js::value_context& vctx, const std::vector<uint8_t>& cb)
{
    if(cb.size() == 0)
        throw std::runtime_error("0 byte long cbor array");

    char* ptr = (char*)duk_push_buffer(vctx.ctx, cb.size(), false);

    for(int i=0; i < (int)cb.size(); i++)
    {
        ptr[i] = cb[i];
    }

    //memcpy(&ptr, &cb[0], cb.size());

    duk_cbor_decode(vctx.ctx, -1, 0);

    return js::value(vctx, -1);
}

void test_func()
{

}

void test_func_with_args(int one, std::string two)
{

}

void test_set(js::value_context* vctx, js::value val)
{
    std::cout << "GOT " << (std::string)val << std::endl;
}

js::value test_get(js::value_context* vctx)
{
    js::value test_ret(*vctx);
    test_ret = "got";
    return test_ret;
}

std::string test_func_with_return(double one, std::string two)
{
    printf("Called with %lf %s\n", one, two.c_str());

    return "poopy";
}

double test_func_with_context(js::value_context* ctx, std::string one, double two)
{
    assert(ctx);

    printf("Called with %s %lf\n", one.c_str(), two);

    return 256;
}

js::value test_js_val(js::value_context* ctx, js::value val)
{
    return js::value(*ctx);
}

std::string test_error(js::value_context* vctx, js::value val)
{
    return "hi";
}

js_quickjs::value_context::value_context(value_context& other)
{
    heap = other.heap;
    void* sandbox = JS_GetContextOpaque(other.ctx);
    ctx = JS_NewContext(heap);
    JS_SetContextOpaque(ctx, sandbox);
}

js_quickjs::value_context::value_context()
{
    sandbox_data* leaked_data = new sandbox_data;

    //heap = JS_NewRuntime2();
    heap = JS_NewRuntime();
    ctx = JS_NewContext(heap);
    JS_SetContextOpaque(ctx, (void*)leaked_data);
    owner = true;
}

js_quickjs::value_context::~value_context()
{
    JS_FreeContext(ctx);

    if(owner)
    {
        JS_FreeRuntime(heap);
    }
}

js_quickjs::value::value(const js_quickjs::value& other)
{
    vctx = other.vctx;
    ctx = other.ctx;

    val = JS_DupValue(other.ctx, other.val);
    has_value = true;
}

js_quickjs::value::value(js_quickjs::value_context& _vctx)
{
    vctx = &_vctx;
    ctx = vctx->ctx;

    val = JS_NewObject(ctx);
    has_value = true;
}

js_quickjs::value::value(js_quickjs::value_context& vctx, const js_quickjs::value& other) : js_quickjs::value(other)
{

}

js_quickjs::value::value(js_quickjs::value_context& _vctx, const js_quickjs::value& other, const std::string& key) : value(_vctx, other, key.c_str())
{

}

js_quickjs::value::value(js_quickjs::value_context& _vctx, const js_quickjs::value& parent, const char* key)
{
    ctx = _vctx.ctx;
    vctx = &_vctx;

    if(!parent.has_value)
        throw std::runtime_error("Parent is not a value");

    has_parent = true;
    parent_value = JS_DupValue(parent.ctx, parent.val);
    indices = key;

    if(!parent.has(key))
        return;

    has_value = true;
    val = JS_GetPropertyStr(ctx, parent_value, key);
}

js_quickjs::value::value(js_quickjs::value_context& _vctx, const js_quickjs::value& parent, int key)
{
    if(key < 0)
        throw std::runtime_error("Key < 0");

    ctx = _vctx.ctx;
    vctx = &_vctx;
    indices = key;

    if(!parent.has_value)
        throw std::runtime_error("Parent is not a value");

    has_parent = true;
    parent_value = JS_DupValue(parent.ctx, parent.val);

    if(!parent.has(key))
        return;

    has_value = true;
    val = JS_GetPropertyUint32(ctx, parent_value, key);
}

js_quickjs::value::~value()
{
    if(has_value)
    {
        JS_FreeValue(ctx, val);
    }

    if(has_parent)
    {
        JS_FreeValue(ctx, parent_value);
    }
}

bool js_quickjs::value::has(const char* key) const
{
    if(!has_value)
        return false;

    if(is_undefined())
        return false;

    JSAtom atom = JS_NewAtom(ctx, key);

    bool has_prop = JS_HasProperty(ctx, val, atom);

    JS_FreeAtom(ctx, atom);

    return has_prop;
}

bool js_quickjs::value::has(const std::string& key) const
{
    if(!has_value)
        return false;

    if(is_undefined())
        return false;

    JSAtom atom = JS_NewAtomLen(ctx, key.c_str(), key.size());

    bool has_prop = JS_HasProperty(ctx, val, atom);

    JS_FreeAtom(ctx, atom);

    return has_prop;
}

bool js_quickjs::value::has(int key) const
{
    if(key < 0)
        throw std::runtime_error("value.has key < 0");

    if(!has_value)
        return false;

    if(is_undefined())
        return false;

    JSAtom atom = JS_NewAtomUInt32(ctx, (uint32_t)key);

    bool has_prop = JS_HasProperty(ctx, val, atom);

    JS_FreeAtom(ctx, atom);

    return has_prop;
}

js_quickjs::value js_quickjs::value::get(const std::string& key)
{
    return js_quickjs::value(*vctx, *this, key);
}

js_quickjs::value js_quickjs::value::get(int key)
{
    return js_quickjs::value(*vctx, *this, key);
}

js_quickjs::value js_quickjs::value::get(const char* key)
{
    return js_quickjs::value(*vctx, *this, key);
}

bool js_quickjs::value::is_string()
{
    if(!has_value)
        return false;

    return JS_IsString(val);
}

bool js_quickjs::value::is_number()
{
    if(!has_value)
        return false;

    return JS_IsNumber(val);
}

bool js_quickjs::value::is_array()
{
    if(!has_value)
        return false;

    return JS_IsArray(ctx, val);
}

bool js_quickjs::value::is_map()
{
    if(!has_value)
        return false;

    return JS_IsObject(val);
}

bool js_quickjs::value::is_empty()
{
    return !has_value;
}

bool js_quickjs::value::is_function()
{
    if(!has_value)
        return false;

    return JS_IsFunction(ctx, val);
}

bool js_quickjs::value::is_boolean()
{
    if(!has_value)
        return false;

    return JS_IsBool(val);
}

bool js_quickjs::value::is_undefined() const
{
    if(!has_value)
        return false;

    return JS_IsUndefined(val);
}

bool js_quickjs::value::is_truthy()
{
    if(!has_value)
        return false;

    return JS_ToBool(ctx, val) > 0;
}

bool js_quickjs::value::is_object_coercible()
{
    if(!has_value)
        return false;

    /*DUK_TYPE_MASK_BOOLEAN | \
      DUK_TYPE_MASK_NUMBER | \
      DUK_TYPE_MASK_STRING | \
      DUK_TYPE_MASK_OBJECT | \
      DUK_TYPE_MASK_BUFFER | \
      DUK_TYPE_MASK_POINTER | \
      DUK_TYPE_MASK_LIGHTFUNC + SYMBOL*/

    bool is_sym = JS_IsSymbol(val);

    return is_object() || is_boolean() || is_number() || is_function() || is_sym;
}

bool js_quickjs::value::is_object()
{
    if(!has_value)
        return false;

    return JS_IsObject(val);
}

/*
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
}*/

js_quickjs::qstack_manager::qstack_manager(js_quickjs::value& _val) : val(_val)
{
    if(val.has_value)
    {
        JS_FreeValue(val.ctx, val.val);
    }
}

///not exactly the same behaviour of duktape
///if i replace a parent object in duktape, children will relocate to that parent index
///here its actually object-y, so they'll refer to the old object
js_quickjs::qstack_manager::~qstack_manager()
{
    val.has_value = true;

    if(val.has_parent)
    {
        if(val.indices.index() == 0)
            assert(false);

        if(val.indices.index() == 1)
        {
            int idx = std::get<1>(val.indices);

            assert(idx >= 0);

            JSValue dp = JS_DupValue(val.ctx, val.val);

            JS_SetPropertyUint32(val.ctx, val.parent_value, idx, dp);
        }
        else
        {
            std::string idx = std::get<2>(val.indices);

            JSValue dp = JS_DupValue(val.ctx, val.val);

            JS_SetPropertyStr(val.ctx, val.parent_value, idx.c_str(), dp);
        }
    }
}

JSValue qarg::push(JSContext* ctx, const js_quickjs::value& in)
{
    if(!in.has_value)
        return JS_UNDEFINED;

    return JS_DupValue(ctx, in.val);
}

void qarg::get(js_quickjs::value_context& vctx, const JSValue& val, js_quickjs::value& out)
{
    if(JS_IsUndefined(val))
        return;

    out = val;
}

void qarg::get(js_quickjs::value_context& vctx, const JSValue& val, std::vector<std::pair<js_quickjs::value, js_quickjs::value>>& out)
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

    for(uint32_t i=0; i < len; i++)
    {
        JSAtom atom = names[0].atom;

        JSValue found = JS_GetProperty(vctx.ctx, val, atom);
        JSValue key = JS_AtomToValue(vctx.ctx, atom);

        js_quickjs::value out_key(vctx);
        js_quickjs::value out_value(vctx);

        out_key = key;
        out_value = found;

        out.push_back({out_key, out_value});
    }

    for(uint32_t i=0; i < len; i++)
    {
        JS_FreeAtom(vctx.ctx, names[i].atom);
    }

    js_free(vctx.ctx, names);
}

void qarg::get(js_quickjs::value_context& vctx, const JSValue& val, std::vector<js_quickjs::value>& out)
{
    UNDEF();

    out.clear();

    int len = 0;
    JS_GetPropertyStr(vctx.ctx, val, "length");

    out.reserve(len);

    for(int i=0; i < len; i++)
    {
        JSValue found = JS_GetPropertyUint32(vctx.ctx, val, i);

        js_quickjs::value next(vctx);
        next = found;

        out.push_back(next);
    }
}

js_quickjs::value& js_quickjs::value::operator=(const char* v)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, v);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(const std::string& v)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, v);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(int64_t v)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, v);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(int v)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, v);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(double v)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, v);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(bool v)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, v);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(std::nullopt_t v)
{
    if(!has_value)
        return *this;

    JS_FreeValue(ctx, val);
    has_value = false;

    if(has_parent)
    {
        JSAtom atom;

        if(indices.index() == 1)
            atom = JS_NewAtomUInt32(ctx, std::get<1>(indices));
        else if(indices.index() == 2)
            atom = JS_NewAtom(ctx, std::get<2>(indices).c_str());
        else
            throw std::runtime_error("Bad indices");

        JS_DeleteProperty(ctx, parent_value, atom, 0);

        JS_FreeAtom(ctx, atom);
    }

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(const value& right)
{
    if(!has_value && !right.has_value)
        return *this;

    qstack_manager m(*this);

    val = JS_DupValue(ctx, right.val);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(js::undefined_t)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, js::undefined);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(const nlohmann::json& in)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, in);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(const JSValue& _val)
{
    qstack_manager m(*this);

    val = JS_DupValue(ctx, _val);

    return *this;
}

js_quickjs::value::operator std::string()
{
    if(!has_value)
        return std::string();

    std::string ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator int64_t()
{
    if(!has_value)
        return int64_t();

    int64_t ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator int()
{
    if(!has_value)
        return int();

    int ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator double()
{
    if(!has_value)
        return double();

    double ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator bool()
{
    if(!has_value)
        return bool();

    bool ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value js_quickjs::value::operator[](int64_t arg)
{
    return js_quickjs::value(*vctx, *this, arg);
}

js_quickjs::value js_quickjs::value::operator[](const std::string& arg)
{
    return js_quickjs::value(*vctx, *this, arg);
}

js_quickjs::value js_quickjs::value::operator[](const char* arg)
{
    return js_quickjs::value(*vctx, *this, arg);
}

nlohmann::json js_quickjs::value::to_nlohmann()
{
    nlohmann::json built;

    if(is_object())
    {
        built = nlohmann::json::parse("{}");

        printf("Is object\n");

        std::vector<std::pair<js_quickjs::value, js_quickjs::value>> val = *this;

        std::cout << "Val size " << val.size() << std::endl;

        for(auto& i : val)
        {
            nlohmann::json keyn = (std::string)i.first;
            nlohmann::json valn = i.second.to_nlohmann();

            std::cout << "Fkey " << (std::string)keyn << std::endl;

            built[(std::string)keyn] = valn;
        }

        std::cout << "Got " << built.dump() << std::endl;

        return built;
    }

    if(is_array())
    {
        std::vector<js_quickjs::value> val = *this;

        nlohmann::json arr;

        for(int i=0; i < (int)val.size(); i++)
        {
            arr[i] = val[i].to_nlohmann();
        }

        return arr;
    }

    if(is_string())
    {
        return (std::string)*this;
    }

    if(is_number())
    {
        return (double)*this;
    }

    if(is_empty())
    {
        return nlohmann::json();
    }

    if(is_function())
    {
        return nlohmann::json();
    }

    if(is_boolean())
    {
        return (bool)*this;
    }

    if(is_undefined())
    {
        return nullptr;
    }

    throw std::runtime_error("No such json type");
}

std::string js_quickjs::value::to_json()
{
    return to_nlohmann().dump();
}

struct quickjs_tester
{
    quickjs_tester()
    {
        js_quickjs::value_context vctx;

        {
            js_quickjs::value val(vctx);

            js_quickjs::value dependent(vctx, val, "hello");
        }

        {
            js_quickjs::value val(vctx);
            val = 1234;

            assert((int)val == 1234);
        }

        {
            js_quickjs::value root(vctx);
            root["dep"] = "hello";

            std::string found = root["dep"];

            assert(found == "hello");

            std::cout << "Root dump " << root.to_json() << std::endl;
        }

        printf("Tested quickjs\n");
    }
};

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

            assert(res);

            std::string rvals = retval;

            printf("Found %s\n", rvals.c_str());

            assert(rvals == "poopy");

            printf("TOP %i\n", duk_get_top(ctx));

            assert(duk_get_top(ctx) == 8);
        }

        {
            js::value func(vctx);
            func = js::function<test_func_with_context>;

            js::value a2 = js::make_value(vctx, std::string("hello"));
            js::value a3 = js::make_value(vctx, 2345.);

            auto [res, retvalue] = js::call(func, a2, a3);

            std::cout << "FOUND " << (std::string)retvalue << std::endl;

            assert(res);

            double rval = retvalue;

            assert(rval == 256);
        }

        {
            js::value func(vctx);
            func = js::function<test_js_val>;

            js::value a3 = js::make_value(vctx, "hello");

            auto [res, va] = js::call(func, a3);

            std::cout << "FOUND2 " << (std::string)va << std::endl;

            assert(res);
        }

        {
            js::value glob = js::get_global(vctx);
            js::value func = js::add_key_value(glob, "test_error", js::function<test_error>);

            js::value a1(vctx);
            a1 = "asdf";

            auto [res, va] = js::call_prop(glob, "test_error", a1);

            assert(res);
        }

        {
            js::value glob = js::get_global(vctx);

            js::add_setter(glob, "test", js::function<test_set>);
            js::add_getter(glob, "test", js::function<test_get>);

            js::eval(vctx, "test = 1;");
            js::value result = js::eval(vctx, "var hello = test; hello;");

            assert((std::string)result == "got");
        }

        printf("Done js val testers\n");
    }
};

namespace
{
    js_val_tester tester;
    quickjs_tester qjstester;
}
