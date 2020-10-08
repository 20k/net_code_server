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
    void button(js::value_context* vctx, std::string str);
    void bullet(js::value_context* vctx);

    void pushstylecolor(js::value_context* vctx, int idx, double r, double g, double b, double a);
    void popstylecolor(js::value_context* vctx, int cnt);

    void sameline(js::value_context* vctx);
    void newline(js::value_context* vctx);
    void spacing(js::value_context* vctx);
    void begingroup(js::value_context* vctx);
    void endgroup(js::value_context* vctx);
    bool isitemclicked(js::value_context* vctx);
    bool isitemhovered(js::value_context* vctx);

    std::optional<ui_stack> consume(js::value_context& vctx);
}

#endif // JS_UI_HPP_INCLUDED
