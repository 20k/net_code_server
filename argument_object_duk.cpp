#include "argument_object_duk.hpp"

#include "duktape.h"
#include <iostream>
#include "memory_sandbox.hpp"
#include "argument_object.hpp"

void arg::dukx_push(duk_context* ctx, const js_duk::value& val)
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

js_duk::value_context::value_context(context_t* _ctx) : ctx(_ctx)
{

}

js_duk::value_context::value_context(js_duk::value_context& octx) : parent_context(&octx)
{
    duk_idx_t fidx = duk_push_thread_new_globalenv(octx.ctx);
    ctx = duk_get_context(octx.ctx, fidx);
    parent_idx = fidx;
}

js_duk::value_context::value_context()
{
    ctx = create_sandbox_heap();
    owner = true;
}

js_duk::value_context::~value_context()
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

void js_duk::value_context::free(int idx)
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

stack_manage::stack_manage(js_duk::value& in) : sh(in)
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

/*void dukx_push(duk_context* ctx, const js_duk::value& v)
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

js_duk::value& js_duk::value::operator=(const char* v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js_duk::value& js_duk::value::operator=(const std::string& v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js_duk::value& js_duk::value::operator=(int64_t v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js_duk::value& js_duk::value::operator=(int v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js_duk::value& js_duk::value::operator=(double v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js_duk::value& js_duk::value::operator=(bool v)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, v);

    return *this;
}

js_duk::value& js_duk::value::operator=(std::nullopt_t t)
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

js_duk::value& js_duk::value::operator=(const value& right)
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

js_duk::value& js_duk::value::operator=(js_duk::funcptr_t fptr)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, fptr);

    return *this;
}

js_duk::value& js_duk::value::operator=(js_duk::undefined_t)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, js_duk::undefined);

    return *this;
}

js_duk::value& js_duk::value::operator=(const nlohmann::json& in)
{
    stack_manage m(*this);

    arg::dukx_push(ctx, in);

    return *this;
}

js_duk::value::value(const js_duk::value& value)
{
    vctx = value.vctx;
    ctx = value.ctx;
    parent_idx = value.parent_idx;
    indices = value.indices;

    duk_dup(ctx, value.idx);
    idx = duk_get_top_index(ctx);
}

js_duk::value::value(js_duk::value_context& vctx, const value& other) : js_duk::value::value(other)
{

}

js_duk::value::value(js_duk::value&& other)
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

js_duk::value& js_duk::value::operator=(js_duk::value&& other)
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

js_duk::value::value(js_duk::value_context& _vctx) : vctx(&_vctx), ctx(_vctx.ctx)
{
    idx = duk_push_object(ctx);
}

js_duk::value::value(js_duk::value_context& _vctx, int _idx) : vctx(&_vctx), ctx(_vctx.ctx), idx(_idx)
{
    if(idx < 0)
    {
        idx = duk_get_top_index(ctx);

        if(idx < 0)
            throw std::runtime_error("bad idx < 0");
    }
}

js_duk::value::value(js_duk::value_context& _vctx, const js_duk::value& base, const std::string& key) : vctx(&_vctx), ctx(_vctx.ctx)
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

js_duk::value::value(js_duk::value_context& _vctx, const js_duk::value& base, int key) : vctx(&_vctx), ctx(_vctx.ctx)
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

js_duk::value::value(js_duk::value_context& _vctx, const js_duk::value& base, const char* key) : vctx(&_vctx), ctx(_vctx.ctx)
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

js_duk::value::~value()
{
    if(idx != -1 && !released)
    {
        vctx->free(idx);
    }
}

std::string js_duk::value::to_json()
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

std::vector<uint8_t> js_duk::value::to_cbor()
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

void js_duk::value::stringify_parse()
{
    duk_json_encode(ctx, idx);
    duk_json_decode(ctx, idx);
}

js_duk::value js_duk::value::operator[](int64_t val)
{
    return js_duk::value(*vctx, *this, val);
}

js_duk::value js_duk::value::operator[](const std::string& val)
{
    return js_duk::value(*vctx, *this, val);
}

js_duk::value js_duk::value::operator[](const char* val)
{
    return js_duk::value(*vctx, *this, val);
}

bool js_duk::value::has(const std::string& key) const
{
    if(idx == -1)
        return false;

    if(is_undefined())
        return false;

    return duk_has_prop_lstring(ctx, idx, key.c_str(), key.size());
}

bool js_duk::value::has(int key) const
{
    if(idx == -1)
        return false;

    if(is_undefined())
        return false;

    return duk_has_prop_index(ctx, idx, key);
}

bool js_duk::value::has(const char* key) const
{
    if(idx == -1)
        return false;

    if(is_undefined())
        return false;

    return duk_has_prop_string(ctx, idx, key);
}

bool js_duk::value::has_hidden(const std::string& key) const
{
    if(idx == -1)
       return false;

    if(is_undefined())
        return false;

    std::string rkey = "\xFF" + key;

    return has(rkey);
}

js_duk::value js_duk::value::get(const std::string& key)
{
    //if(!has(key))
    //    return js_duk::make_value(*vctx, std::nullopt);

    return js_duk::value(*vctx, *this, key);
}

js_duk::value js_duk::value::get(int key)
{
    //if(!has(key))
    //    return js_duk::make_value(*vctx, std::nullopt);

    return js_duk::value(*vctx, *this, key);
}

js_duk::value js_duk::value::get(const char* key)
{
    //if(!has(key))
    //    return js_duk::make_value(*vctx, std::nullopt);

    return js_duk::value(*vctx, *this, key);
}

js_duk::value js_duk::value::get_hidden(const std::string& key)
{
    //if(!has_hidden(key))
    //    return js_duk::make_value(*vctx, std::nullopt);

    std::string rkey = "\xFF" + key;

    return js_duk::value(*vctx, *this, rkey);
}

bool js_duk::value::del(const std::string& key)
{
    if(!has(key))
        return false;

    js_duk::value val(*vctx, *this, key);
    val = std::nullopt;
    return true;
}

bool js_duk::value::is_string()
{
    if(idx == -1)
        return false;

    return duk_is_string(ctx, idx);
}

bool js_duk::value::is_number()
{
    if(idx == -1)
        return false;

    return duk_is_number(ctx, idx);
}

bool js_duk::value::is_array()
{
    if(idx == -1)
        return false;

    return duk_is_array(ctx, idx);
}

bool js_duk::value::is_map()
{
    if(idx == -1)
        return false;

    return duk_is_object(ctx, idx);
}

bool js_duk::value::is_empty()
{
    return idx == -1;
}

bool js_duk::value::is_function()
{
    if(idx == -1)
        return false;

    return duk_is_function(ctx, idx);
}

bool js_duk::value::is_boolean()
{
    if(idx == -1)
        return false;

    return duk_is_boolean(ctx, idx);
}

bool js_duk::value::is_undefined() const
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

bool js_duk::value::is_truthy()
{
    if(idx == -1)
        return false;

    return ::is_truthy(ctx, idx);
}

bool js_duk::value::is_object_coercible()
{
    if(idx == -1)
        return false;

    return duk_is_object_coercible(ctx, idx);
}

bool js_duk::value::is_object()
{
    if(idx == -1)
        return false;

    return duk_is_object(ctx, idx);
}

void js_duk::value::release()
{
    released = true;
}

js_duk::value js_duk::get_global(js_duk::value_context& vctx)
{
    duk_push_global_object(vctx.ctx);

    return js_duk::value(vctx, -1);
}

void js_duk::set_global(js_duk::value_context& vctx, const js_duk::value& val)
{
    duk_dup(vctx.ctx, val.idx);
    duk_set_global_object(vctx.ctx);
}

js_duk::value js_duk::get_current_function(js_duk::value_context& vctx)
{
    duk_push_current_function(vctx.ctx);

    return js_duk::value(vctx, -1);
}

js_duk::value js_duk::get_this(js_duk::value_context& vctx)
{
    duk_push_this(vctx.ctx);

    return js_duk::value(vctx, -1);
}

js_duk::value js_duk::get_heap_stash(js_duk::value_context& vctx)
{
    duk_push_heap_stash(vctx.ctx);

    return js_duk::value(vctx, -1);
}

js_duk::value js_duk::get_global_stash(js_duk::value_context& vctx)
{
    duk_push_global_stash(vctx.ctx);

    return js_duk::value(vctx, -1);
}

void* js_duk::get_sandbox_data_impl(value_context& vctx)
{
    duk_memory_functions mem_funcs_duk; duk_get_memory_functions(vctx.ctx, &mem_funcs_duk);
    return mem_funcs_duk.udata;
}

void js_duk::dump_stack(js_duk::value_context& vctx)
{
    duk_push_context_dump(vctx.ctx);

    js_duk::value val(vctx, -1);

    std::cout << "GOT " << (std::string)val << std::endl;
}

js_duk::value js_duk::add_setter(js_duk::value& base, const std::string& key, js_duk::funcptr_t func)
{
    js_duk::value val(*base.vctx);
    val = func;

    duk_push_lstring(base.ctx, key.c_str(), key.size());
    duk_dup(base.ctx, val.idx);

    duk_def_prop(base.ctx, base.idx, DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_FORCE);

    return val;
}

js_duk::value js_duk::add_getter(js_duk::value& base, const std::string& key, js_duk::funcptr_t func)
{
    js_duk::value val(*base.vctx);
    val = func;

    duk_push_lstring(base.ctx, key.c_str(), key.size());
    duk_dup(base.ctx, val.idx);

    duk_def_prop(base.ctx, base.idx, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_FORCE);

    return val;
}

std::pair<bool, js_duk::value> js_duk::compile(js_duk::value_context& vctx, const std::string& data)
{
    return compile(vctx, data, "test-name");
}

std::pair<bool, js_duk::value> js_duk::compile(js_duk::value_context& vctx, const std::string& data, const std::string& name)
{
    duk_push_string(vctx.ctx, data.c_str());
    duk_push_string(vctx.ctx, name.c_str());

    bool success = duk_pcompile(vctx.ctx, DUK_COMPILE_EVAL) == 0;

    return {success, js_duk::value(vctx, -1)};
}

std::string js_duk::dump_function(js_duk::value& val)
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

js_duk::value js_duk::eval(js_duk::value_context& vctx, const std::string& data)
{
    duk_eval_string(vctx.ctx, data.c_str());

    return js_duk::value(vctx, -1);
}

js_duk::value js_duk::xfer_between_contexts(js_duk::value_context& destination, const js_duk::value& val)
{
    if(destination.ctx == val.ctx)
        throw std::runtime_error("Bad same contexts");

    duk_dup(val.ctx, val.idx);

    duk_xmove_top(destination.ctx, val.ctx, 1);

    return js_duk::value(destination, -1);
}

void js_duk::value::pack()
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

js_duk::value js_duk::make_proxy(js_duk::value& target, js_duk::value& handle)
{
    duk_dup(target.ctx, target.idx);
    duk_dup(handle.ctx, handle.idx);

    duk_push_proxy(target.ctx, 0);

    return js_duk::value(*target.vctx, -1);
}

js_duk::value js_duk::from_cbor(js_duk::value_context& vctx, const std::vector<uint8_t>& cb)
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

    return js_duk::value(vctx, -1);
}

void test_func()
{

}

void test_func_with_args(int one, std::string two)
{

}

void test_set(js_duk::value_context* vctx, js_duk::value val)
{
    std::cout << "GOT " << (std::string)val << std::endl;
}

js_duk::value test_get(js_duk::value_context* vctx)
{
    js_duk::value test_ret(*vctx);
    test_ret = "got";
    return test_ret;
}

std::string test_func_with_return(double one, std::string two)
{
    printf("Called with %lf %s\n", one, two.c_str());

    return "poopy";
}

double test_func_with_context(js_duk::value_context* ctx, std::string one, double two)
{
    assert(ctx);

    printf("Called with %s %lf\n", one.c_str(), two);

    return 256;
}

js_duk::value test_js_val(js_duk::value_context* ctx, js_duk::value val)
{
    return js_duk::value(*ctx);
}

std::string test_error(js_duk::value_context* vctx, js_duk::value val)
{
    return "hi";
}


struct js_val_tester
{
    js_val_tester()
    {
        printf("Duk\n");

        duk_context* ctx = duk_create_heap_default();

        js_duk::value_context vctx(ctx);

        js_duk::value val(vctx);
        val = (int64_t)53;

        assert((int64_t)val == 53);

        assert(duk_get_top(ctx) == 1);

        js_duk::value root(vctx);
        js_duk::value base(vctx, root, "hello");
        base = (int64_t)53;

        assert((int64_t)root["hello"] == 53);

        assert(duk_get_top(ctx) == 3);

        duk_push_object(ctx);

        duk_push_string(ctx, "key");
        duk_push_string(ctx, "value");

        duk_put_prop(ctx, -3);

        js_duk::value tobj(vctx, -1);

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

        js_duk::value fmap(vctx);
        fmap = test_object;

        assert((int64_t)fmap["hithere"] == 12);
        assert((int64_t)fmap["pooper"] == 55);

        std::map<std::string, int> out_map = fmap;

        assert(out_map.size() == 2);
        assert(out_map["hithere"] == 12);
        assert(out_map["pooper"] == 55);

        assert(duk_get_top(ctx) == 4);

        {
            js_duk::value func(vctx);
            func = js_duk::function<test_func_with_return>;

            js_duk::value a1(vctx);
            a1 = 12;

            js_duk::value some_string(vctx);
            some_string = "poopersdf";

            assert(duk_get_top(ctx) == 7);

            auto [res, retval] = js_duk::call(func, a1, some_string);

            assert(res);

            std::string rvals = retval;

            printf("Found %s\n", rvals.c_str());

            assert(rvals == "poopy");

            printf("TOP %i\n", duk_get_top(ctx));

            assert(duk_get_top(ctx) == 8);
        }

        {
            js_duk::value func(vctx);
            func = js_duk::function<test_func_with_context>;

            js_duk::value a2 = js_duk::make_value(vctx, std::string("hello"));
            js_duk::value a3 = js_duk::make_value(vctx, 2345.);

            auto [res, retvalue] = js_duk::call(func, a2, a3);

            std::cout << "FOUND " << (std::string)retvalue << std::endl;

            assert(res);

            double rval = retvalue;

            assert(rval == 256);
        }

        {
            js_duk::value func(vctx);
            func = js_duk::function<test_js_val>;

            js_duk::value a3 = js_duk::make_value(vctx, "hello");

            auto [res, va] = js_duk::call(func, a3);

            std::cout << "FOUND2 " << (std::string)va << std::endl;

            assert(res);
        }

        {
            js_duk::value glob = js_duk::get_global(vctx);
            js_duk::value func = js_duk::add_key_value(glob, "test_error", js_duk::function<test_error>);

            js_duk::value a1(vctx);
            a1 = "asdf";

            auto [res, va] = js_duk::call_prop(glob, "test_error", a1);

            assert(res);
        }

        {
            js_duk::value glob = js_duk::get_global(vctx);

            js_duk::add_setter(glob, "test", js_duk::function<test_set>);
            js_duk::add_getter(glob, "test", js_duk::function<test_get>);

            js_duk::eval(vctx, "test = 1;");
            js_duk::value result = js_duk::eval(vctx, "var hello = test; hello;");

            assert((std::string)result == "got");
        }

        printf("Done js val testers\n");
    }
};

namespace
{
    js_val_tester tester;
}
