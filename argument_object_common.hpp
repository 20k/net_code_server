#ifndef ARGUMENT_OBJECT_COMMON_HPP_INCLUDED
#define ARGUMENT_OBJECT_COMMON_HPP_INCLUDED

#include "duktape.h"

using js_funcptr_t = duk_ret_t(*)(duk_context*);

namespace js
{
    struct undefined_t;
}


#endif // ARGUMENT_OBJECT_COMMON_HPP_INCLUDED
