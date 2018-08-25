#include "stacktrace.hpp"

#include <signal.h>     // ::signal, ::raise
#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <sstream>

void stack_on_start()
{
    ::signal(SIGSEGV, &signal_handler);
    ::signal(SIGABRT, &signal_handler);


    if (boost::filesystem::exists("./backtrace.dump"))
    {
        // there is a backtrace
        std::ifstream ifs("./backtrace.dump");

        boost::stacktrace::stacktrace st = boost::stacktrace::stacktrace::from_dump(ifs);
        std::cout << "Previous run crashed:\n" << st << std::endl;

        // cleaning up
        ifs.close();
        if(boost::filesystem::exists("./backtrace_1.dump"))
            boost::filesystem::remove("./backtrace_1.dump");

        rename("./backtrace.dump", "./backtrace_1.dump");
    }
}

void signal_handler(int signum)
{
    ::signal(signum, SIG_DFL);
    boost::stacktrace::safe_dump_to("./backtrace.dump");
    ::raise(SIGABRT);
}

std::string get_stacktrace()
{
    std::stringstream stream;

    stream << boost::stacktrace::stacktrace();

    return stream.str();
}
