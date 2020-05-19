#include "db_storage_backend_lmdb.hpp"
#include <lmdb.h>
#include <assert.h>
#include <stdexcept>
#include <vector>
#include <string_view>
#include <optional>
#include <sstream>
#include <iostream>
#include "tls.hpp"
#include "mongo.hpp"

#define CHECK_THROW(x) if(const int rc = x) { std::cout << rc << std::endl; throw std::runtime_error("DB Error " + std::to_string(rc) + " " + #x);}
#define CHECK_ASSERT(x) if(const int rc = x) {printf("DB Error %i %s\n", rc, #x); assert(false && #x);}

boost::fibers::mutex thread_mut;

tls_variable<int, 0> lock_count;

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
        //mdb_env_set_mapsize(env, 10485760ull * 10000ull);
        mdb_env_set_mapsize(env, 1000 * 1000 * 1000);

        mdb_env_set_maxreaders(env, 1024);

        std::cout << "STORAGE " << storage << std::endl;

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

    if(mdb_get(tx.transaction, dbi, &key, &data) != 0)
    {
        return std::nullopt;
    }

    return db::data({(const char*)data.mv_data, data.mv_size});
}

void do_write_tx(MDB_dbi dbi, const db::impl_tx& tx, std::string_view skey, std::string_view sdata)
{
    MDB_val key = {skey.size(), const_cast<void*>((const void*)skey.data())};
    MDB_val data = {sdata.size(), const_cast<void*>((const void*)sdata.data())};

    //std::cout << "KEY " << skey.size() << " DATA " << sdata.size() << std::endl;

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

void do_drop_tx(MDB_dbi dbi, const db::impl_tx& tx)
{
    CHECK_THROW(mdb_drop(tx.transaction, dbi, 0));
}

std::vector<db::data> do_read_all_tx(MDB_dbi dbi, const db::impl_tx& tx)
{
    MDB_cursor* cursor = nullptr;

    CHECK_THROW(mdb_cursor_open(tx.transaction, dbi, &cursor));

    bool first = true;

    std::vector<db::data> ret;

    MDB_val key;
    MDB_val data;

    while(1)
    {
        int rc = -1;

        if(first)
            rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
        else
            rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);

        if(rc == MDB_NOTFOUND)
            break;

        first = false;

        ret.push_back(db::data({(const char*)data.mv_data, data.mv_size}));
    }

    mdb_cursor_close(cursor);

    return ret;
}

db::data::data(std::string_view _data_view) : data_view(_data_view){}

db::data::data(db::data&& other)
{
    data_view = other.data_view;
}

db::data::data::~data()
{

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
        //printf("STOP\n");

        CHECK_ASSERT(mdb_txn_commit(transaction));
    }
}

db::read_tx::read_tx() : read_tx(true)
{

}

db::read_tx::read_tx(bool enabled)
{
    if(enabled)
    {
        CHECK_THROW(mdb_txn_begin(get_backend().env, nullptr, MDB_RDONLY, &transaction));
    }
    else
    {
        (*lock_count.get())++;

        if((*lock_count.get()) > 1)
            assert(false);
    }
}

db::read_tx::~read_tx()
{

}

db::read_write_tx::read_write_tx() : read_tx(false), guard(thread_mut)
{
    //printf("START\n");

    CHECK_THROW(mdb_txn_begin(get_backend().env, nullptr, 0, &transaction));
}

db::read_write_tx::~read_write_tx()
{
    (*lock_count.get())--;
}

std::optional<db::data> db::read_tx::read(int _db_id, std::string_view skey)
{
    return do_read_tx(get_backend().get_db(_db_id), *this, skey);
}

std::vector<db::data> db::read_tx::read_all(int _db_id)
{
    return do_read_all_tx(get_backend().get_db(_db_id), *this);
}

void db::read_write_tx::write(int _db_id, std::string_view skey, std::string_view sdata)
{
    return do_write_tx(get_backend().get_db(_db_id), *this, skey, sdata);
}

bool db::read_write_tx::del(int _db_id, std::string_view skey)
{
    return do_del_tx(get_backend().get_db(_db_id), *this, skey);
}

void db::read_write_tx::drop(int _db_id)
{
    do_drop_tx(get_backend().get_db(_db_id), *this);
}

size_t db::get_next_id(db::read_write_tx& rwtx)
{
    std::optional<db::data> dat = rwtx.read(get_backend().get_db((int)mongo_database_type::GLOBAL_ID_COUNTER), "g");

    size_t gid = 0;

    if(dat.has_value())
    {
        if(dat.value().data_view.size() != 8)
            throw std::runtime_error("Super bad, db id size != 8");

        const char* ptr = (const char*)dat.value().data_view.data();

        gid = *(size_t*)ptr;
    }

    gid++;

    char* ptr = (char*)&gid;

    rwtx.write(get_backend().get_db((int)mongo_database_type::GLOBAL_ID_COUNTER), "g", std::string_view(ptr, sizeof(size_t)));

    return gid;
}

/*db::bound_read_tx::bound_read_tx(int _db_id)
{
    dbid = get_backend().get_db(_db_id);
}

db::bound_read_tx::bound_read_tx(int _db_id, bool enabled) : read_tx(enabled)
{
    dbid = get_backend().get_db(_db_id);
}

db::bound_read_tx::~bound_read_tx()
{

}

std::optional<db::data> db::bound_read_tx::read(std::string_view skey)
{
    return do_read_tx(dbid, *this, skey);
}

std::optional<db::data> db::bound_read_tx::read_all()
{
    return do_read_all_tx(dbid, *this);
}

db::bound_read_write_tx::bound_read_write_tx(int _db_id) : bound_read_tx(_db_id, false)
{
    CHECK_THROW(mdb_txn_begin(get_backend().env, nullptr, 0, &transaction));
}

db::bound_read_write_tx::~bound_read_write_tx()
{
    (*lock_count.get())--;
}

void db::bound_read_write_tx::write(std::string_view skey, std::string_view sdata)
{
    return do_write_tx(dbid, *this, skey, sdata);
}

bool db::bound_read_write_tx::del(std::string_view skey)
{
    return do_del_tx(dbid, *this, skey);
}*/

void db_tests()
{
    //get_backend() = db::backend("./lmdb_test", 50);

    /*{
        db::read_write_tx tx;

        tx.write(0, "key", "value");
        tx.write(0, "key1", "value2");
        tx.write(0, "key2", "value3");

        auto val = tx.read(0, "key");

        std::cout << "VHAS " << val.has_value() << std::endl;
    }

    {
        db::read_write_tx tx;
        tx.write(0, "key", "WEOW");
    }

    {
        db::read_write_tx tx;

        auto all = tx.read_all(0);

        for(db::data& i : all)
        {
            std::cout << "FOUND " << i.data_view << std::endl;
        }
    }*/

    //get_backend() = db::backend("./lmdb_storage", 50);
}
