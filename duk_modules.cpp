#include "duk_modules.hpp"
#include "scripting_api.hpp"
#include "duk_module_duktape.h"

void dukx_inject_modules(duk_context* ctx)
{
#ifdef TESTING
    duk_module_duktape_init(ctx);

    duk_eval_string(ctx,
R"D(
Duktape.modSearch = function (id, require, exports, module) {
    if (id === 'foo')
    {
        console.log("hello");
        return 'exports.hello = function () { print("Hello world from bar!"); };';
    }

    throw new Error('cannot find module: ' + id);
}
  )D");


#endif // TESTING
}
