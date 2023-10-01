#include "popen_ex.h"

#ifdef _MSC_VER

#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include <cstdio>

FILE * goofer_popen(const char * command, const char * mode, size_t & child)
{
    child = reinterpret_cast<size_t>(INVALID_HANDLE_VALUE);

    if (nullptr == command || nullptr == mode)
    {
        return (nullptr);
    }

    bool binary_data = (0 != strchr(mode, 'b'));
    bool parent_read_child_write = (0 != strchr(mode, 'r'));
    bool parent_write_child_read = (0 != strchr(mode, 'w'));

    if (parent_read_child_write == parent_write_child_read)
    {
        return (nullptr);
    }

    HANDLE reader_handle = INVALID_HANDLE_VALUE;
    HANDLE writer_handle = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    if (!CreatePipe(&reader_handle, &writer_handle, &sa, 0))
    {
        return (nullptr);
    }

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    if (parent_read_child_write)
    {
        SetHandleInformation(reader_handle, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = writer_handle;
        si.hStdError = writer_handle;
        si.dwFlags |= STARTF_USESTDHANDLES;
    }
    else
    {
        SetHandleInformation(writer_handle, HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = reader_handle;
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    PROCESS_INFORMATION pi = { 0x0 };
    if (!CreateProcessA(nullptr, reinterpret_cast<LPSTR>(const_cast<char *>(command)), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(reader_handle);
        CloseHandle(writer_handle);
        return (nullptr);
    }

    CloseHandle(pi.hThread);

    int file_flags = (binary_data ? _O_BINARY : _O_TEXT);
    HANDLE file_handle = INVALID_HANDLE_VALUE;
    if (parent_read_child_write)
    {
        file_flags |= _O_RDONLY;
        file_handle = reader_handle;
        CloseHandle(writer_handle);
    }
    else
    {
        file_flags |= _O_APPEND;
        file_handle = writer_handle;
        CloseHandle(reader_handle);
    }

    int fd = _open_osfhandle(reinterpret_cast<intptr_t>(file_handle), file_flags);
    if (fd < 0)
    {
        CloseHandle(file_handle);
        CloseHandle(pi.hProcess);
        return (nullptr);
    }

    FILE * file = _fdopen(fd, mode);
    if (nullptr == file)
    {
        _close(fd);
        CloseHandle(pi.hProcess);
        return (nullptr);
    }

    child = reinterpret_cast<size_t>(pi.hProcess);

    return (file);
}

bool goofer_palive(size_t child)
{
    HANDLE child_process = reinterpret_cast<HANDLE>(child);
    if (INVALID_HANDLE_VALUE == child_process)
    {
        return (false);
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess(child_process, &exit_code))
    {
        return (false);
    }
    else if (STILL_ACTIVE != exit_code)
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

void goofer_pkill(size_t child)
{
    HANDLE child_process = reinterpret_cast<HANDLE>(child);
    if (INVALID_HANDLE_VALUE != child_process)
    {
        TerminateProcess(child_process, 9);
    }
}

int goofer_pclose(FILE * file, size_t child)
{
    HANDLE child_process = reinterpret_cast<HANDLE>(child);
    if (INVALID_HANDLE_VALUE != child_process)
    {
        CloseHandle(child_process);
    }

    if (nullptr == file)
    {
        return (-1);
    }
    return (fclose(file));
}

#else

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cstring>
#include <cstdio>

FILE * goofer_popen(const char * command, const char * mode, size_t & child)
{
    child = 0;

    if (nullptr == command || nullptr == mode)
    {
        return (nullptr);
    }

    bool parent_read_child_write = (0 != strchr(mode, 'r'));
    bool parent_write_child_read = (0 != strchr(mode, 'w'));
    if (parent_read_child_write == parent_write_child_read)
    {
        return (nullptr);
    }

    int pipe_fd[2] = { -1, -1 };
    if (pipe(pipe_fd) < 0)
    {
        return (nullptr);
    }

    pid_t cpid = fork();
    if (cpid < 0)
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return (nullptr);
    }
    else if (0 == cpid)
    {
        if (parent_read_child_write)
        {
            close(pipe_fd[0]);
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(pipe_fd[1], STDERR_FILENO);
            close(pipe_fd[1]);
        }
        else
        {
            close(pipe_fd[1]);
            dup2(pipe_fd[0], STDIN_FILENO);
            close(pipe_fd[0]);
        }

        execl("/bin/sh", "sh", "-c", command, nullptr);
        _exit(0);

        return (nullptr);
    }
    else
    {
        int using_file = (parent_read_child_write ? 0 : 1);
        int unuse_file = (1 - using_file);

        close(pipe_fd[unuse_file]);

        FILE * file = fdopen(pipe_fd[using_file], mode);
        if (nullptr == file)
        {
            close(pipe_fd[using_file]);
            return (nullptr);
        }

        child = static_cast<size_t>(cpid);

        return (file);
    }
}

bool goofer_palive(size_t child)
{
    if (0 == child)
    {
        return (false);
    }

    pid_t cpid = static_cast<pid_t>(child);
    pid_t epid = waitpid(cpid, nullptr, WNOHANG);
    if (epid < 0)
    {
        return (false);
    }
    else if (epid == cpid)
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

void goofer_pkill(size_t child)
{
    if (0 != child)
    {
        pid_t cpid = static_cast<pid_t>(child);
        kill(cpid, SIGKILL);
    }
}

int goofer_pclose(FILE * file, size_t child)
{
    if (nullptr == file)
    {
        return (-1);
    }
    return (fclose(file));
}

#endif // _MSC_VER

int main()
{
#ifdef _MSC_VER
    const char * command = "ping -n 4 baidu.com";
#else
    const char * command = "ping -c 4 baidu.com";
#endif // _MSC_VER
    size_t child = -1;
    FILE * file = goofer_popen(command, "r", child);
    if (nullptr == file)
    {
        return (-1);
    }

    char line[128] = { 0x0 };
    while (nullptr != fgets(line, sizeof(line), file))
    {
        printf("%s", line);
    }

    goofer_pclose(file, child);

    return (0);
}
