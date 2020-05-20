#include "entity_manager.hpp"

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

            printf("VAL %f\n", val);
            exit(0);
        }
    };

    timestamp_tester test;
}
