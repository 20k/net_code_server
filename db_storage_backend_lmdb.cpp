#include "db_storage_backend_lmdb.hpp"
#include <lmdb.h>
#include <assert.h>
#include <stdexcept>
#include <vector>
#include <string_view>
#include <optional>
#include <sstream>
#include <iostream>

#define CHECK_THROW(x) if(const int rc = x) { std::cout << rc << std::endl; throw std::runtime_error("DB Error " + std::to_string(rc));}
#define CHECK_ASSERT(x) if(const int rc = x) {printf("DB Error %i %s\n", rc, #x); assert(false && #x);}

struct db::backend
{
    MDB_env* env = nullptr;
    std::string storage;
    int db_count = 0;

    std::vector<MDB_dbi> dbis;

    backend(const std::string& _storage, int _db_count) : storage(_storage), db_count(_db_count)
    {
        /*std::vector<std::string> dirs = split(storage, '/');

        for(auto& i : dirs)
        {
            if(i == ".")
                continue;

            _mkdir(i.c_str());
        }*/

        CHECK_ASSERT(mdb_env_create(&env));

        mdb_env_set_maxdbs(env, 50);

        ///10000 MB
        mdb_env_set_mapsize(env, 10485760ull * 10000ull);

        CHECK_ASSERT(mdb_env_open(env, storage.c_str(), MDB_NOTLS, 0777));

        dbis.resize(db_count);

        for(int i=0; i < db_count; i++)
        {
            MDB_txn* transaction = nullptr;

            CHECK_ASSERT(mdb_txn_begin(env, nullptr, 0, &transaction));

            CHECK_ASSERT(mdb_dbi_open(transaction, std::to_string(i).c_str(), MDB_CREATE, &dbis[i]));

            CHECK_ASSERT(mdb_txn_commit(transaction));
        }
    }

    ~backend()
    {
        for(int i=0; i < (int)dbis.size(); i++)
        {
            mdb_dbi_close(env, dbis[i]);
        }

        mdb_env_close(env);
    }

    MDB_dbi get_db(int id) const
    {
        if(id < 0 || id >= (int)dbis.size())
            throw std::runtime_error("Bad db id");

        return dbis[id];
    }
};

db::backend& get_backend()
{
    static db::backend ret("./lmdb_storage", 50);

    return ret;
}

std::optional<db::data> do_read_tx(MDB_dbi dbi, const db::impl_tx& tx, std::string_view skey)
{
    MDB_val key = {skey.size(), const_cast<void*>((const void*)skey.data())};

    MDB_val data;

    MDB_cursor* cursor = nullptr;

    CHECK_THROW(mdb_cursor_open(tx.transaction, dbi, &cursor));

    if(mdb_cursor_get(cursor, &key, &data, MDB_SET_KEY) != 0)
    {
        mdb_cursor_close(cursor);

        return std::nullopt;
    }

    return db::data({(const char*)data.mv_data, data.mv_size}, cursor);
}

void do_write_tx(MDB_dbi dbi, const db::impl_tx& tx, std::string_view skey, std::string_view sdata)
{
    MDB_val key = {skey.size(), const_cast<void*>((const void*)skey.data())};
    MDB_val data = {sdata.size(), const_cast<void*>((const void*)sdata.data())};

    CHECK_THROW(mdb_put(tx.transaction, dbi, &key, &data, 0));
}

bool do_del_tx(MDB_dbi dbi, const db::impl_tx& tx, std::string_view skey)
{
    MDB_val key = {skey.size(), const_cast<void*>((const void*)skey.data())};

    int rval = mdb_del(tx.transaction, dbi, &key, nullptr);

    if(rval != 0 && rval != MDB_NOTFOUND)
    {
        std::cout << rval << std::endl;
        throw std::runtime_error("DB Error " + std::to_string(rval));
    }

    return rval != MDB_NOTFOUND;
}

db::data::data(std::string_view _data_view, MDB_cursor* _cursor) : cursor(_cursor), data_view(_data_view){}

db::data::data(db::data&& other)
{
    data_view = other.data_view;
    cursor = other.cursor;

    other.cursor = nullptr;
}

db::data::data::~data()
{
    if(cursor == nullptr)
        return;

    mdb_cursor_close(cursor);
}

db::impl_tx::impl_tx()
{
    exception_count = std::uncaught_exceptions();
}

MDB_txn* db::impl_tx::get()
{
    return transaction;
}

db::impl_tx::~impl_tx()
{
    if(transaction == nullptr)
        return;

    ///exception
    if(exception_count != std::uncaught_exceptions())
    {
        mdb_txn_abort(transaction);
    }
    ///no exception
    else
    {
        CHECK_ASSERT(mdb_txn_commit(transaction));
    }
}

db::read_tx::read_tx()
{
    CHECK_THROW(mdb_txn_begin(get_backend().env, nullptr, MDB_RDONLY, &transaction));
}

db::read_write_tx::read_write_tx()
{
    CHECK_THROW(mdb_txn_begin(get_backend().env, nullptr, 0, &transaction));
}

std::optional<db::data> db::read_tx::read(int _db_id, std::string_view skey)
{
    return do_read_tx(get_backend().get_db(_db_id), *this, skey);
}

void db::read_write_tx::write(int _db_id, std::string_view skey, std::string_view sdata)
{
    return do_write_tx(get_backend().get_db(_db_id), *this, skey, sdata);
}

bool db::read_write_tx::del(int _db_id, std::string_view skey)
{
    return do_del_tx(get_backend().get_db(_db_id), *this, skey);
}

db::bound_read_tx::bound_read_tx(int _db_id)
{
    dbid = get_backend().get_db(_db_id);
}

std::optional<db::data> db::bound_read_tx::read(std::string_view skey)
{
    return do_read_tx(dbid, *this, skey);
}

db::bound_read_write_tx::bound_read_write_tx(int _db_id)
{
    dbid = get_backend().get_db(_db_id);
}

std::optional<db::data> db::bound_read_write_tx::read(std::string_view skey)
{
    return do_read_tx(dbid, *this, skey);
}

void db::bound_read_write_tx::write(std::string_view skey, std::string_view sdata)
{
    return do_write_tx(dbid, *this, skey, sdata);
}

bool db::bound_read_write_tx::del(std::string_view skey)
{
    return do_del_tx(dbid, *this, skey);
}
