#include "js_ui.hpp"
#include "command_handler_state.hpp"
#include <cmath>
#include "rate_limiting.hpp"
#include <tuple>
#include "db_storage_backend.hpp"

namespace
{
using ImU32 = uint32_t;

///taken from ImGui
const ImU32 GCrc32LookupTable[256] =
{
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
};

#define MAX_STR_SIZE 5000

ImU32 ImHashStr(const char* data_p, size_t data_size, ImU32 seed)
{
    seed = ~seed;
    ImU32 crc = seed;
    const unsigned char* data = (const unsigned char*)data_p;
    const ImU32* crc32_lut = GCrc32LookupTable;
    if (data_size != 0)
    {
        while (data_size-- != 0)
        {
            unsigned char c = *data++;
            if (c == '#' && data_size >= 2 && data[0] == '#' && data[1] == '#')
                crc = seed;
            crc = (crc >> 8) ^ crc32_lut[(crc & 0xFF) ^ c];
        }
    }
    else
    {
        while (unsigned char c = *data++)
        {
            if (c == '#' && data[0] == '#' && data[1] == '#')
                crc = seed;
            crc = (crc >> 8) ^ crc32_lut[(crc & 0xFF) ^ c];
        }
    }
    return ~crc;
}
}

std::string generate_unique_id()
{
    size_t id = db_storage_backend::get_unique_id();

    return std::to_string(id) + "##idboy";
}

std::string sanitise_value(const std::string& str)
{
    auto hashed = ImHashStr(str.c_str(), str.size(), 0);

    return str + "###somesalt" + std::to_string(hashed) + "doot";
}

bool too_large(js_ui::ui_stack& stk)
{
    uint64_t sum = stk.current_size;

    for(uint64_t idx = stk.current_idx; idx < stk.elements.size(); idx++)
    {
        sum += stk.elements[idx].type.size();
        sum += stk.elements[idx].element_id.size();
    }

    stk.current_idx = stk.elements.size();
    stk.current_size = sum;

    return sum >= 1024 * 1024 * 10;
}

double san_col(double in)
{
    if(!std::isfinite(in))
        return 0;

    return clamp(in, 0., 1.);
}

double san_val(double in)
{
    if(!std::isfinite(in))
        return 0;

    return in;
}

double san_clamp(double in)
{
    in = san_val(in);

    in = clamp(in, -9999, 9999);

    return in;
}

std::optional<std::pair<ui_element_state*, std::unique_lock<lock_type_t>>> get_named_element(js::value_context& vctx, const std::string& name)
{
    command_handler_state* found_ptr = js::get_heap_stash(vctx)["command_handler_state_pointer"].get_ptr<command_handler_state>();

    if(found_ptr == nullptr)
        return std::nullopt;

    if(!js::get_heap_stash(vctx).has("realtime_id"))
        return std::nullopt;

    int realtime_id = js::get_heap_stash(vctx)["realtime_id"];

    std::unique_lock<lock_type_t> guard(found_ptr->script_data_lock);

    auto realtime_it = found_ptr->script_data.find(realtime_id);

    if(realtime_it == found_ptr->script_data.end())
        return std::nullopt;

    realtime_script_data& dat = realtime_it->second;

    if(dat.realtime_ui.element_states.find(name) == dat.realtime_ui.element_states.end())
        return std::nullopt;

    ui_element_state& st = dat.realtime_ui.element_states[name];

    return std::pair<ui_element_state*, std::unique_lock<lock_type_t>>{&st, std::move(guard)};
}

template<int N>
std::pair<std::array<js::value, N>, bool> replace_args(std::array<js::value, N> vals, js::value_context& vctx, const std::string& name)
{
    std::optional value_opt = get_named_element(vctx, name);

    if(!value_opt.has_value())
        return {vals, false};

    ui_element_state& ui_state = *value_opt.value().first;

    if(!ui_state.client_override_arguments.is_array())
        return {vals, false};

    std::vector<nlohmann::json> arr_args = ui_state.client_override_arguments;

    if((int)arr_args.size() != N)
        return {vals, false};

    for(int i=0; i < (int)arr_args.size(); i++)
    {
        vals[i] = ui_state.client_override_arguments[i];
    }

    ui_state.client_override_arguments = nlohmann::json();

    return {vals, true};
}

namespace process
{
    void colour(double& in)
    {
        in = san_col(in);
        in = round(in * 255) / 255.;
    }

    void float_value(double& in)
    {
        in = san_val(in);
        in = clamp(in, -FLT_MAX/INT_MAX, FLT_MAX/INT_MAX);
    }

    void double_value(double& in)
    {
        in = san_val(in);
        in = clamp(in, -DBL_MAX, DBL_MAX);
    }

    void int_value(double& in)
    {
        in = san_val(in);
        in = clamp(in, INT_MIN, INT_MAX);
    }

    void dimension(double& in)
    {
        in = san_clamp(in);
    }

    void positive_dimension(double& in)
    {
        in = san_val(in);
        in = clamp(in, 0., 9999.);
    }

    void fraction(double& in)
    {
        in = san_val(in);
        in = clamp(in, 0., 1.);
    }

    void id(std::string& in)
    {
        in = sanitise_value(in);
    }

    bool inout_ref(js::value_context& vctx, js::value& val, const std::string& id)
    {
        if(!val.has("v"))
            throw std::runtime_error("No property v on imgui reference shim object");

        std::array<js::value, 1> arr_val = {val["v"]};

        auto [rval, dirty] = replace_args<1>(arr_val, vctx, id);

        val = rval[0];
        return dirty;
    }

    template<int N>
    bool inout_ref(js::value_context& vctx, std::array<js::value, N>& vals, const std::string& id)
    {
        std::array<js::value, N> res = vals;

        for(int i=0; i < N; i++)
        {
            if(!vals[i].has("v"))
                throw std::runtime_error("No property v on imgui reference shim object, member " + std::to_string(i));

            res[i] = vals[i]["v"];
        }

        auto [rvals, dirty] = replace_args<N>(res, vctx, id);

        vals = rvals;
        return dirty;
    }
}

template<typename... T>
void add_element(js::value_context* vctx, const std::string& type, const std::string& element_id, T&&... args)
{
    js_ui::ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(too_large(*stk))
        return;

    js_ui::ui_element& e = stk->elements.emplace_back();
    e.type = type;
    e.element_id = element_id;
    (e.arguments.push_back(args), ...);
}


void js_ui::text(js::value_context* vctx, std::string str)
{
    if(str.size() > MAX_STR_SIZE)
        return;

    add_element(vctx, "text", str, str);
}

void js_ui::textcolored(js::value_context* vctx, double r, double g, double b, double a, std::string str)
{
    if(str.size() > MAX_STR_SIZE)
        return;

    process::colour(r);
    process::colour(g);
    process::colour(b);
    process::colour(a);

    add_element(vctx, "textcolored", str, r, g, b, a, str);
}

void js_ui::textdisabled(js::value_context* vctx, std::string str)
{
    if(str.size() > MAX_STR_SIZE)
        return;

    add_element(vctx, "textdisabled", str, str);
}

void js_ui::bullettext(js::value_context* vctx, std::string str)
{
    if(str.size() > MAX_STR_SIZE)
        return;

    add_element(vctx, "bullettext", str, str);
}

bool js_ui::smallbutton(js::value_context* vctx, std::string str)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    add_element(vctx, "smallbutton", str, str);
    return isitemclicked(vctx);
}

bool js_ui::invisiblebutton(js::value_context* vctx, std::string str, double w, double h)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);

    process::dimension(w);
    process::dimension(h);

    add_element(vctx, "invisiblebutton", str, str, w, h);
    return isitemclicked(vctx);
}

bool js_ui::arrowbutton(js::value_context* vctx, std::string str, int dir)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);

    dir = clamp(dir, 0, 3);

    add_element(vctx, "arrowbutton", str, str, dir);
    return isitemclicked(vctx);
}

bool js_ui::button(js::value_context* vctx, std::string str, std::optional<double> w, std::optional<double> h)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    if(!w.has_value())
        w = 0;

    if(!h.has_value())
        h = 0;

    process::id(str);

    process::dimension(w.value());
    process::dimension(h.value());

    add_element(vctx, "button", str, str, w.value(), h.value());
    return isitemclicked(vctx);
}

bool js_ui::checkbox(js::value_context* vctx, std::string str, js::value is_checked)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);
    bool dirty = process::inout_ref(*vctx, is_checked, str);

    add_element(vctx, "checkbox", str, str, (int)is_checked);
    return dirty;
}

bool js_ui::radiobutton(js::value_context* vctx, std::string str, int is_active)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);

    add_element(vctx, "radiobutton", str, str, is_active);
    return isitemclicked(vctx);
}

void js_ui::progressbar(js::value_context* vctx, double fraction, std::optional<double> w, std::optional<double> h, std::optional<std::string> overlay)
{
    if(!w.has_value())
        w = 0;

    if(!h.has_value())
        h = 0;

    if(!overlay.has_value())
        overlay = "";

    if(overlay.value().size() > MAX_STR_SIZE)
        return;

    process::dimension(w.value());
    process::dimension(h.value());

    process::fraction(fraction);

    add_element(vctx, "progressbar", "", fraction, w.value(), h.value(), overlay.value());
}

void js_ui::bullet(js::value_context* vctx)
{
    add_element(vctx, "bullet", "");
}

template<typename T, int N, typename... U>
void sliderdragTNimpl(const std::string& type, js::value_context* vctx, std::string str, std::array<js::value, N> v, U&&... vals)
{
    std::array<double, N> to_send;

    for(int i=0; i < N; i++)
    {
        to_send[i] = v[i];

        if(std::is_same_v<T, double>)
            process::float_value(to_send[i]);

        if(std::is_same_v<T, int>)
            process::int_value(to_send[i]);
    }

    static_assert(std::is_same_v<T, int> || std::is_same_v<T, double>);

    js_ui::ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(too_large(*stk))
        return;

    js_ui::ui_element& e = stk->elements.emplace_back();
    e.type = type;
    e.element_id = str;
    e.arguments.push_back(str);

    for(int i=0; i < N; i++)
    {
        e.arguments.push_back(to_send[i]);
    }

    (e.arguments.push_back(vals), ...);
}

template<typename T, int N>
///JS doesn't have ints, so its doubles, and ints are emulated
bool dragTN(const std::string& type, js::value_context* vctx, std::string str, std::array<js::value, N> v, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);
    bool dirty = process::inout_ref<N>(*vctx, v, str);

    if(!v_speed.has_value())
        v_speed = 1;

    if(!v_min.has_value())
        v_min = 0;

    if(!v_max.has_value())
        v_max = 0;

    process::float_value(v_speed.value());

    if(std::is_same_v<T, double>)
    {
        process::float_value(v_min.value());
        process::float_value(v_max.value());
    }

    if(std::is_same_v<T, int>)
    {
        process::int_value(v_min.value());
        process::int_value(v_max.value());
    }

    sliderdragTNimpl<T, N>(type, vctx, str, v, v_speed.value(), v_min.value(), v_max.value());
    return dirty;
}

template<typename T, int N>
bool sliderTN(const std::string& type, js::value_context* vctx, std::string str, std::array<js::value, N> v, double v_min, double v_max)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);
    bool dirty = process::inout_ref<N>(*vctx, v, str);

    if(std::is_same_v<T, double>)
    {
        process::float_value(v_min);
        process::float_value(v_max);
    }

    if(std::is_same_v<T, int>)
    {
        process::int_value(v_min);
        process::int_value(v_max);
    }

    sliderdragTNimpl<T, N>(type, vctx, str, v, v_min, v_max);
    return dirty;
}

template<typename T, int N>
bool inputTN(const std::string& type, js::value_context* vctx, std::string str, std::array<js::value, N> v)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);
    bool dirty = process::inout_ref<N>(*vctx, v, str);

    js_ui::ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(too_large(*stk))
        return false;

    std::array<T, N> to_send;

    for(int i=0; i < N; i++)
    {
        //to_send[i] = (T)v[i];

        if constexpr(std::is_same_v<T, double>)
        {
            double to_san = v[i];
            process::double_value(to_san);

            to_send[i] = to_san;
        }

        if constexpr(std::is_same_v<T, float>)
        {
            double to_san = v[i];
            process::float_value(to_san);

            to_send[i] = to_san;
        }

        if constexpr(std::is_same_v<T, int>)
        {
            double to_san = v[i];
            process::int_value(to_san);

            to_send[i] = to_san;
        }

        if constexpr(std::is_same_v<T, std::string>)
        {
            std::string val = v[i];

            to_send[i] = val;
        }
    }

    js_ui::ui_element& e = stk->elements.emplace_back();
    e.type = type;
    e.element_id = str;
    e.arguments.push_back(str);

    for(int i=0; i < N; i++)
    {
        e.arguments.push_back(to_send[i]);
    }

    return dirty;
}

bool js_ui::dragfloat(js::value_context* vctx, std::string str, js::value v, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<double, 1>("dragfloat", vctx, str, {v}, v_speed, v_min, v_max);
}

bool js_ui::dragfloat2(js::value_context* vctx, std::string str, js::value v1, js::value v2, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<double, 2>("dragfloat2", vctx, str, {v1, v2}, v_speed, v_min, v_max);
}

bool js_ui::dragfloat3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<double, 3>("dragfloat3", vctx, str, {v1, v2, v3}, v_speed, v_min, v_max);
}

bool js_ui::dragfloat4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<double, 4>("dragfloat4", vctx, str, {v1, v2, v3, v4}, v_speed, v_min, v_max);
}

bool js_ui::dragint(js::value_context* vctx, std::string str, js::value v, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<int, 1>("dragint", vctx, str, {v}, v_speed, v_min, v_max);
}

bool js_ui::dragint2(js::value_context* vctx, std::string str, js::value v1, js::value v2, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<int, 2>("dragint2", vctx, str, {v1, v2}, v_speed, v_min, v_max);
}

bool js_ui::dragint3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<int, 3>("dragint3", vctx, str, {v1, v2, v3}, v_speed, v_min, v_max);
}

bool js_ui::dragint4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max)
{
    return dragTN<int, 4>("dragint4", vctx, str, {v1, v2, v3, v4}, v_speed, v_min, v_max);
}

bool js_ui::sliderfloat(js::value_context* vctx, std::string str, js::value v, double v_min, double v_max)
{
    return sliderTN<double, 1>("sliderfloat", vctx, str, {v}, v_min, v_max);
}

bool js_ui::sliderfloat2(js::value_context* vctx, std::string str, js::value v1, js::value v2, double v_min, double v_max)
{
    return sliderTN<double, 2>("sliderfloat2", vctx, str, {v1, v2}, v_min, v_max);
}

bool js_ui::sliderfloat3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, double v_min, double v_max)
{
    return sliderTN<double, 3>("sliderfloat3", vctx, str, {v1, v2, v3}, v_min, v_max);
}

bool js_ui::sliderfloat4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, double v_min, double v_max)
{
    return sliderTN<double, 4>("sliderfloat4", vctx, str, {v1, v2, v3, v4}, v_min, v_max);
}

bool js_ui::sliderangle(js::value_context* vctx, std::string str, js::value v, std::optional<double> v_min, std::optional<double> v_max)
{
    if(!v_min.has_value())
        v_min = -360.0;

    if(!v_max.has_value())
        v_max = 360.0;

    return sliderTN<double, 1>("sliderangle", vctx, str, {v}, v_min.value(), v_max.value());
}

bool js_ui::sliderint(js::value_context* vctx, std::string str, js::value v, double v_min, double v_max)
{
    return sliderTN<int, 1>("sliderint", vctx, str, {v}, v_min, v_max);
}

bool js_ui::sliderint2(js::value_context* vctx, std::string str, js::value v1, js::value v2, double v_min, double v_max)
{
    return sliderTN<int, 2>("sliderint2", vctx, str, {v1, v2}, v_min, v_max);
}

bool js_ui::sliderint3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, double v_min, double v_max)
{
    return sliderTN<int, 3>("sliderint3", vctx, str, {v1, v2, v3}, v_min, v_max);
}

bool js_ui::sliderint4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, double v_min, double v_max)
{
    return sliderTN<int, 4>("sliderint4", vctx, str, {v1, v2, v3, v4}, v_min, v_max);
}

bool js_ui::inputtext(js::value_context* vctx, std::string str, js::value buffer)
{
    return inputTN<std::string, 1>("inputtext", vctx, str, {buffer});
}

bool js_ui::inputtextmultiline(js::value_context* vctx, std::string str, js::value buffer)
{
    return inputTN<std::string, 1>("inputtextmultiline", vctx, str, {buffer});
}

bool js_ui::inputint(js::value_context* vctx, std::string str, js::value v)
{
    return inputTN<int, 1>("inputint", vctx, str, {v});
}

bool js_ui::inputfloat(js::value_context* vctx, std::string str, js::value v)
{
    return inputTN<float, 1>("inputfloat", vctx, str, {v});
}

bool js_ui::inputdouble(js::value_context* vctx, std::string str, js::value v)
{
    return inputTN<double, 1>("inputdouble", vctx, str, {v});
}

bool js_ui::inputint2(js::value_context* vctx, std::string str, js::value v1, js::value v2)
{
    return inputTN<int, 2>("inputint2", vctx, str, {v1, v2});
}

bool js_ui::inputfloat2(js::value_context* vctx, std::string str, js::value v1, js::value v2)
{
    return inputTN<float, 2>("inputfloat2", vctx, str, {v1, v2});
}

bool js_ui::inputint3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3)
{
    return inputTN<int, 3>("inputint3", vctx, str, {v1, v2, v3});
}

bool js_ui::inputfloat3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3)
{
    return inputTN<float, 3>("inputfloat3", vctx, str, {v1, v2, v3});
}

bool js_ui::inputint4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4)
{
    return inputTN<int, 4>("inputint4", vctx, str, {v1, v2, v3, v4});
}

bool js_ui::inputfloat4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4)
{
    return inputTN<float, 4>("inputfloat4", vctx, str, {v1, v2, v3, v4});
}

template<typename T, int N, typename... U>
bool colorTN(const std::string& type, js::value_context* vctx, std::string str, std::array<js::value, N> v, U&&... u)
{
    if(str.size() > MAX_STR_SIZE)
        return false;

    process::id(str);
    bool dirty = process::inout_ref<N>(*vctx, v, str);

    js_ui::ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(too_large(*stk))
        return false;

    std::array<T, N> to_send;

    for(int i=0; i < N; i++)
    {
        double to_san = v[i];
        process::double_value(to_san);

        to_send[i] = to_san;
    }

    js_ui::ui_element& e = stk->elements.emplace_back();
    e.type = type;
    e.element_id = str;
    e.arguments.push_back(str);

    for(int i=0; i < N; i++)
    {
        e.arguments.push_back(to_send[i]);
    }

    (e.arguments.push_back(u), ...);

    return dirty;
}

bool js_ui::coloredit3(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b)
{
    return colorTN<double, 3>("coloredit3", vctx, str, {r, g, b});
}

bool js_ui::coloredit4(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b, js::value a)
{
    return colorTN<double, 4>("coloredit4", vctx, str, {r, g, b, a});
}

bool js_ui::colorpicker3(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b)
{
    return colorTN<double, 3>("colorpicker3", vctx, str, {r, g, b});
}

bool js_ui::colorpicker4(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b, js::value a)
{
    return colorTN<double, 4>("colorpicker4", vctx, str, {r, g, b, a});
}

bool js_ui::colorbutton(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b, js::value a, std::optional<double> unused, std::optional<double> w, std::optional<double> h)
{
    if(!w.has_value())
        w = 0;

    if(!h.has_value())
        h = 0;

    unused = 0;

    return colorTN<double, 4>("colorbutton", vctx, str, {r,g, b, a}, unused.value(), w.value(), h.value());
}

bool js_ui::treenode(js::value_context* vctx, std::string str)
{
    process::id(str);

    add_element(vctx, "treenode", str, str);

    return last_element_has_state(vctx, "returnstrue");
}

void js_ui::treepush(js::value_context* vctx, std::string str)
{
    process::id(str);

    add_element(vctx, "treepush", str, str);
}

void js_ui::treeend(js::value_context* vctx)
{
    add_element(vctx, "treeend", "");
}

bool js_ui::collapsingheader(js::value_context* vctx, std::string str)
{
    process::id(str);

    add_element(vctx, "collapsingheader", str, str);

    return last_element_has_state(vctx, "returnstrue");
}

void js_ui::setnextitemopen(js::value_context* vctx, bool is_open)
{
    add_element(vctx, "setnextitemopen", "", is_open);
}

bool js_ui::selectable(js::value_context* vctx, std::string str, js::value overloaded_ref_or_bool, double unused, std::optional<double> w, std::optional<double> h)
{
    process::id(str);

    if(!w.has_value())
        w = 0;

    if(!h.has_value())
        h = 0;

    process::dimension(w.value());
    process::dimension(h.value());

    unused = 0;

    bool is_dirty = false;

    ///if its a 'reference' type
    if(overloaded_ref_or_bool.has("v"))
    {
        is_dirty = process::inout_ref(*vctx, overloaded_ref_or_bool, str);
    }
    else
    {
        js::value dummy_val(*vctx);
        dummy_val["v"] = 0;

        is_dirty = process::inout_ref(*vctx, dummy_val, str);
    }

    add_element(vctx, "selectable", str, str, (int)overloaded_ref_or_bool, (int)unused, w.value(), h.value());

    return is_dirty;
}

bool js_ui::listbox(js::value_context* vctx, std::string str, js::value current_value, std::vector<std::string> names, std::optional<double> height_in_items)
{
    process::id(str);

    if(!height_in_items.has_value())
        height_in_items = -1;

    height_in_items.value() = clamp(san_val(height_in_items.value()), -1, 9999999);

    bool dirty = process::inout_ref(*vctx, current_value, str);

    add_element(vctx, "listbox", str, str, (int)current_value, names, (int)height_in_items.value());

    return dirty;
}

void js_ui::plotlines(js::value_context* vctx, std::string str, std::vector<double> values, std::optional<int> value_offset,
                   std::optional<std::string> overlay_string,
                   std::optional<double> scale_min, std::optional<double> scale_max,
                   std::optional<double> graph_w, std::optional<double> graph_h)
{
    process::id(str);

    if(!value_offset.has_value())
        value_offset = 0;

    value_offset = clamp(value_offset.value(), 0, (int)values.size());

    if(!overlay_string.has_value())
        overlay_string = "";

    if(overlay_string.value().size() > MAX_STR_SIZE)
        return;

    if(!scale_min.has_value())
        scale_min = FLT_MAX;

    if(!scale_max.has_value())
        scale_max = FLT_MAX;

    scale_min.value() = san_val(scale_min.value());
    scale_max.value() = san_val(scale_max.value());

    if(!graph_w.has_value())
        graph_w = 0;

    if(!graph_h.has_value())
        graph_h = 0;

    process::dimension(graph_w.value());
    process::dimension(graph_h.value());

    add_element(vctx, "plotlines", str, str, values, value_offset.value(), overlay_string.value(), scale_min.value(), scale_max.value(), graph_w.value(), graph_h.value());
}

void js_ui::plothistogram(js::value_context* vctx, std::string str, std::vector<double> values, std::optional<int> value_offset,
                   std::optional<std::string> overlay_string,
                   std::optional<double> scale_min, std::optional<double> scale_max,
                   std::optional<double> graph_w, std::optional<double> graph_h)
{
    process::id(str);

    if(!value_offset.has_value())
        value_offset = 0;

    value_offset = clamp(value_offset.value(), 0, (int)values.size());

    if(!overlay_string.has_value())
        overlay_string = "";

    if(overlay_string.value().size() > MAX_STR_SIZE)
        return;

    if(!scale_min.has_value())
        scale_min = FLT_MAX;

    if(!scale_max.has_value())
        scale_max = FLT_MAX;

    scale_min.value() = san_val(scale_min.value());
    scale_max.value() = san_val(scale_max.value());

    if(!graph_w.has_value())
        graph_w = 0;

    if(!graph_h.has_value())
        graph_h = 0;

    process::dimension(graph_w.value());
    process::dimension(graph_h.value());

    add_element(vctx, "plothistogram", str, str, values, value_offset.value(), overlay_string.value(), scale_min.value(), scale_max.value(), graph_w.value(), graph_h.value());
}

void js_ui::pushstylecolor(js::value_context* vctx, int idx, double r, double g, double b, double a)
{
    if(idx < 0)
        return;

    process::colour(r);
    process::colour(g);
    process::colour(b);
    process::colour(a);

    ///IDX IS NOT SANITISED ON THE SERVER
    add_element(vctx, "pushstylecolor", "", idx, r, g, b, a);
}

void js_ui::popstylecolor(js::value_context* vctx, std::optional<int> cnt)
{
    if(!cnt.has_value())
        cnt = 1;

    if(cnt.value() < 0)
        return;

    add_element(vctx, "popstylecolor", "", cnt.value());
}

void js_ui::pushitemwidth(js::value_context* vctx, double item_width)
{
    ///sure
    process::dimension(item_width);

    add_element(vctx, "pushitemwidth", "", item_width);
}

void js_ui::popitemwidth(js::value_context* vctx)
{
    add_element(vctx, "popitemwidth", "");
}

void js_ui::setnextitemwidth(js::value_context* vctx, double item_width)
{
    process::dimension(item_width);

    add_element(vctx, "setnextitemwidth", "", item_width);
}

void js_ui::separator(js::value_context* vctx)
{
    add_element(vctx, "separator", "");
}

void js_ui::sameline(js::value_context* vctx, std::optional<double> offset_from_start, std::optional<double> spacing)
{
    if(!offset_from_start.has_value())
        offset_from_start = 0;

    if(!spacing.has_value())
        spacing = -1;

    process::dimension(offset_from_start.value());
    process::dimension(spacing.value());

    add_element(vctx, "sameline", "", offset_from_start.value(), spacing.value());
}

void js_ui::newline(js::value_context* vctx)
{
    add_element(vctx, "newline", "");
}

void js_ui::spacing(js::value_context* vctx)
{
    add_element(vctx, "spacing", "");
}

void js_ui::dummy(js::value_context* vctx, double w, double h)
{
    process::positive_dimension(w);
    process::positive_dimension(h);

    add_element(vctx, "dummy", "", w, h);
}

void js_ui::indent(js::value_context* vctx, std::optional<double> indent_w)
{
    if(!indent_w.has_value())
        indent_w = 0;

    process::dimension(indent_w.value());

    add_element(vctx, "indent", "", indent_w.value());
}

void js_ui::unindent(js::value_context* vctx, std::optional<double> indent_w)
{
    if(!indent_w.has_value())
        indent_w = 0;

    process::dimension(indent_w.value());

    add_element(vctx, "unindent", "", indent_w.value());
}

///This function accepts an id, but it simply stashes it so it can be sent with endgroup
///this is confusing because the network layer does not match the api for begingroup, and endgroup
///but is necessary to achieve imgui.begingroup("hellothere") imgui.endgroup(), vs imgui.begingroup() imgui.endgroup("hellothere")
///which is out of keeping with the rest of the imgui api. Document this in the networking format with big letters
void js_ui::begingroup(js::value_context* vctx, std::string id_str)
{
    process::id(id_str);

    js_ui::ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(too_large(*stk))
        return;

    add_element(vctx, "begingroup", "");

    stk->group_id_stack.push_back(id_str);
}

void js_ui::endgroup(js::value_context* vctx)
{
    js_ui::ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(too_large(*stk))
        return;

    std::string my_id;

    if(stk->group_id_stack.size() > 0)
    {
        my_id = stk->group_id_stack.back();
        stk->group_id_stack.pop_back();
    }

    add_element(vctx, "endgroup", my_id, my_id);
}

std::optional<std::pair<ui_element_state*, std::unique_lock<lock_type_t>>> get_last_element(js::value_context& vctx)
{
    js_ui::ui_stack* stk = js::get_heap_stash(vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(stk->elements.size() == 0)
        return std::nullopt;

    command_handler_state* found_ptr = js::get_heap_stash(vctx)["command_handler_state_pointer"].get_ptr<command_handler_state>();

    if(found_ptr == nullptr)
        return std::nullopt;

    if(!js::get_heap_stash(vctx).has("realtime_id"))
        return std::nullopt;

    int realtime_id = js::get_heap_stash(vctx)["realtime_id"];

    std::unique_lock<lock_type_t> guard(found_ptr->script_data_lock);

    auto realtime_it = found_ptr->script_data.find(realtime_id);

    if(realtime_it == found_ptr->script_data.end())
        return std::nullopt;

    realtime_script_data& dat = realtime_it->second;

    std::string last_id = stk->elements.back().element_id;

    if(dat.realtime_ui.element_states.find(last_id) == dat.realtime_ui.element_states.end())
        return std::nullopt;

    ui_element_state& st = dat.realtime_ui.element_states[last_id];

    return std::pair<ui_element_state*, std::unique_lock<lock_type_t>>{&st, std::move(guard)};
}

bool is_any_of(const std::vector<std::string>& data, const std::string& val)
{
    for(const auto& i : data)
    {
        if(i == val)
            return true;
    }

    return false;
}

bool js_ui::last_element_has_state(js::value_context* vctx, const std::string& state)
{
    auto last_element_opt = get_last_element(*vctx);

    if(!last_element_opt.has_value())
        return false;

    return is_any_of(last_element_opt.value().first->value, state);
}

bool js_ui::isitemhovered(js::value_context* vctx)
{
    return last_element_has_state(vctx, "hovered");
}

bool js_ui::isitemactive(js::value_context* vctx)
{
    return last_element_has_state(vctx, "active");
}

bool js_ui::isitemfocused(js::value_context* vctx)
{
    return last_element_has_state(vctx, "focused");
}

bool js_ui::isitemclicked(js::value_context* vctx)
{
    return last_element_has_state(vctx, "clicked");
}

bool js_ui::isitemvisible(js::value_context* vctx)
{
    return last_element_has_state(vctx, "visible");
}

bool js_ui::isitemedited(js::value_context* vctx)
{
    return last_element_has_state(vctx, "edited");
}

bool js_ui::isitemactivated(js::value_context* vctx)
{
    return last_element_has_state(vctx, "activated");
}

bool js_ui::isitemdeactivated(js::value_context* vctx)
{
    return last_element_has_state(vctx, "deactivated");
}

bool js_ui::isitemdeactivatedafteredit(js::value_context* vctx)
{
    return last_element_has_state(vctx, "deactivatedafteredit");
}

bool js_ui::isitemtoggledopen(js::value_context* vctx)
{
    return last_element_has_state(vctx, "toggledopen");
}

bool any_element_has_state(js::value_context* vctx, const std::string& state)
{
    js_ui::ui_stack* stk = js::get_heap_stash(vctx)["ui_stack"].get_ptr<js_ui::ui_stack>();

    if(stk->elements.size() == 0)
        return std::nullopt;

    command_handler_state* found_ptr = js::get_heap_stash(vctx)["command_handler_state_pointer"].get_ptr<command_handler_state>();

    if(found_ptr == nullptr)
        return std::nullopt;

    if(!js::get_heap_stash(vctx).has("realtime_id"))
        return std::nullopt;

    int realtime_id = js::get_heap_stash(vctx)["realtime_id"];

    std::unique_lock<lock_type_t> guard(found_ptr->script_data_lock);

    auto realtime_it = found_ptr->script_data.find(realtime_id);

    if(realtime_it == found_ptr->script_data.end())
        return std::nullopt;

    realtime_script_data& dat = realtime_it->second;

    for(auto& i : dat.element_states)
    {
        ui_element_state& st = i.second;

        if(is_any_of(st.value, state))
            return true;
    }

    return false;
}

bool js_ui::isanyitemhovered(js::value_context* vctx)
{
    return any_element_has_state(vctx, "hovered");
}

bool js_ui::isanyitemactive(js::value_context* vctx)
{
    return any_element_has_state(vctx, "active");
}

bool js_ui::isanyitemfocused(js::value_context* vctx)
{
    return any_element_has_state(vctx, "focused");
}

js::value js_ui::ref(js::value_context* vctx, js::value val)
{
    js::value ret(*vctx);
    ret["v"] = val;

    return ret;
}

js::value js_ui::get(js::value_context* vctx, js::value val)
{
    if(!val.has("v"))
        return js::value(*vctx, js::undefined);

    return val["v"];
}

std::optional<js_ui::ui_stack> js_ui::consume(js::value_context& vctx)
{
    ui_stack* stk = js::get_heap_stash(vctx)["ui_stack"].get_ptr<ui_stack>();

    stk->group_id_stack.clear();

    if(stk->elements.size() == 0)
    {
        if((int)js::get_heap_stash(vctx)["blank_ui_is_significant"] > 0)
        {
            js::get_heap_stash(vctx)["blank_ui_is_significant"] = 0;

            return *stk;
        }
        else
        {
            return std::nullopt;
        }
    }

    if(too_large(*stk))
        return ui_stack();

    ui_stack ret;

    ret.elements = std::move(stk->elements);
    *stk = ui_stack();

    js::get_heap_stash(vctx)["blank_ui_is_significant"] = 1;

    return ret;
}
