#include "argument_object_qjs.hpp"
#include "memory_sandbox.hpp"
#include "argument_object.hpp"

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
        JSAtom atom = names[i].atom;

        JSValue found = JS_GetProperty(vctx.ctx, val, atom);
        JSValue key = JS_AtomToValue(vctx.ctx, atom);

        js_quickjs::value out_key(vctx);
        js_quickjs::value out_value(vctx);

        out_key = key;
        out_value = found;

        out.push_back({out_key, out_value});

        JS_FreeValue(vctx.ctx, found);
        JS_FreeValue(vctx.ctx, key);
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

        JS_FreeValue(vctx.ctx, found);

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

nlohmann::json js_quickjs::value::to_nlohmann(int stack_depth)
{
    if(stack_depth > 100)
        throw std::runtime_error("Exceeded to_nlohmann stack");

    nlohmann::json built;

    if(is_object())
    {
        built = nlohmann::json::parse("{}");

        std::vector<std::pair<js_quickjs::value, js_quickjs::value>> val = *this;

        for(auto& i : val)
        {
            nlohmann::json keyn = (std::string)i.first;
            nlohmann::json valn = i.second.to_nlohmann(stack_depth+1);

            built[(std::string)keyn] = valn;
        }

        return built;
    }

    if(is_array())
    {
        std::vector<js_quickjs::value> val = *this;

        nlohmann::json arr;

        for(int i=0; i < (int)val.size(); i++)
        {
            arr[i] = val[i].to_nlohmann(stack_depth+1);
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

        {
            js_quickjs::value root(vctx);
            root["hi"] = "hellothere";
            root["yep"] = "test";
            root["cat"] = 1234;

            js_quickjs::value subobj(vctx);
            subobj["further_sub"] = "nope";

            root["super_sub"] = subobj;

            std::string found_1 = root["hi"];
            std::string found_2 = root["yep"];
            std::string found_3 = root["testsub"];

            assert(found_1 == "hellothere");
            assert(found_2 == "test");

            std::cout << "Dumped " << root.to_json() << std::endl;
        }

        printf("Tested quickjs\n");
    }
};


namespace
{
    quickjs_tester qjstester;
}

