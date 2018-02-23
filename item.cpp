#include "item.hpp"

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
