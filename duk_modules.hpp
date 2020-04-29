#ifndef DUK_MODULES_HPP_INCLUDED
#define DUK_MODULES_HPP_INCLUDED

#include <string>
#include <map>
#include "argument_object.hpp"

void dukx_inject_modules(js::value_context& vctx);
void init_module_cache();

std::map<std::string, std::string>& module_binary_cache();
std::map<std::string, std::string>& module_cache();

#endif // DUK_MODULES_HPP_INCLUDED
