#ifndef JS_UI_HPP_INCLUDED
#define JS_UI_HPP_INCLUDED

#include "argument_object.hpp"
#include <optional>

namespace js_ui
{
    struct ui_element
    {
        std::string type;
        std::string value;
    };

    struct ui_stack
    {
        std::vector<ui_element> elements;
    };

    void text(js::value_context* vctx, std::string str);
    void button(js::value_context* vctx, std::string str);
    void sameline(js::value_context* vctx);
    std::optional<ui_stack> consume(js::value_context& vctx);
}

#endif // JS_UI_HPP_INCLUDED
