#include "perfmon.hpp"
#include "mongo.hpp"

mongo_diagnostics::mongo_diagnostics()
{
    mongo_lock_proxy::print_performance_diagnostics++;
}

mongo_diagnostics::~mongo_diagnostics()
{
    mongo_lock_proxy::print_performance_diagnostics--;
}
