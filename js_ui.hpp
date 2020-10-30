#ifndef JS_UI_HPP_INCLUDED
#define JS_UI_HPP_INCLUDED

#include "argument_object.hpp"
#include <optional>
#include <variant>

namespace js_ui
{
    struct ui_element
    {
        std::string type;
        std::string element_id;
        std::vector<nlohmann::json> arguments;

        std::vector<js::value> argument_state;
    };

    struct ui_stack
    {
        std::vector<ui_element> elements;
        uint64_t current_size = 0;
        uint64_t current_idx = 0;

        uint64_t server_sequence_id = 0;
        uint64_t last_client_sequence_id = 0;

        std::vector<std::string> group_id_stack;
        std::vector<std::string> source_drag_drop_id_stack;
        std::vector<std::string> target_drag_drop_id_stack;
    };

    bool is_edge_event(const std::string& str);

    void text(js::value_context* vctx, std::string str);
    void textcolored(js::value_context* vctx, double r, double g, double b, double a, std::string str);
    void textdisabled(js::value_context* vctx, std::string str);
    void bullettext(js::value_context* vctx, std::string str);
    bool smallbutton(js::value_context* vctx, std::string str);
    bool invisiblebutton(js::value_context* vctx, std::string str, double w, double h);
    bool arrowbutton(js::value_context* vctx, std::string str, int dir);
    bool button(js::value_context* vctx, std::string str, std::optional<double> w, std::optional<double> h);
    bool checkbox(js::value_context* vctx, std::string str, js::value is_checked);
    bool radiobutton(js::value_context* vctx, std::string str, int is_active);
    void progressbar(js::value_context* vctx, double fraction, std::optional<double> w, std::optional<double> h, std::optional<std::string> overlay);
    void bullet(js::value_context* vctx);

    bool dragfloat(js::value_context* vctx, std::string str, js::value v, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);
    bool dragfloat2(js::value_context* vctx, std::string str, js::value v1, js::value v2, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);
    bool dragfloat3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);
    bool dragfloat4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);
    bool dragint(js::value_context* vctx, std::string str, js::value v, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);
    bool dragint2(js::value_context* vctx, std::string str, js::value v1, js::value v2, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);
    bool dragint3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);
    bool dragint4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, std::optional<double> v_speed, std::optional<double> v_min, std::optional<double> v_max);

    bool sliderfloat(js::value_context* vctx, std::string str, js::value v, double v_min, double v_max);
    bool sliderfloat2(js::value_context* vctx, std::string str, js::value v1, js::value v2, double v_min, double v_max);
    bool sliderfloat3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, double v_min, double v_max);
    bool sliderfloat4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, double v_min, double v_max);

    bool sliderangle(js::value_context* vctx, std::string str, js::value v, std::optional<double> v_min, std::optional<double> v_max);
    bool sliderint(js::value_context* vctx, std::string str, js::value v, double v_min, double v_max);
    bool sliderint2(js::value_context* vctx, std::string str, js::value v1, js::value v2, double v_min, double v_max);
    bool sliderint3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, double v_min, double v_max);
    bool sliderint4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4, double v_min, double v_max);

    bool inputtext(js::value_context* vctx, std::string str, js::value buffer);
    bool inputtextmultiline(js::value_context* vctx, std::string str, js::value buffer);
    bool inputint(js::value_context* vctx, std::string str, js::value v);
    bool inputfloat(js::value_context* vctx, std::string str, js::value v);
    bool inputdouble(js::value_context* vctx, std::string str, js::value v);
    bool inputint2(js::value_context* vctx, std::string str, js::value v1, js::value v2);
    bool inputfloat2(js::value_context* vctx, std::string str, js::value v1, js::value v2);
    bool inputint3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3);
    bool inputfloat3(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3);
    bool inputint4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4);
    bool inputfloat4(js::value_context* vctx, std::string str, js::value v1, js::value v2, js::value v3, js::value v4);

    bool coloredit3(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b);
    bool coloredit4(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b, js::value a);
    bool colorpicker3(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b);
    bool colorpicker4(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b, js::value a);
    bool colorbutton(js::value_context* vctx, std::string str, js::value r, js::value g, js::value b, js::value a, std::optional<double> unused, std::optional<double> w, std::optional<double> h);

    bool treenode(js::value_context* vctx, std::string str);
    void treepush(js::value_context* vctx, std::string str);
    void treepop(js::value_context* vctx);
    bool collapsingheader(js::value_context* vctx, std::string str);
    void setnextitemopen(js::value_context* vctx, bool is_open);

    bool selectable(js::value_context* vctx, std::string str, js::value overloaded_ref_or_bool, double unused, std::optional<double> w, std::optional<double> h);

    bool listbox(js::value_context* vctx, std::string str, js::value current_value, std::vector<std::string> names, std::optional<double> height_in_items);

    void plotlines(js::value_context* vctx, std::string str, std::vector<double> values, std::optional<int> value_offset,
                   std::optional<std::string> overlay_string,
                   std::optional<double> scale_min, std::optional<double> scale_max,
                   std::optional<double> graph_w, std::optional<double> graph_h);

    void plothistogram(js::value_context* vctx, std::string str, std::vector<double> values, std::optional<int> value_offset,
                   std::optional<std::string> overlay_string,
                   std::optional<double> scale_min, std::optional<double> scale_max,
                   std::optional<double> graph_w, std::optional<double> graph_h);

    void pushstylecolor(js::value_context* vctx, int idx, double r, double g, double b, double a);
    void popstylecolor(js::value_context* vctx, std::optional<int> cnt);

    void pushitemwidth(js::value_context* vctx, double item_width);
    void popitemwidth(js::value_context* vctx);
    void setnextitemwidth(js::value_context* vctx, double item_width);

    void separator(js::value_context* vctx);
    void sameline(js::value_context* vctx, std::optional<double> offset_from_start, std::optional<double> spacing);
    void newline(js::value_context* vctx);
    void spacing(js::value_context* vctx);
    void dummy(js::value_context* vctx, double w, double h);
    void indent(js::value_context* vctx, std::optional<double> indent_w);
    void unindent(js::value_context* vctx, std::optional<double> indent_w);
    void begingroup(js::value_context* vctx, std::string id_str);
    void endgroup(js::value_context* vctx);
    bool isitemhovered(js::value_context* vctx);
    bool isitemactive(js::value_context* vctx);
    bool isitemfocused(js::value_context* vctx);
    bool isitemclicked(js::value_context* vctx);
    bool isitemvisible(js::value_context* vctx);
    bool isitemedited(js::value_context* vctx);
    bool isitemactivated(js::value_context* vctx);
    bool isitemdeactivated(js::value_context* vctx);
    bool isitemdeactivatedafteredit(js::value_context* vctx);
    bool isitemtoggledopen(js::value_context* vctx);
    bool isanyitemhovered(js::value_context* vctx);
    bool isanyitemactive(js::value_context* vctx);
    bool isanyitemfocused(js::value_context* vctx);

    bool begindragdropsource(js::value_context* vctx);
    bool setdragdroppayload(js::value_context* vctx, std::string type, js::value buffer);
    void enddragdropsource(js::value_context* vctx);

    bool begindragdroptarget(js::value_context* vctx);
    js::value acceptdragdroppayload(js::value_context* vctx, std::string type);
    void enddragdroptarget(js::value_context* vctx);
    //js::value getdragdroppayload(js::value_context* vctx);

    js::value ref(js::value_context* vctx, js::value val);
    js::value get(js::value_context* vctx, js::value val);

    std::optional<ui_stack> consume(js::value_context& vctx);

    bool last_element_has_state(js::value_context* vctx, const std::string& state);
}

#endif // JS_UI_HPP_INCLUDED
