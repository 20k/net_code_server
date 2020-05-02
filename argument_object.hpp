#ifndef ARGUMENT_OBJECT_HPP_INCLUDED
#define ARGUMENT_OBJECT_HPP_INCLUDED

#define USE_QUICKJS
//#define USE_DUKTAPE

#ifdef USE_QUICKJS
#include "argument_object_qjs.hpp"
namespace js = js_quickjs;
#endif // USE_QUICKJS

#ifdef USE_DUKTAPE
#include "argument_object_duk.hpp"
namespace js = js_duk;
#endif // USE_DUKTAPE

#endif // ARGUMENT_OBJECT_HPP_INCLUDED
