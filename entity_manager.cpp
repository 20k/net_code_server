#include "entity_manager.hpp"
#include <assert.h>

namespace
{
    struct timestamp_tester
    {
        timestamp_tester()
        {
            event_queue::event_stack<float> stk;

            stk.init(3);

            stk.interrupt(50, 60, 4);

            float val = stk.get(55);

            assert(val == 3.5);

            stk.interrupt(55, 75, 8);

            float val2 = stk.get(65);

            assert(val2 == 5.75);
        }
    };

    timestamp_tester test;
}
