#ifndef SERIALISABLES_HPP_INCLUDED
#define SERIALISABLES_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include <networking/serialisable_msgpack_fwd.hpp>
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

DECLARE_MSG_FSERIALISE(user_limit);
DECLARE_MSG_FSERIALISE(timestamped_position);
DECLARE_MSG_FSERIALISE(timestamp_move_queue);
DECLARE_MSG_FSERIALISE(user);
DECLARE_MSG_FSERIALISE(auth);
DECLARE_MSG_FSERIALISE(user_log_fragment);
DECLARE_MSG_FSERIALISE(user_log);
DECLARE_MSG_FSERIALISE(user_node);
DECLARE_MSG_FSERIALISE(user_nodes);
DECLARE_MSG_FSERIALISE(npc_prop);
DECLARE_MSG_FSERIALISE(npc_prop_list);
DECLARE_MSG_FSERIALISE(task_data_db);
DECLARE_MSG_FSERIALISE(event_impl);
DECLARE_MSG_FSERIALISE(quest);
DECLARE_MSG_FSERIALISE(low_level_structure);
DECLARE_MSG_FSERIALISE(item);
DECLARE_MSG_FSERIALISE(playspace_network_link);
DECLARE_MSG_FSERIALISE(chat_channel);
DECLARE_MSG_FSERIALISE(chat_message);

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

#endif // SERIALISABLES_HPP_INCLUDED
