#ifndef JS_UI_HPP_INCLUDED
#define JS_UI_HPP_INCLUDED

#include "argument_object.hpp"
#include <optional>

namespace js_ui
{
    struct ui_element
    {
        std::string type;
        std::string element_id;
        std::vector<nlohmann::json> arguments;
    };

    struct ui_stack
    {
        std::vector<ui_element> elements;
    };

    void text(js::value_context* vctx, std::string str);
    void textcolored(js::value_context* vctx, double r, double g, double b, double a, std::string str);
    void textdisabled(js::value_context* vctx, std::string str);
    void bullettext(js::value_context* vctx, std::string str);
    void smallbutton(js::value_context* vctx, std::string str);
    void invisiblebutton(js::value_context* vctx, std::string str, double w, double h);
    void arrowbutton(js::value_context* vctx, std::string str, int dir);
    void button(js::value_context* vctx, std::string str, std::optional<double> w, std::optional<double> h);
    void bullet(js::value_context* vctx);

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

    std::optional<ui_stack> consume(js::value_context& vctx);
}

#endif // JS_UI_HPP_INCLUDED
