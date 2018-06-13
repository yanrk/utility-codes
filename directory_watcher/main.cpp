/********************************************************
 * Description : test directory watcher
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#include "directory_watcher.h"

class ShowWatchSink : public WatchSink
{
public:
    virtual void handle_event(WatchEvent::value_t watch_event, const std::string & pathname)
    {
        if (WatchEvent::file_create == watch_event)
        {
            DBG_MESSAGE("create file: (%s)", pathname.c_str());
        }
        else if (WatchEvent::file_delete == watch_event)
        {
            DBG_MESSAGE("delete file: (%s)", pathname.c_str());
        }
        else if (WatchEvent::file_modify == watch_event)
        {
            DBG_MESSAGE("modify file: (%s)", pathname.c_str());
        }
    };
};

int main(int, char * [])
{
#ifdef _MSC_VER
    const char * directory = "d:/test/";
#else
    const char * directory = "/home/test/";
#endif // _MSC_VER
    ShowWatchSink sink;
    DirectoryWatcher watcher(sink);
    watcher.init(directory);
    watcher.watch_wait();
    return (0);
}
