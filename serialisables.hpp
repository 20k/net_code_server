#ifndef SERIALISABLES_HPP_INCLUDED
#define SERIALISABLES_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include "db_interfaceable.hpp"

DECLARE_SERIALISE_FUNCTION(user_limit);
DECLARE_SERIALISE_FUNCTION(timestamped_position);
DECLARE_SERIALISE_FUNCTION(timestamp_move_queue);
DECLARE_SERIALISE_FUNCTION(user);
DECLARE_SERIALISE_FUNCTION(auth);
DECLARE_SERIALISE_FUNCTION(user_log_fragment);
DECLARE_SERIALISE_FUNCTION(user_log);
DECLARE_SERIALISE_FUNCTION(user_node);
DECLARE_SERIALISE_FUNCTION(user_nodes);
DECLARE_SERIALISE_FUNCTION(npc_prop);
DECLARE_SERIALISE_FUNCTION(npc_prop_list);

namespace event
{
    struct event_impl;
}

DECLARE_SERIALISE_FUNCTION(event::event_impl);

DECLARE_GENERIC_DB(npc_prop_list, std::string);
DECLARE_GENERIC_DB(event::event_impl, std::string);

#endif // SERIALISABLES_HPP_INCLUDED
