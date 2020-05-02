#include "argument_object_qjs.hpp"
#include "memory_sandbox.hpp"
#include "argument_object.hpp"

uint64_t value_to_key(const js_quickjs::value& root)
{
    void* ptr = root.val.u.ptr;

    uint64_t as_val = 0;
    memcpy(&as_val, &ptr, sizeof(ptr));

    return as_val;
}

uint64_t value_to_key(const JSValue& root)
{
    void* ptr = root.u.ptr;

    uint64_t as_val = 0;
    memcpy(&as_val, &ptr, sizeof(ptr));

    return as_val;
}

struct heap_stash
{
    sandbox_data* sandbox = nullptr;
    JSValue heap_stash_value;
    JSContext* ctx = nullptr;
    std::map<uint64_t, std::map<std::string, JSValue>> hidden_map;

    heap_stash(JSContext* global)
    {
        sandbox = new sandbox_data;
        heap_stash_value = JS_NewObject(global);
        ctx = global;
    }

    ~heap_stash()
    {
        JS_FreeValue(ctx, heap_stash_value);

        /*if(hidden_map.size() != 0)
        {
            printf("Hidden map failure\n");

            for(auto& i : hidden_map)
            {
                for(auto& j : i.second)
                {
                    std::cout << "Key " << j.first << std::endl;
                }
            }
        }

        assert(hidden_map.size() == 0);*/

        for(auto& i : hidden_map)
        {
            for(auto& j : i.second)
            {
                JS_FreeValue(ctx, j.second);
            }
        }
    }

    void add_hidden(const js_quickjs::value& root, const std::string& key, const js_quickjs::value& val)
    {
        if(!root.is_function() && !root.is_object())
            throw std::runtime_error("Must be function or array");

        uint64_t rkey = value_to_key(root);

        JSValue dup = JS_DupValue(root.ctx, val.val);

        hidden_map[rkey].emplace(key, dup);
    }

    bool has_hidden(const js_quickjs::value& root, const std::string& key)
    {
        uint64_t rkey = value_to_key(root);

        auto it = hidden_map.find(rkey);

        if(it == hidden_map.end())
            return false;

        auto it2 = it->second.find(key);

        if(it2 == it->second.end())
            return false;

        return true;
    }

    js_quickjs::value get_hidden(const js_quickjs::value& root, const std::string& key)
    {
        uint64_t rkey = value_to_key(root);

        auto it = hidden_map.find(rkey);

        if(it == hidden_map.end())
            throw std::runtime_error("No root in get_hidden");

        auto it2 = it->second.find(key);

        if(it2 == it->second.end())
            throw std::runtime_error("No key in get_hidden");

        js_quickjs::value val(*root.vctx);
        val = it2->second;
        return val;
    }

    void remove_hidden(const js_quickjs::value& root, const std::string& key)
    {
        uint64_t rkey = value_to_key(root);

        auto it = hidden_map.find(rkey);

        if(it == hidden_map.end())
            return;

        auto it2 = it->second.find(key);

        if(it2 == it->second.end())
            return;

        JS_FreeValue(root.ctx, it2->second);

        it->second.erase(it2);
    }

    void remove_all_hidden(const js_quickjs::value& root)
    {
        uint64_t rkey = value_to_key(root);

        auto it = hidden_map.find(rkey);

        if(it == hidden_map.end())
            return;

        for(auto& i : it->second)
        {
            JS_FreeValue(root.ctx, i.second);
        }

        hidden_map.erase(it);
    }

    void remove_all_hidden(JSContext* ctx, const JSValue& root)
    {
        uint64_t rkey = value_to_key(root);

        auto it = hidden_map.find(rkey);

        if(it == hidden_map.end())
            return;

        for(auto& i : it->second)
        {
            JS_FreeValue(ctx, i.second);
        }

        hidden_map.erase(it);
    }
};

struct global_stash
{
    JSValue global_stash_value;
    JSContext* ctx = nullptr;

    global_stash(JSContext* _ctx)
    {
        ctx = _ctx;
        global_stash_value = JS_NewObject(ctx);
    }

    ~global_stash()
    {
        JS_FreeValue(ctx, global_stash_value);
    }
};

void init_heap(JSContext* root)
{
    heap_stash* heap = new heap_stash(root);
    global_stash* stash = new global_stash(root);

    JS_SetContextOpaque(root, (void*)stash);
    JS_SetRuntimeOpaque(JS_GetRuntime(root), (void*)heap);
}

void init_context(JSContext* me)
{
    global_stash* stash = new global_stash(me);

    JS_SetContextOpaque(me, (void*)stash);
}

heap_stash* get_heap_stash(JSContext* ctx)
{
    return (heap_stash*)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
}

js_quickjs::value_context::value_context(JSContext* _ctx)
{
    ctx = _ctx;
    heap = JS_GetRuntime(ctx);
}

js_quickjs::value_context::value_context(value_context& other)
{
    heap = other.heap;
    ctx = JS_NewContext(heap);

    init_context(ctx);

    context_owner = true;
}

js_quickjs::value_context::value_context()
{
    heap = JS_NewRuntime();
    ctx = JS_NewContext(heap);

    init_heap(ctx);

    runtime_owner = true;
    context_owner = true;
}

js_quickjs::value_context::~value_context()
{
    if(context_owner)
    {
        global_stash* stash = (global_stash*)JS_GetContextOpaque(ctx);

        if(runtime_owner)
        {
            heap_stash* heaps = (heap_stash*)JS_GetRuntimeOpaque(heap);

            delete heaps;
        }

        delete stash;
        JS_FreeContext(ctx);
    }

    if(runtime_owner)
    {
        JS_FreeRuntime(heap);
    }
}

js_quickjs::value_context& js_quickjs::value_context::operator=(const value_context& other)
{
    assert(false);
    return *this;
}

void js_quickjs::value_context::push_this(const value& val)
{
    this_stack.push_back(val);
}

void js_quickjs::value_context::pop_this()
{
    assert(this_stack.size() > 0);

    this_stack.pop_back();
}

js_quickjs::value js_quickjs::value_context::get_current_this()
{
    if(this_stack.size() > 0)
        return this_stack.back();

    js_quickjs::value val(*this);
    val = js_quickjs::undefined;

    return val;
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
    if(!released && has_value)
    {
        if((is_function() || is_object()))
        {
            assert(JS_VALUE_HAS_REF_COUNT(val));

            JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(val);

            if(p->ref_count <= 1)
            {
                heap_stash* stash = get_heap_stash(ctx);

                stash->remove_all_hidden(*this);
            }
        }

        JS_FreeValue(ctx, val);
    }

    if(has_parent)
    {
        if(JS_VALUE_HAS_REF_COUNT(parent_value))
        {
            JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(parent_value);

            if(p->ref_count <= 1)
            {
                heap_stash* stash = get_heap_stash(ctx);

                stash->remove_all_hidden(ctx, parent_value);
            }
        }

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

bool js_quickjs::value::has_hidden(const std::string& key) const
{
    if(!has_value)
        return false;

    if(is_undefined())
        return false;

    heap_stash* stash = get_heap_stash(ctx);

    return stash->has_hidden(*this, key);
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

js_quickjs::value js_quickjs::value::get_hidden(const std::string& key)
{
    if(!has_hidden(key))
    {
        js_quickjs::value val(*vctx);
        val = js_quickjs::undefined;
        return val;
    }

    heap_stash* heap = get_heap_stash(ctx);

    return heap->get_hidden(*this, key);
}

bool js_quickjs::value::del(const std::string& key)
{
    if(!has(key))
        return false;

    JSAtom atom = JS_NewAtomLen(ctx, key.c_str(), key.size());

    JS_DeleteProperty(ctx, val, atom, 0);

    JS_FreeAtom(ctx, atom);

    return true;
}

void js_quickjs::value::add_hidden_value(const std::string& key, const value& val)
{
    heap_stash* heap = get_heap_stash(ctx);

    heap->add_hidden(*this, key, val);
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

bool js_quickjs::value::is_function() const
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

    return is_object() || is_boolean() || is_number() || is_function() || is_sym || is_string();
}

bool js_quickjs::value::is_object() const
{
    if(!has_value)
        return false;

    return JS_IsObject(val);
}

bool js_quickjs::value::is_error() const
{
    if(!has_value)
        return false;

    return JS_IsError(ctx, val);
}

bool js_quickjs::value::is_exception() const
{
    if(!has_value)
        return false;

    return JS_IsException(val);
}

void js_quickjs::value::release()
{
    released = true;
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
        heap_stash* stash = get_heap_stash(_val.ctx);

        stash->remove_all_hidden(_val);

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

    JSValue jslen = JS_GetPropertyStr(vctx.ctx, val, "length");

    int32_t len = 0;
    JS_ToInt32(vctx.ctx, &len, jslen);

    JS_FreeValue(vctx.ctx, jslen);

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

js_quickjs::value& js_quickjs::value::operator=(js_quickjs::undefined_t)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, js_quickjs::undefined);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(const nlohmann::json& in)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, in);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(js_quickjs::funcptr_t in)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, in);

    return *this;
}

js_quickjs::value& js_quickjs::value::operator=(const JSValue& in)
{
    qstack_manager m(*this);

    val = qarg::push(ctx, in);

    return *this;
}


void js_quickjs::value::stringify_parse()
{
    std::string json = to_json();

    JS_FreeValue(ctx, val);

    val = JS_ParseJSON(ctx, json.c_str(), json.size(), nullptr);
}

js_quickjs::value::operator std::string() const
{
    if(!has_value)
        return std::string();

    std::string ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator int64_t() const
{
    if(!has_value)
        return int64_t();

    int64_t ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator int() const
{
    if(!has_value)
        return int();

    int ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator double() const
{
    if(!has_value)
        return double();

    double ret;
    qarg::get(*vctx, val, ret);

    return ret;
}

js_quickjs::value::operator bool() const
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
    JSValue jval = JS_JSONStringify(ctx, val, JS_UNDEFINED, JS_UNDEFINED);

    value sval(*vctx);
    sval = jval;

    nlohmann::json ret = nlohmann::json::parse((std::string)sval);

    JS_FreeValue(ctx, jval);

    return ret;
}

std::string js_quickjs::value::to_json()
{
    JSValue jval = JS_JSONStringify(ctx, val, JS_UNDEFINED, JS_UNDEFINED);

    value sval(*vctx);
    sval = jval;

    JS_FreeValue(ctx, jval);

    return (std::string)sval;
}

std::string js_quickjs::value::to_error_message()
{
    std::string err = "Error:\n";

    if(has("stack"))
    {
        err += "Stack: " + (std::string)get("stack");
    }

    if(has("message"))
    {
        err += "Message: " + (std::string)get("message") + "\n";
    }

    if(has("lineNumber"))
    {
        err += "lineNumber: " + (std::string)get("lineNumber");
    }

    if(has("columnNumber"))
    {
        err += "columnNumber: " + (std::string)get("columnNumber");
    }

    return err;
}

js_quickjs::value js_quickjs::get_global(js_quickjs::value_context& vctx)
{
    js_quickjs::value val(vctx);
    JSValue found = JS_GetGlobalObject(vctx.ctx);

    val = found;

    JS_FreeValue(vctx.ctx, found);

    return val;
}

void js_quickjs::set_global(js_quickjs::value_context& vctx, const js_quickjs::value& val)
{
    ///UNIMPLEMENTED BUT NOT THE END OF THE WORLD
    //JS_SetGlobal
}

js_quickjs::value js_quickjs::get_current_function(js_quickjs::value_context& vctx)
{
    JSValue rval = JS_GetActiveFunction(vctx.ctx);

    js_quickjs::value ret(vctx);

    ret = rval;

    return ret;
}

js_quickjs::value js_quickjs::get_this(js_quickjs::value_context& vctx)
{
    return vctx.get_current_this();
}

js_quickjs::value js_quickjs::get_heap_stash(js_quickjs::value_context& vctx)
{
    heap_stash* stash = (heap_stash*)JS_GetRuntimeOpaque(JS_GetRuntime(vctx.ctx));

    js_quickjs::value ret(vctx);
    ret = stash->heap_stash_value;

    return ret;
}

js_quickjs::value js_quickjs::get_global_stash(js_quickjs::value_context& vctx)
{
    global_stash* stash = (global_stash*)JS_GetContextOpaque(vctx.ctx);

    js_quickjs::value ret(vctx);
    ret = stash->global_stash_value;

    return ret;
}

void* js_quickjs::get_sandbox_data_impl(js_quickjs::value_context& vctx)
{
    heap_stash* stash = (heap_stash*)JS_GetRuntimeOpaque(JS_GetRuntime(vctx.ctx));

    return (void*)stash->sandbox;
}

js_quickjs::value js_quickjs::add_getter(js_quickjs::value& base, const std::string& key, js_quickjs::funcptr_t func)
{
    js_quickjs::value val(*base.vctx);
    val = func;

    JSAtom str = JS_NewAtomLen(base.ctx, key.c_str(), key.size());

    JS_DefineProperty(base.ctx, base.val, str, JS_UNDEFINED, val.val, JS_UNDEFINED, JS_PROP_HAS_GET | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE);

    JS_FreeAtom(base.ctx, str);

    return val;
}

js_quickjs::value js_quickjs::add_setter(js_quickjs::value& base, const std::string& key, js_quickjs::funcptr_t func)
{
    js_quickjs::value val(*base.vctx);
    val = func;

    JSAtom str = JS_NewAtomLen(base.ctx, key.c_str(), key.size());

    JS_DefineProperty(base.ctx, base.val, str, JS_UNDEFINED, JS_UNDEFINED, val.val, JS_PROP_HAS_SET | JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_ENUMERABLE);

    JS_FreeAtom(base.ctx, str);

    return val;
}

std::pair<bool, js_quickjs::value> js_quickjs::call_compiled(value& bitcode)
{
    JSValue ret = JS_EvalFunction(bitcode.ctx, JS_DupValue(bitcode.ctx, bitcode.val));

    value val(*bitcode.vctx);
    val = ret;

    bool err = JS_IsError(bitcode.ctx, ret) || JS_IsException(ret);

    JS_FreeValue(bitcode.ctx, ret);

    if(err)
    {
        JSValue err_val = JS_GetException(bitcode.ctx);

        val = err_val;

        JS_FreeValue(bitcode.ctx, err_val);
    }

    return {!err, val};
}

std::pair<bool, js_quickjs::value> js_quickjs::compile(value_context& vctx, const std::string& data)
{
    return compile(vctx, data, "unnamed");
}

std::pair<bool, js_quickjs::value> js_quickjs::compile(value_context& vctx, const std::string& data, const std::string& name)
{
    JSValue ret = JS_Eval(vctx.ctx, data.c_str(), data.size(), name.c_str(), JS_EVAL_FLAG_COMPILE_ONLY);

    js_quickjs::value val(vctx);
    val = ret;

    JS_FreeValue(vctx.ctx, ret);

    bool err = JS_IsException(val.val) || JS_IsError(vctx.ctx, val.val);

    if(err)
    {
        JSValue err_val = JS_GetException(vctx.ctx);

        val = err_val;

        JS_FreeValue(vctx.ctx, err_val);
    }

    return {!err, val};
}

namespace js_quickjs
{

std::string dump_function(value& val)
{
    size_t size = 0;
    uint8_t* out = JS_WriteObject(val.ctx, &size, val.val, JS_WRITE_OBJ_BYTECODE);

    return std::string(out, out + size);
}

value eval(value_context& vctx, const std::string& data)
{
    JSValue ret = JS_Eval(vctx.ctx, data.c_str(), data.size(), "test-eval", 0);

    value rval(vctx);
    rval = ret;

    JS_FreeValue(vctx.ctx, ret);

    if(rval.is_error() || rval.is_exception())
    {
        JSValue err_val = JS_GetException(vctx.ctx);

        rval = err_val;

        JS_FreeValue(vctx.ctx, err_val);
    }

    return rval;
}

value xfer_between_contexts(value_context& destination, const value& val)
{
    value next(destination);
    next = val.val;

    return next;
}

value sanitise_xfer_between_contexts(value_context& destination, const value& val)
{
    return xfer_between_contexts(destination, val);
}

value make_proxy(value& target, value& handle)
{
    JSValue arr[2] = {target.val, handle.val};

    JSValue val = js_proxy_constructor(target.ctx, JS_UNDEFINED, 2, arr);

    value ret(*target.vctx);
    ret = val;

    JS_FreeValue(target.ctx, val);

    return ret;
}


value from_cbor(value_context& vctx, const std::vector<uint8_t>& cb);

void dump_stack(value_context& vctx)
{
    printf("Stack tracing unimplemented\n");
}
}

void empty_func(js_quickjs::value_context* vctx)
{

}

struct quickjs_tester
{
    quickjs_tester()
    {
        printf("Testing quick\n");

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

        printf("hidden 1\n");

        {
            js_quickjs::value root(vctx);

            root.add_hidden("hello", 1234);

            assert(root.has_hidden("hello"));
            assert(!root.has_hidden("hello2"));

            int val = root.get_hidden("hello");

            assert(val == 1234);
        }

        printf("hidden 2\n");

        {
            js_quickjs::value root(vctx);

            root.add_hidden("hello", "yep");
            root.add_hidden("hello2", "hurro");

            std::string str = root.get_hidden("hello");

            assert(str == "yep");
        }

        printf("hidden 3\n");

        {
            js_quickjs::value root(vctx);
            root = js_quickjs::function<empty_func>;

            root.add_hidden("checky", "yes.hello");

            std::string val = root.get_hidden("checky");

            assert(val == "yes.hello");
        }

        printf("hidden 4\n");

        {
            js_quickjs::value root(vctx);

            auto val = root.add("hello", js_quickjs::function<empty_func>);
            val.add_hidden("testy", "hellothere");
        }

        printf("call\n");

        {
            js_quickjs::value func(vctx);
            func = js_quickjs::function<empty_func>;

            auto [success, res] = js_quickjs::call(func);

            assert(success);
        }

        printf("compile\n");

        {
            auto [success, res] = js_quickjs::compile(vctx, "1+1", "none");

            assert(success);

            ///not sure this will work for compiled scripts
            JSValue ret = JS_EvalFunction(vctx.ctx, JS_DupValue(vctx.ctx, res.val));

            bool err = JS_IsError(vctx.ctx, ret) || JS_IsException(ret);

            JS_FreeValue(vctx.ctx, ret);

            assert(!err);
        }

        {
            js_quickjs::value root(vctx);

            root.add("Hello", "1234");

            assert(root.has("Hello"));

            std::string val = root.get("Hello");

            assert(val == "1234");
        }

        {
            js_quickjs::value root(vctx);

            std::vector<std::string> val{"hi", "hello"};

            root = val;

            std::vector<std::string> rval = root;

            assert(rval.size() == 2);

            assert(rval[0] == "hi");
            assert(rval[1] == "hello");
        }

        {
            int some_ptr = 0;

            js_quickjs::value root(vctx);
            root.set_ptr(&some_ptr);

            int* fptr = root.get_ptr<int>();

            assert(&some_ptr == fptr);

            assert(fptr);

            assert(*fptr == some_ptr);
        }

        {
            js_quickjs::value root(vctx);

            root["hi"] = "hello";

            std::string json = root.to_json();

            assert(json.size() > 0);
        }

        {
            js_quickjs::value glob = js_quickjs::get_global(vctx);

            std::cout << "my global " << glob.to_json() << std::endl;

            glob["hi"] = "hello";

            assert(glob.has("hi") && ((std::string)glob["hi"]) == "hello");

            std::cout << "New global " << glob.to_json() << std::endl;

            assert(glob.has("globalThis"));
        }

        printf("Tested quickjs\n");

        //exit(0);
    }
};


namespace
{
    quickjs_tester qjstester;
}

