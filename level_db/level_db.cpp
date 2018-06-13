/********************************************************
 * Description : level db class
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/iterator.h"
#include "level_db.h"

LevelDBIterator::LevelDBIterator()
    : m_iter(nullptr)
{

}

LevelDBIterator::~LevelDBIterator()
{
    release();
}

void LevelDBIterator::begin(LevelDB & db)
{
    release();

    if (nullptr == db.m_level_db)
    {
        return;
    }

    m_iter = db.m_level_db->NewIterator(leveldb::ReadOptions());
    if (nullptr == m_iter)
    {
        return;
    }

    m_iter->SeekToFirst();
    if (m_iter->Valid())
    {
        return;
    }

    release();
}

bool LevelDBIterator::end()
{
    return (nullptr == m_iter);
}

void LevelDBIterator::next()
{
    if (nullptr == m_iter)
    {
        return;
    }

    if (!m_iter->Valid())
    {
        release();
        return;
    }

    m_iter->Next();

    if (!m_iter->Valid())
    {
        release();
        return;
    }
}

std::string LevelDBIterator::key()
{
    if (nullptr != m_iter && m_iter->Valid())
    {
        const std::string key_string = m_iter->key().ToString();
        return (key_string);
    }
    else
    {
        static const std::string key_empty;
        return (key_empty);
    }
}

std::string LevelDBIterator::value()
{
    if (nullptr != m_iter && m_iter->Valid())
    {
        const std::string value_string = m_iter->value().ToString();
        return (value_string.c_str());
    }
    else
    {
        static const std::string value_empty;
        return (value_empty);
    }
}

void LevelDBIterator::release()
{
    if (nullptr != m_iter)
    {
        delete m_iter;
        m_iter = nullptr;
    }
}

LevelDB::LevelDB()
    : m_level_db(nullptr)
    , m_level_db_pathname()
{

}

LevelDB::~LevelDB()
{
    close();
}

bool LevelDB::open(const std::string & db_path)
{
    close();
    m_level_db_pathname = db_path;
    leveldb::Options options;
    options.create_if_missing = true;
    return (leveldb::DB::Open(options, m_level_db_pathname.c_str(), &m_level_db).ok());
}

void LevelDB::close()
{
    if (nullptr != m_level_db)
    {
        delete m_level_db;
        m_level_db = nullptr;
    }
}

bool LevelDB::destroy()
{
    if (m_level_db_pathname.empty())
    {
        return (false);
    }

    close();
    std::string level_db_pathname(std::move(m_level_db_pathname));
    return (LevelDB::destroy(level_db_pathname));
}

bool LevelDB::repair(const std::string & db_path)
{
    return (RepairDB(db_path, leveldb::Options()).ok());
}

bool LevelDB::destroy(const std::string & db_path)
{
    return (DestroyDB(db_path, leveldb::Options()).ok());
}

bool LevelDB::do_set(const std::string & key, const std::string & value)
{
    leveldb::WriteOptions write_options;
    write_options.sync = false;
    return (nullptr != m_level_db && m_level_db->Put(write_options, key, value).ok());
}

bool LevelDB::do_get(const std::string & key, std::string & value)
{
    leveldb::ReadOptions read_options;
    read_options.fill_cache = true;
    return (nullptr != m_level_db && m_level_db->Get(read_options, key, &value).ok());
}

bool LevelDB::do_erase(const std::string & key)
{
    leveldb::WriteOptions write_options;
    write_options.sync = false;
    return (nullptr != m_level_db && m_level_db->Delete(write_options, key).ok());
}

bool LevelDB::set(const std::string & key, const std::string & value)
{
    return (do_set(key, value));
}

bool LevelDB::get(const std::string & key, std::string & value)
{
    if (do_get(key, value))
    {
        value = value.c_str();
        return (true);
    }
    else
    {
        return (false);
    }
}

bool LevelDB::erase(const std::string & key)
{
    return (do_erase(key));
}
