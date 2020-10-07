#include "js_ui.hpp"
#include "command_handler_state.hpp"

bool too_large(js_ui::ui_stack& stk)
{
    uint64_t sum = 0;

    for(const js_ui::ui_element& e : stk.elements)
    {
        sum += e.type.size();
        sum += e.value.size();
    }

    if(sum >= 1024 * 1024 * 10)
        return true;

    return false;
}

void js_ui::text(js::value_context* vctx, std::string str)
{
    ui_element e;
    e.type = "text";
    e.value = std::move(str);

    ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<ui_stack>();

    if(too_large(*stk))
        return;

    stk->elements.push_back(e);
}

void js_ui::button(js::value_context* vctx, std::string str)
{
    ui_element e;
    e.type = "button";
    e.value = std::move(str);

    ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<ui_stack>();

    if(too_large(*stk))
        return;

    stk->elements.push_back(e);
}

void js_ui::sameline(js::value_context* vctx)
{
    ui_element e;
    e.type = "sameline";

    ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<ui_stack>();

    if(too_large(*stk))
        return;

    stk->elements.push_back(e);
}

bool js_ui::isitemclicked(js::value_context* vctx)
{
    ui_stack* stk = js::get_heap_stash(*vctx)["ui_stack"].get_ptr<ui_stack>();

    if(stk->elements.size() == 0)
        return false;

    command_handler_state* found_ptr = js::get_heap_stash(*vctx)["command_handler_state_pointer"].get_ptr<command_handler_state>();

    if(found_ptr == nullptr)
        return false;

    if(!js::get_heap_stash(*vctx).has("realtime_id"))
        return false;

    int realtime_id = js::get_heap_stash(*vctx)["realtime_id"];

    std::lock_guard guard(found_ptr->script_data_lock);

    auto realtime_it = found_ptr->script_data.find(realtime_id);

    if(realtime_it == found_ptr->script_data.end())
        return false;

    realtime_script_data& dat = realtime_it->second;

    std::string last_id = stk->elements.back().value;

    if(dat.realtime_ui.element_states.find(last_id) == dat.realtime_ui.element_states.end())
        return false;

    ui_element_state& st = dat.realtime_ui.element_states[last_id];

    return st.value == "clicked";
}

std::optional<js_ui::ui_stack> js_ui::consume(js::value_context& vctx)
{
    ui_stack* stk = js::get_heap_stash(vctx)["ui_stack"].get_ptr<ui_stack>();

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

    ui_stack ret = *stk;

    stk->elements.clear();

    js::get_heap_stash(vctx)["blank_ui_is_significant"] = 1;

    return ret;
}
