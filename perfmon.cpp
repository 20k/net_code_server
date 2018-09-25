#include "perfmon.hpp"
#include "mongo.hpp"

mongo_diagnostics::mongo_diagnostics()
{
    int* ptr = tls_get_print_performance_diagnostics();

    (*ptr)++;
}

mongo_diagnostics::~mongo_diagnostics()
{
    int* ptr = tls_get_print_performance_diagnostics();

    (*ptr)--;
}
