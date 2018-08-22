#ifndef STACKTRACE_HPP_INCLUDED
#define STACKTRACE_HPP_INCLUDED

#include <string>

void stack_on_start();

void signal_handler(int signum);

std::string get_stacktrace();

#endif // STACKTRACE_HPP_INCLUDED
