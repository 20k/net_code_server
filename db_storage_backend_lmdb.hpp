#ifndef DB_STORAGE_BACKEND_LMDB_HPP_INCLUDED
#define DB_STORAGE_BACKEND_LMDB_HPP_INCLUDED

#include <optional>
#include <string_view>

typedef unsigned int MDB_dbi;
typedef struct MDB_txn MDB_txn;
typedef struct MDB_cursor MDB_cursor;

namespace db
{
    struct backend;

    struct data
    {
        MDB_cursor* cursor = nullptr;
        std::string_view data_view;

        data(const data&) = delete;
        data& operator=(const data&) = delete;
        data& operator=(data&&) = delete;

        data(data&&);
        data(std::string_view _data_view, MDB_cursor* _cursor);
        ~data();
    };

    struct impl_tx
    {
        MDB_txn* transaction = nullptr;
        MDB_txn* get();

        impl_tx();
        ~impl_tx();

        impl_tx(const impl_tx&) = delete;
        impl_tx& operator=(const impl_tx&) = delete;
        impl_tx(impl_tx&&) = delete;
        impl_tx& operator=(impl_tx&&) = delete;

    private:
        int exception_count = 0;
    };

    struct read_tx : impl_tx
    {
        read_tx();

        std::optional<data> read(int _db_id, std::string_view skey);
    };

    struct read_write_tx : read_tx
    {
        read_write_tx();

        void write(int _db_id, std::string_view skey, std::string_view sdata);
        bool del(int _db_id, std::string_view skey); //returns true on successful deletion
    };

    struct bound_read_tx : read_tx
    {
        MDB_dbi dbid;

        bound_read_tx(int _db_id);

        std::optional<data> read(std::string_view skey);
    };

    struct bound_read_write_tx : read_write_tx
    {
        MDB_dbi dbid;

        bound_read_write_tx(int _db_id);

        std::optional<data> read(std::string_view skey);
        void write(std::string_view skey, std::string_view sdata);
        bool del(std::string_view skey); //returns true on successful deletion
    };
}


#endif // DB_STORAGE_BACKEND_LMDB_HPP_INCLUDED
