#include "item.hpp"

int32_t item::get_new_id(mongo_lock_proxy& global_props_ctx)
{
    mongo_requester request;
    request.set_prop("items_id_is_gid", 1);

    std::vector<mongo_requester> found = request.fetch_from_db(global_props_ctx);

    if(found.size() == 0)
    {
        mongo_requester to_set;
        to_set.set_prop("items_id_is_gid", 1);
        to_set.set_prop("items_id_gid", 1);

        to_set.insert_in_db(global_props_ctx);

        return 0;
    }
    else
    {
        mongo_requester& fid = found[0];

        int32_t id = fid.get_prop_as_integer("items_id_gid");

        return id;
    }
}

bool item::exists_in_db(mongo_lock_proxy& ctx, const std::string& item_id)
{
    mongo_requester request;
    request.set_prop("item_id", item_id);

    return request.fetch_from_db(ctx).size() > 0;
}

void item::overwrite_in_db(mongo_lock_proxy& ctx)
{
    mongo_requester request;
    request.set_prop("item_id", properties["item_id"]);

    mongo_requester to_update;
    to_update.properties = properties;

    request.update_in_db_if_exact(ctx, to_update);
}

void item::create_in_db(mongo_lock_proxy& ctx)
{
    if(exists_in_db(ctx, properties["item_id"]))
        return;

    mongo_requester to_store;
    to_store.properties = properties;

    to_store.insert_in_db(ctx);
}

void item::load_from_db(mongo_lock_proxy& ctx, const std::string& item_id)
{
    mongo_requester request;
    request.set_prop("item_id", item_id);

    auto found = request.fetch_from_db(ctx);

    for(auto& i : found)
    {
        properties = i.properties;
    }
}
