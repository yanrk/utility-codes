#ifdef _MSC_VER
#include <windows.h>
#include <wincon.h>
#else
#include <signal.h>
#endif // _MSC_VER

#include <thread>
#include <chrono>
#include <iostream>

static bool s_running = true;

#ifdef _MSC_VER
static BOOL WINAPI console_handler(DWORD event)
{
    switch (event)
    {
    case CTRL_C_EVENT: // never come in
        std::cout << "CTRL_C_EVENT" << std::endl;
        break;
    case CTRL_BREAK_EVENT:
        std::cout << "CTRL_BREAK_EVENT" << std::endl;
        break;
    case CTRL_CLOSE_EVENT:
        std::cout << "CTRL_CLOSE_EVENT" << std::endl;
        break;
    case CTRL_LOGOFF_EVENT:
        std::cout << "CTRL_LOGOFF_EVENT" << std::endl;
        break;
    case CTRL_SHUTDOWN_EVENT:
        std::cout << "CTRL_SHUTDOWN_EVENT" << std::endl;
        break;
    default:
        std::cout << "UNKNOWN: " << event << std::endl;
        break;
    }
    s_running = false; // useless in windows
    return (TRUE); // it will terminate the process at here
}
#else
static void signal_handler(int signo)
{
    switch (signo)
    {
    case SIGHUP: // 1
        std::cout << "SIGHUP" << std::endl;
        break;
    case SIGINT: // 2
        std::cout << "SIGINT" << std::endl;
        break;
    case SIGQUIT: // 3
        std::cout << "SIGQUIT" << std::endl;
        break;
    case SIGTERM: // 15
        std::cout << "SIGTERM" << std::endl;
        break;
    default:
        std::cout << "UNKNOWN: " << signo << std::endl;
        break;
    }
    s_running = false;
}
#endif // _MSC_VER

void handle_signal()
{
#ifdef _MSC_VER
    HANDLE console_input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD console_input_mode = 0 ;
    GetConsoleMode(console_input, &console_input_mode);
    SetConsoleMode(console_input, ~ENABLE_PROCESSED_INPUT & console_input_mode); // disable input, so it can ignore ctrl+c
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif // _MSC_VER
}

int main()
{
    handle_signal();

    while (s_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "alive" << std::endl;
    }

    return 0;
}
