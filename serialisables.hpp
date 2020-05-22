#ifndef SERIALISABLES_HPP_INCLUDED
#define SERIALISABLES_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include "db_storage_backend_lmdb.hpp"

struct mongo_lock_proxy;
struct mongo_read_proxy;

#define DECLARE_GENERIC_DB(type, keytype) \
    bool db_disk_load(db::read_tx& ctx, type& val, const keytype& key_val); \
    bool db_disk_exists(db::read_tx& ctx, const type& val); \
    void db_disk_remove(db::read_write_tx& ctx, const type& val); \
    void db_disk_overwrite(db::read_write_tx& ctx, type& val); \
    std::vector<type> db_disk_load_all(db::read_tx& ctx, const type& dummy); \
    void db_disk_remove_all(db::read_write_tx& ctx, const type& dummy);

#define DEFINE_GENERIC_DB(type, keytype, key_name, db_id) \
    bool db_disk_load(db::read_tx& ctx, type& val, const keytype& key_val) {return db_load_impl(val, ctx, #key_name, key_val, (int)db_id);} \
    bool db_disk_exists(db::read_tx& ctx, const type& val) {return db_exists_impl(ctx, #key_name, val.key_name, (int)db_id);} \
    void db_disk_remove(db::read_write_tx& ctx, const type& val) {return db_remove_impl(ctx, #key_name, val.key_name, (int)db_id);} \
    void db_disk_overwrite(db::read_write_tx& ctx, type& val) {return db_overwrite_impl(val, ctx, #key_name, val.key_name, (int)db_id);} \
    std::vector<type> db_disk_load_all(db::read_tx& ctx, const type& dummy) {return db_load_all_impl<type>(ctx, #key_name, (int)db_id);} \
    void db_disk_remove_all(db::read_write_tx& ctx, const type& dummy){return db_remove_all_impl<type>(ctx, #key_name, (int)db_id);}


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
DECLARE_SERIALISE_FUNCTION(task_data_db);
DECLARE_SERIALISE_FUNCTION(event_impl);
DECLARE_SERIALISE_FUNCTION(quest);
DECLARE_SERIALISE_FUNCTION(low_level_structure);
DECLARE_SERIALISE_FUNCTION(item);
DECLARE_SERIALISE_FUNCTION(playspace_network_link);
DECLARE_SERIALISE_FUNCTION(chat_channel);
DECLARE_SERIALISE_FUNCTION(chat_message);

namespace entity
{
    struct ship;
}

DECLARE_SERIALISE_FUNCTION(entity::ship);

DECLARE_GENERIC_DB(npc_prop_list, std::string);
DECLARE_GENERIC_DB(event_impl, std::string);
DECLARE_GENERIC_DB(task_data_db, std::string);
DECLARE_GENERIC_DB(quest, std::string);
DECLARE_GENERIC_DB(low_level_structure, std::string);
DECLARE_GENERIC_DB(item, std::string);
DECLARE_GENERIC_DB(user, std::string);
DECLARE_GENERIC_DB(playspace_network_link, std::string);
DECLARE_GENERIC_DB(auth, std::string);
DECLARE_GENERIC_DB(chat_channel, std::string);
DECLARE_GENERIC_DB(chat_message, size_t);
DECLARE_GENERIC_DB(user_nodes, std::string);
DECLARE_GENERIC_DB(entity::ship, uint32_t);

#endif // SERIALISABLES_HPP_INCLUDED
