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
    };

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
    void begingroup(js::value_context* vctx);
    void endgroup(js::value_context* vctx);
    bool isitemclicked(js::value_context* vctx);
    bool isitemhovered(js::value_context* vctx);

    js::value ref(js::value_context* vctx, js::value val);
    js::value get(js::value_context* vctx, js::value val);

    std::optional<ui_stack> consume(js::value_context& vctx);
}

#endif // JS_UI_HPP_INCLUDED
