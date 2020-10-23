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
    bool sliderfloat(js::value_context* vctx, std::string str, js::value v, double v_min, double v_max);

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
