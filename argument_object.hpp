#ifndef ARGUMENT_OBJECT_HPP_INCLUDED
#define ARGUMENT_OBJECT_HPP_INCLUDED

#include "argument_object_qjs.hpp"
#include "argument_object_duk.hpp"

///this stuff is features, not implementation dependent
/*namespace js
{
    using namespace js_duk;


}*/

/*namespace js
{
    using namespace js_duk;
}*/

//namespace js = js_quickjs;
namespace js = js_duk;

#define USE_DUKTAPE
//#define USE_QUICKJS

#endif // ARGUMENT_OBJECT_HPP_INCLUDED
