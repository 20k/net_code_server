#include "seccallers.hpp"

int my_timeout_check(void* udata)
{
    COOPERATE_KILL_UDATA(udata);

    return 0;
}
