#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <userenv.h>
#include <wtsapi32.h>
#include <cstdio>
#include <string>

#define RUN_LOG_ERR(fmt, ...) printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#define RUN_LOG_WAR(fmt, ...) printf("[WAR] " fmt "\n", ##__VA_ARGS__)
#define RUN_LOG_DBG(fmt, ...) printf("[DBG] " fmt "\n", ##__VA_ARGS__)

static bool reboot_system()
{
    do
    {
        HANDLE token_handle = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token_handle))
        {
            RUN_LOG_ERR("open process token failed");
            break;
        }

        TOKEN_PRIVILEGES token_privileges;
        LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &token_privileges.Privileges[0].Luid);
        token_privileges.PrivilegeCount = 1;
        token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        AdjustTokenPrivileges(token_handle, FALSE, &token_privileges, 0, nullptr, 0);
        if (GetLastError() != ERROR_SUCCESS)
        {
            RUN_LOG_ERR("adjust token privileges failed");
            break;
        }
    } while (false);

    if (!InitiateSystemShutdownExW(NULL, NULL, 0, TRUE, TRUE, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_SECURITY))
    {
        if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_SECURITY))
        {
            RUN_LOG_ERR("exit windows failed");
            return (false);
        }
    }

    return (true);
}

static bool change_desktop()
{
    bool result = false;
    HWINSTA window_station = nullptr;
    HDESK active_desktop = nullptr;

    do
    {
        window_station = OpenWindowStationA("WinSta0", TRUE, GENERIC_ALL);
        if (nullptr == window_station)
        {
            RUN_LOG_ERR("open window station failed (%d)", GetLastError());
            break;
        }

        if (!SetProcessWindowStation(window_station))
        {
            RUN_LOG_ERR("set process window station failed (%d)", GetLastError());
            break;
        }

        active_desktop = OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK, FALSE, MAXIMUM_ALLOWED);
        if (nullptr == active_desktop)
        {
            RUN_LOG_ERR("open input desktop failed (%d)", GetLastError());
            break;
        }

        if (!SetThreadDesktop(active_desktop))
        {
            RUN_LOG_ERR("set thread desktop failed (%d)", GetLastError());
            break;
        }

        ret = true;
    } while (false);

    if (nullptr != window_station)
    {
        CloseWindowStation(window_station);
    }

    if (nullptr != active_desktop)
    {
        CloseDesktop(active_desktop);
    }

    return (ret);
}

static DWORD get_current_active_session_id()
{
    WTS_SESSION_INFOA * session_info = NULL;
    DWORD session_count = 0;
    if (WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &session_info, &session_count))
    {
        for (DWORD index = 0; index < session_count; ++index)
        {
            DWORD session_id = session_info[index].SessionId;
            LPSTR ptr_connect_state = nullptr;
            DWORD return_bytes = 0;
            if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, session_id, WTSConnectState, &ptr_connect_state, &return_bytes))
            {
                WTS_CONNECTSTATE_CLASS wts_connect_state = *reinterpret_cast<WTS_CONNECTSTATE_CLASS *>(ptr_connect_state);
                WTSFreeMemory(ptr_connect_state);
                if (WTSActive == wts_connect_state)
                {
                    return (session_id);
                }
            }
        }
    }
    return (0);
}

static bool service_create_user_process(const std::wstring & program_path, const std::wstring & program_command_line)
{
    bool result = false;
    HANDLE service_token = nullptr;
    HANDLE process_token = nullptr;
    LPVOID process_environment = nullptr;

    do
    {
        DWORD session_id = WTSGetActiveConsoleSessionId();
        if (!WTSQueryUserToken(session_id, &service_token))
        {
            session_id = get_current_active_session_id();
            if (!WTSQueryUserToken(session_id, &service_token))
            {
                if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES | TOKEN_ADJUST_SESSIONID | TOKEN_READ | TOKEN_WRITE, &service_token))
                {
                    RUN_LOG_ERR("open service token failure (%u)", GetLastError());
                    break;
                }
                session_id = WTSGetActiveConsoleSessionId();
            }
        }

        if (!CreateEnvironmentBlock(&process_environment, service_token, FALSE))
        {
            RUN_LOG_WAR("create environment block failure (%u)", GetLastError());
        }

        if (!DuplicateTokenEx(service_token, MAXIMUM_ALLOWED, 0, SecurityImpersonation, TokenPrimary, &process_token))
        {
            RUN_LOG_ERR("duplicate service token failure (%u)", GetLastError());
            break;
        }

        if (!SetTokenInformation(process_token, (TOKEN_INFORMATION_CLASS)TokenSessionId, &session_id, sizeof(session_id)))
        {
            RUN_LOG_ERR("set process (%s) token failure (%u)", "session id", GetLastError());
            break;
        }

        DWORD ui_control = 1;
        if (!SetTokenInformation(process_token, (TOKEN_INFORMATION_CLASS)TokenUIAccess, &ui_control, sizeof(ui_control)))
        {
            RUN_LOG_ERR("set process (%s) token failure (%u)", "ui access", GetLastError());
            break;
        }

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0x0 };
        if (!CreateProcessAsUserW(process_token, program_path.c_str(), const_cast<LPWSTR>(program_command_line.c_str()), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, process_environment, nullptr, &si, &pi))
        {
            RUN_LOG_ERR("create process as user failure while error (%u)", GetLastError());
            break;
        }

        RUN_LOG_DBG("create process as user success while pid (%u)", static_cast<uint32_t>(pi.dwProcessId));

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        result = true;
    } while (false);

    if (nullptr != service_token)
    {
        CloseHandle(service_token);
    }

    if (nullptr != process_token)
    {
        CloseHandle(process_token);
    }

    if (nullptr != process_environment)
    {
        DestroyEnvironmentBlock(process_environment);
        process_environment = nullptr;
    }

    return (result);
}
