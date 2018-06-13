/********************************************************
 * Description : test level db
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <iostream>
#include "level_db.h"

struct user_storage_t
{
    int32_t     user_id;
    int32_t     storage_id;
    uint64_t    file_count;
    uint64_t    file_bytes;
};

static bool operator != (const user_storage_t & lhs, const user_storage_t & rhs)
{
    return (0 != memcmp(&lhs, &rhs, sizeof(user_storage_t)));
}

int main()
{
    {
        const std::string db_path("G:\\testdb\\db1");

        if (!LevelDB::destroy(db_path))
        {
            assert(false);
        }

        LevelDB db;
        if (!db.open(db_path))
        {
            assert(false);
        }

        if (!db.erase("not exist"))
        {
            assert(false);
        }

        user_storage_t data_in = { 0x0 };
        data_in.user_id = 10001;
        data_in.storage_id = 34411;
        data_in.file_count = 11111;
        data_in.file_bytes = 123456789;
        if (!db.set("test", data_in))
        {
            assert(false);
        }
        user_storage_t data_out = { 0x0 };
        if (!db.get("test", data_out))
        {
            assert(false);
        }
        if (data_in != data_out)
        {
            assert(false);
        }
        if (!db.erase("test"))
        {
            assert(false);
        }
        if (db.get("test", data_out))
        {
            assert(false);
        }

        db.close();

        if (!db.destroy())
        {
            assert(false);
        }
    }

    {
        const std::string db_path("G:\\testdb\\db2");

        if (!LevelDB::destroy(db_path))
        {
            assert(false);
        }

        LevelDB db;
        if (!db.open(db_path))
        {
            assert(false);
        }

        LevelDBIterator iter;
        if (!iter.end())
        {
            assert(false);
        }

        iter.begin(db);
        if (!iter.end())
        {
            assert(false);
        }

        if (!db.set("a", "app"))
        {
            assert(false);
        }
        if (!db.set("b", "bak"))
        {
            assert(false);
        }
        if (!db.set("c", "cfg"))
        {
            assert(false);
        }
        if (!db.set("d", "dev"))
        {
            assert(false);
        }
        if (!db.set("e", "etc"))
        {
            assert(false);
        }

        std::cout << std::endl;
        for (iter.begin(db); !iter.end(); iter.next())
        {
            std::cout << "(" << iter.key() << ")" << " - " << "(" << iter.value() << ")" << std::endl;
        }
        std::cout << std::endl;

        if (!db.set("c", "1cfg"))
        {
            assert(false);
        }
        if (!db.set("d", "2dev"))
        {
            assert(false);
        }
        if (!db.set("e", "3etc"))
        {
            assert(false);
        }
        if (!db.set("f", "4fee"))
        {
            assert(false);
        }
        if (!db.set("a", "5app"))
        {
            assert(false);
        }
        if (!db.set("b", "6bak"))
        {
            assert(false);
        }

        std::cout << std::endl;
        for (iter.begin(db); !iter.end(); iter.next())
        {
            std::cout << "(" << iter.key() << ")" << " - " << "(" << iter.value() << ")" << std::endl;
        }
        std::cout << std::endl;

        db.close();

        if (!db.destroy())
        {
            assert(false);
        }
    }

    return 0;
}
