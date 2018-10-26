#include "stacktrace.hpp"

#include <signal.h>     // ::signal, ::raise
#include <boost/stacktrace.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <sstream>
#include <windows.h>

void signal_handler(int signum)
{
    ::signal(signum, SIG_DFL);
    //boost::stacktrace::safe_dump_to("./backtrace.dump");

    std::string stacktrace = get_stacktrace();

    printf("stacktrace %s\n", stacktrace.c_str());

    FILE* pFile = fopen("crash.txt", "w");

    fwrite(stacktrace.c_str(), 1, stacktrace.size(), pFile);

    fclose(pFile);

    system("pause");

    ::raise(SIGABRT);
}

void stack_on_start()
{
    //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

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

std::string get_stacktrace()
{
    std::stringstream stream;

    stream << boost::stacktrace::stacktrace();

    return stream.str();
}
