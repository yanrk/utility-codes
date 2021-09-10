#include <windows.h>
#include <shlobj.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <string>

bool create_program_link(const std::wstring & file_path, const std::wstring & link_path)
{
    CoInitializeEx(0, COINIT_MULTITHREADED);

    IShellLinkW * shell_link = nullptr;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<LPVOID *>(&shell_link));
    if (!SUCCEEDED(result) || nullptr == shell_link)
    {
        return (false);
    }

    IPersistFile * persist_file = nullptr;
    result = shell_link->QueryInterface(IID_IPersistFile, reinterpret_cast<LPVOID *>(&persist_file));
    if (!SUCCEEDED(result) || nullptr == persist_file)
    {
        shell_link->Release();
        return (false);
    }

    shell_link->SetPath(file_path.c_str());
    persist_file->Save(link_path.c_str(), TRUE);
    persist_file->SaveCompleted(link_path.c_str());

    persist_file->Release();
    shell_link->Release();

    CoUninitialize();

    return (true);
}

bool create_program_desktop_link(const std::wstring & file_path, const std::wstring & link_name)
{
    wchar_t desktop[MAX_PATH] = { 0x0 };
    if (!SHGetSpecialFolderPathW(nullptr, desktop, CSIDL_DESKTOPDIRECTORY, FALSE))
    {
        return (false);
    }

    std::wstring link_path(std::wstring(desktop) + std::wstring(L"\\") + link_name + std::wstring(".lnk"));

    return (create_program_link(file_path, link_path));
}
