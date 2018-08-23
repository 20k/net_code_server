#ifndef PERFMON_HPP_INCLUDED
#define PERFMON_HPP_INCLUDED

#include <SFML/System.hpp>
#include <iostream>
#include <vector>

struct perfmon
{
    sf::Clock clk;

    int line;
    std::string file;
    std::string func;
    bool has_detailed = false;

    bool enabled = true;
    int locks = 0;
    int db_hits = 0;
    std::vector<std::string> lock_stacktraces;

    perfmon(int pline, const std::string& pfile, const std::string& pfunc)
    {
        line = pline;
        file = pfile;
        func = pfunc;

        has_detailed = true;
    }

    perfmon()
    {

    }

    ~perfmon()
    {
        if(enabled)
        {
            if(has_detailed)
                std::cout << clk.getElapsedTime().asMicroseconds() / 1000. << "ms @" << line << " f " << file << " " << func;
            else
                std::cout << clk.getElapsedTime().asMicroseconds() / 1000.;

            std::cout << " locks " << locks << " db hits " << db_hits << std::endl;

            if(lock_stacktraces.size() > 0)
            {
                for(auto& i : lock_stacktraces)
                {
                    std::cout << "stack: " << i << std::endl;
                }
            }
        }
    }
};

struct mongo_diagnostics
{
    mongo_diagnostics();
    ~mongo_diagnostics();
};

#define MAKE_PERF_COUNTER() perfmon local_perf_##__COUNTER__(__LINE__, __FILE__, __PRETTY_FUNCTION__);

#endif // PERFMON_HPP_INCLUDED
