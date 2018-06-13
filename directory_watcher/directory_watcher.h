/********************************************************
 * Description : directory watcher
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#ifndef DIRECTORY_WATCHER_H
#define DIRECTORY_WATCHER_H


#ifdef _MSC_VER
    #include <wtypes.h>
#else
    #include <cstdint>
    #include <map>
#endif // _MSC_VER

#include <cstdio>
#include <cstdarg>
#include <string>

#ifdef DEBUG
    #define DBG_MESSAGE(fmt, ...)      printf(fmt "\n", ##__VA_ARGS__)
#else
    #define DBG_MESSAGE(fmt, ...)
#endif // DEBUG

struct WatchEvent
{
    enum value_t
    {
        none, 
        file_create, 
        file_delete, 
        file_modify  
    };
};

class WatchSink
{
public:
    virtual ~WatchSink() { }
    virtual void handle_event(WatchEvent::value_t watch_event, const std::string & pathname) = 0;
};

class DirectoryWatcher
{
public:
    DirectoryWatcher(WatchSink & watch_sink);
    ~DirectoryWatcher();

public:
    bool init(const std::string & directory);
    bool watch_wait();

private:
    void exit();

private:
    DirectoryWatcher(const DirectoryWatcher &) = delete;
    DirectoryWatcher & operator = (const DirectoryWatcher &) = delete;

#ifdef _MSC_VER
    WatchSink                     & m_sink;
    HANDLE                          m_watcher;
    std::string                     m_directory;
#else
    const std::string & search_watch(int description);
    int  search_watch(const std::string & directory);
    bool append_watch(const std::string & directory);
    bool append_watch(int description, const std::string & sub_directory);
    void remove_watch(int description);
    void remove_watch(const std::string & directory);
    void remove_watch(int description, const std::string & sub_directory);

    WatchSink                     & m_sink;
    uint32_t                        m_mask;
    int                             m_watcher;
    std::map<int, std::string>      m_description_directory_map;
    std::map<std::string, int>      m_directory_description_map;
#endif // _MSC_VER
};


#endif // DIRECTORY_WATCHER_H
