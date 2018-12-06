#ifndef DUK_MODULES_HPP_INCLUDED
#define DUK_MODULES_HPP_INCLUDED

#include "scripting_api_fwrd.hpp"
#include <map>

void dukx_inject_modules(duk_context* ctx);
void init_module_cache();

std::map<std::string, std::string>& module_binary_cache();
std::map<std::string, std::string>& module_cache();

#endif // DUK_MODULES_HPP_INCLUDED
