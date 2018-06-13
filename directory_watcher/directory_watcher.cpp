/********************************************************
 * Description : directory watcher
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Blog        : blog.csdn.net/cxxmaker
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#ifdef _MSC_VER
    #include <windows.h>
#else
    #include <cstring>
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/inotify.h>
    #include <list>
#endif // _MSC_VER

#include "directory_watcher.h"

#ifdef _MSC_VER

DirectoryWatcher::DirectoryWatcher(WatchSink & watch_sink)
    : m_sink(watch_sink)
    , m_watcher(INVALID_HANDLE_VALUE)
    , m_directory()
{

}

DirectoryWatcher::~DirectoryWatcher()
{
    exit();
}

bool DirectoryWatcher::init(const std::string & directory)
{
    if (INVALID_HANDLE_VALUE != m_watcher)
    {
        DBG_MESSAGE("init watcher failure while watcher has initialized");
        return (false);
    }

    if (!directory.empty() && directory.back() != '/' && directory.back() != '\\')
    {
        m_directory = directory + "\\";
    }
    else
    {
        m_directory = directory;
    }

    if (INVALID_HANDLE_VALUE == (m_watcher = CreateFile(m_directory.c_str(), GENERIC_READ | GENERIC_WRITE | FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL)))
    {
        DBG_MESSAGE("init watcher failure while create file failed");
        return (false);
    }

    return (true);
}

void DirectoryWatcher::exit()
{
    if (INVALID_HANDLE_VALUE != m_watcher)
    {
        CloseHandle(m_watcher);
        m_watcher = INVALID_HANDLE_VALUE;
        m_directory.clear();
    }
}

bool DirectoryWatcher::watch_wait()
{
    if (INVALID_HANDLE_VALUE == m_watcher)
    {
        DBG_MESSAGE("watcher wait events failure while watcher is invalid");
        return (false);
    }

    while (true)
    {
        char event_buf[4096] = { 0x0 };
        DWORD event_len = 0;
        if (!ReadDirectoryChangesW(m_watcher, event_buf, sizeof(event_buf), true, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE, &event_len, NULL, NULL))
        {
            DBG_MESSAGE("watcher wait events failed while read directory change failed");
            break;
        }

        const char * event_pos = event_buf;
        while (0 != event_len)
        {
            if (event_len < sizeof(FILE_NOTIFY_INFORMATION))
            {
                DBG_MESSAGE("watcher wait events exception while read event imcomplete");
                break;
            }

            const FILE_NOTIFY_INFORMATION * notify = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(event_pos);

            char pathname[1024] = { 0x0 };
            WideCharToMultiByte(CP_ACP, 0, notify->FileName, notify->FileNameLength / 2, pathname, sizeof(pathname), NULL, NULL);

            WatchEvent::value_t watch_event = WatchEvent::none;
            switch (notify->Action)
            {
                case FILE_ACTION_ADDED:
                case FILE_ACTION_RENAMED_NEW_NAME:
                {
                    watch_event = WatchEvent::file_create;
                    break;
                }
                case FILE_ACTION_REMOVED:
                case FILE_ACTION_RENAMED_OLD_NAME:
                {
                    watch_event = WatchEvent::file_delete;
                    break;
                }
                case FILE_ACTION_MODIFIED:
                {
                    watch_event = WatchEvent::file_modify;
                    break;
                }
                default:
                {
                    break;
                }
            }

            if (WatchEvent::none != watch_event)
            {
                m_sink.handle_event(watch_event, m_directory + pathname);
            }

            if (0 != notify->NextEntryOffset)
            {
                event_pos += notify->NextEntryOffset;
                event_len -= notify->NextEntryOffset;
            }
            else
            {
                break;
            }
        }
    }

    return (true);
}

#else

DirectoryWatcher::DirectoryWatcher(WatchSink & watch_sink)
    : m_sink(watch_sink)
    , m_mask(IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_MOVE_SELF | IN_DELETE_SELF | IN_ISDIR)
    , m_watcher(-1)
    , m_description_directory_map()
    , m_directory_description_map()
{

}

DirectoryWatcher::~DirectoryWatcher()
{
    exit();
}

bool DirectoryWatcher::init(const std::string & directory)
{
    if (-1 != m_watcher)
    {
        DBG_MESSAGE("init watcher failure while watcher has initialized");
        return (false);
    }

    if (-1 == (m_watcher = inotify_init()))
    {
        DBG_MESSAGE("init watcher failure while inotify init failed");
        return (false);
    }

    std::list<std::string> dir_list;
    if (!directory.empty() && directory.back() != '/')
    {
        dir_list.push_back(directory + "/");
    }
    else
    {
        dir_list.push_back(directory);
    }

    while (!dir_list.empty())
    {
        const std::string dir_name(dir_list.front());
        dir_list.pop_front();

        DIR * dir = opendir(dir_name.c_str());
        if (nullptr == dir)
        {
            DBG_MESSAGE("init watcher exception while open directory (%s) failed", dir_name.c_str());
            continue;
        }
        struct dirent * ent = nullptr;
        while (nullptr != (ent = readdir(dir)))
        {
            if ((0 == strcmp(ent->d_name, ".")) || (0 == strcmp(ent->d_name, "..")))
            {
                continue;
            }
            struct stat64 stat_res = { 0x0 };
            const std::string path_name(dir_name + ent->d_name);
            if (0 != lstat64(path_name.c_str(), &stat_res))
            {
                DBG_MESSAGE("init watcher exception while lstat64 sub path (%s) failed", path_name.c_str());
                continue;
            }
            if (S_ISDIR(stat_res.st_mode) && !S_ISLNK(stat_res.st_mode))
            {
                dir_list.push_back(path_name + "/");
            }
        }
        closedir(dir);

        append_watch(dir_name);
    }

    return (!m_description_directory_map.empty());
}

void DirectoryWatcher::exit()
{
    if (-1 != m_watcher)
    {
        for (std::map<int, std::string>::const_iterator iter = m_description_directory_map.begin(); m_description_directory_map.end() != iter; ++iter)
        {
            inotify_rm_watch(m_watcher, iter->first);
        }
        m_description_directory_map.clear();
        m_directory_description_map.clear();
        close(m_watcher);
        m_watcher = -1;
    }
}

const std::string & DirectoryWatcher::search_watch(int description)
{
    std::map<int, std::string>::const_iterator iter = m_description_directory_map.find(description);
    if (m_description_directory_map.end() != iter)
    {
        return (iter->second);
    }
    else
    {
        DBG_MESSAGE("search watch failed while watcher map can not find (%d)", description);
        static const std::string s_directory;
        return (s_directory);
    }
}

int DirectoryWatcher::search_watch(const std::string & directory)
{
    std::map<std::string, int>::const_iterator iter = m_directory_description_map.find(directory);
    if (m_directory_description_map.end() != iter)
    {
        return (iter->second);
    }
    else
    {
        DBG_MESSAGE("search watch failed while watcher map can not find (%s)", directory.c_str());
        return (-1);
    }
}

bool DirectoryWatcher::append_watch(const std::string & directory)
{
    int description = inotify_add_watch(m_watcher, directory.c_str(), m_mask);
    if (description > 0)
    {
        m_description_directory_map[description] = directory;
        m_directory_description_map[directory] = description;
        return (true);
    }
    else
    {
        DBG_MESSAGE("append watch for (%s) failed while watcher map size (%u)", directory.c_str(), static_cast<unsigned int>(m_description_directory_map.size()));
        return (false);
    }
}

bool DirectoryWatcher::append_watch(int description, const std::string & sub_directory)
{
    const std::string & directory = search_watch(description);
    if (!directory.empty())
    {
        return (append_watch(directory + sub_directory + "/"));
    }
    else
    {
        DBG_MESSAGE("append watch for (.../%s) failed while watcher map can not find (%d)", sub_directory.c_str(), description);
        return (false);
    }
}

void DirectoryWatcher::remove_watch(int description)
{
    std::map<int, std::string>::const_iterator iter = m_description_directory_map.find(description);
    if (m_description_directory_map.end() != iter)
    {
        m_directory_description_map.erase(iter->second);
        m_description_directory_map.erase(iter);
    }
    inotify_rm_watch(m_watcher, description);
}

void DirectoryWatcher::remove_watch(const std::string & directory)
{
    std::map<std::string, int>::const_iterator iter = m_directory_description_map.find(directory);
    if (m_directory_description_map.end() != iter)
    {
        inotify_rm_watch(m_watcher, iter->second);
        m_description_directory_map.erase(iter->second);
        m_directory_description_map.erase(iter);
    }
}

void DirectoryWatcher::remove_watch(int description, const std::string & sub_directory)
{
    const std::string & directory = search_watch(description);
    if (!directory.empty())
    {
        remove_watch(directory + sub_directory + "/");
    }
    else
    {
        DBG_MESSAGE("remove watch for (.../%s) failed while watcher map can not find (%d)", sub_directory.c_str(), description);
    }
}

bool DirectoryWatcher::watch_wait()
{
    if (-1 == m_watcher)
    {
        DBG_MESSAGE("watcher wait events failure while watcher is invalid");
        return (false);
    }

    while (true)
    {
        char event_buf[4096] = { 0x0 };
        int read_len = read(m_watcher, event_buf, sizeof(event_buf));
        if (read_len < static_cast<int>(sizeof(inotify_event)))
        {
            DBG_MESSAGE("watcher wait events failed while read event imcomplete");
            break;
        }

        size_t event_pos = 0;
        size_t event_len = read_len;
        while (event_len >= sizeof(inotify_event))
        {
            const inotify_event * event = reinterpret_cast<inotify_event *>(event_buf + event_pos);
            event_pos += sizeof(inotify_event) + event->len;
            event_len -= sizeof(inotify_event) + event->len;

            if (event->mask & IN_ISDIR)
            {
                if ((event->mask & IN_DELETE_SELF) || (event->mask & IN_MOVE_SELF))
                {
                    remove_watch(event->wd);
                }
                else if (0 != event->len)
                {
                    if (event->mask & IN_MOVED_FROM)
                    {
                        remove_watch(event->wd, event->name);
                    }
                    else if ((event->mask & IN_MOVED_TO) || (event->mask & IN_CREATE))
                    {
                        append_watch(event->wd, event->name);
                    }
                }
            }
            else
            {
                WatchEvent::value_t watch_event = WatchEvent::none;
                if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO))
                {
                    watch_event = WatchEvent::file_create;
                }
                else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM))
                {
                    watch_event = WatchEvent::file_delete;
                }
                else if ((event->mask & IN_MODIFY) || (event->mask & IN_CLOSE_WRITE))
                {
                    watch_event = WatchEvent::file_modify;
                }
                if (WatchEvent::none != watch_event)
                {
                    const std::string & directory = search_watch(event->wd);
                    if (!directory.empty())
                    {
                        m_sink.handle_event(watch_event, directory + event->name);
                    }
                }
            }
        }

        if (0 != event_len)
        {
            DBG_MESSAGE("watcher wait events failed while read event overflow");
            break;
        }
    }

    return (true);
}

#endif // _MSC_VER
