/********************************************************
 * Description : level db class
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#ifndef LEVEL_DB_H
#define LEVEL_DB_H


#include <cstring>
#include <string>

namespace leveldb
{
    class Iterator;
    class DB;
}

class LevelDB;

class LevelDBIterator
{
public:
    LevelDBIterator();
    ~LevelDBIterator();

public:
    void begin(LevelDB & db);
    bool end();
    void next();

public:
    std::string key();
    std::string value();

private:
    void release();

private:
    LevelDBIterator(const LevelDBIterator &) = delete;
    LevelDBIterator & operator = (const LevelDBIterator &) = delete;

private:
    leveldb::Iterator     * m_iter;
};

class LevelDB
{
public:
    LevelDB();
    ~LevelDB();

public:
    static bool repair(const std::string & db_path);
    static bool destroy(const std::string & db_path);

public:
    bool open(const std::string & db_path);
    void close();
    bool destroy();

public:
    bool set(const std::string & key, const std::string & value);
    bool get(const std::string & key, std::string & value);
    bool erase(const std::string & key);

public:
    template <typename T> bool set(const std::string & key, const T & value);
    template <typename T> bool get(const std::string & key, T & value);

private:
    bool do_set(const std::string & key, const std::string & value);
    bool do_get(const std::string & key, std::string & value);
    bool do_erase(const std::string & key);

private:
    LevelDB(const LevelDB &) = delete;
    LevelDB & operator = (const LevelDB &) = delete;

private:
    friend class LevelDBIterator;

private:
    leveldb::DB           * m_level_db;
    std::string             m_level_db_pathname;
};

template <typename T>
bool LevelDB::set(const std::string & key, const T & value)
{
    std::string str_value(sizeof(value), 0x0);
    memcpy(&str_value[0], &value, sizeof(value));
    return (do_set(key, str_value));
}

template <typename T>
bool LevelDB::get(const std::string & key, T & value)
{
    std::string str_value;
    if (!do_get(key, str_value))
    {
        return (false);
    }
    if (sizeof(value) != str_value.size())
    {
        return (false);
    }
    memcpy(&value, &str_value[0], sizeof(value));
    return (true);
}


#endif // LEVEL_DB_H
