#include "stacktrace.hpp"

#ifndef NO_STACKTRACE
#include <signal.h>     // ::signal, ::raise
#include <boost/stacktrace.hpp>
#include <iostream>
#include <sstream>

#ifdef __WIN32__
#include <fstream>
#include <filesystem>
#endif // __WIN32__

void signal_handler(int signum)
{
    ::signal(signum, SIG_DFL);
    //boost::stacktrace::safe_dump_to("./backtrace.dump");

    std::string stacktrace = get_stacktrace();

    printf("stacktrace %s\n", stacktrace.c_str());

    FILE* pFile = fopen("crash.txt", "a+");

    fwrite(stacktrace.c_str(), 1, stacktrace.size(), pFile);

    fclose(pFile);

    system("pause");

    ::raise(SIGABRT);
}

void stack_on_start()
{
    //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    #ifdef NO_STACKTRACE
    return;
    #endif // NO_STACKTRACE

    ::signal(SIGSEGV, &signal_handler);
    ::signal(SIGABRT, &signal_handler);

    #ifdef __WIN32__
    if (std::filesystem::exists("./backtrace.dump"))
    {
        // there is a backtrace
        std::ifstream ifs("./backtrace.dump");

        boost::stacktrace::stacktrace st = boost::stacktrace::stacktrace::from_dump(ifs);
        std::cout << "Previous run crashed:\n" << st << std::endl;

        // cleaning up
        ifs.close();
        if(std::filesystem::exists("./backtrace_1.dump"))
            std::filesystem::remove("./backtrace_1.dump");

        rename("./backtrace.dump", "./backtrace_1.dump");
    }
    #endif // __WIN32__
}

std::string get_stacktrace()
{
    std::stringstream stream;

    stream << boost::stacktrace::stacktrace();

    return stream.str();
}

std::string name_from_ptr(void* ptr)
{
    return boost::stacktrace::frame((boost::stacktrace::detail::native_frame_ptr_t)ptr).name();
}
#else
void stack_on_start()
{

}

void signal_handler(int signum)
{

}

std::string get_stacktrace()
{
    return "";
}

std::string name_from_ptr(void* ptr)
{
    return "";
}

#endif
