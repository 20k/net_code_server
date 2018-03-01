#include "logging.hpp"

std::string lg::logfile;
std::ofstream* lg::output;

void lg::set_logfile(const std::string& file)
{
    if(output)
        delete output;

    output = new std::ofstream();

    output->open(file.c_str(), std::ofstream::out | std::ofstream::trunc);

    logfile = file;
}

void lg::redirect_to_stdout()
{
    std::streambuf* b1 = std::cout.rdbuf();

    std::ios* r2 = lg::output;

    r2->rdbuf(b1);
}

/*void lg::log(const std::string& str)
{
    output << str << std::endl;
}*/
