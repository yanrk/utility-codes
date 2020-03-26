#include <windows.h>
#include <vector>

bool mk_keyboard(uint16_t keycode, bool down)
{
    INPUT input = { 0x0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keycode;
    input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP);
    return (SendInput(1, &input, sizeof(INPUT)) == 1);
}

bool mk_keyboard(uint16_t keycode, bool ctrl, bool alt, bool shift, bool down)
{
    if (VK_LCONTROL == keycode || VK_RCONTROL == keycode)
    {
        keycode = VK_CONTROL;
        ctrl = false;
    }
    if (VK_LMENU == keycode || VK_RMENU == keycode)
    {
        keycode = VK_MENU;
        alt = false;
    }
    if (VK_LSHIFT == keycode || VK_RSHIFT == keycode)
    {
        keycode = VK_SHIFT;
        shift = false;
    }
    INPUT input[4] = { 0x0 };
    uint32_t count = 0;
    if (down)
    {
        if (ctrl)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_CONTROL;
            ++count;
        }
        if (alt)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_MENU;
            ++count;
        }
        if (shift)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_SHIFT;
            ++count;
        }
        input[count].type = INPUT_KEYBOARD;
        input[count].ki.wVk = keycode;
        ++count;
    }
    else
    {
        input[count].type = INPUT_KEYBOARD;
        input[count].ki.wVk = keycode;
        input[count].ki.dwFlags = KEYEVENTF_KEYUP;
        ++count;
        if (shift)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_SHIFT;
            input[count].ki.dwFlags = KEYEVENTF_KEYUP;
            ++count;
        }
        if (alt)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_MENU;
            input[count].ki.dwFlags = KEYEVENTF_KEYUP;
            ++count;
        }
        if (ctrl)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_CONTROL;
            input[count].ki.dwFlags = KEYEVENTF_KEYUP;
            ++count;
        }
    }
    return (SendInput(count, input, sizeof(INPUT)) == count);
}

bool mk_keyboard_string(const char * str)
{
    if (nullptr == str)
    {
        return (false);
    }
    std::size_t len = strlen(str);
    if (0 == len)
    {
        return (true);
    }
    std::vector<INPUT> input(len * 4);
    memset(&input[0], 0x0, sizeof(INPUT) * input.size());
    uint32_t count = 0;
    for (std::size_t index = 0; index < len; ++index)
    {
        uint16_t keycode = VkKeyScanA(str[index]);
        bool shift = (keycode & 0x0100);
        if (shift)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_SHIFT;
            ++count;
        }
        keycode = (keycode & 0x00FF);
        input[count].type = INPUT_KEYBOARD;
        input[count].ki.wVk = keycode;
        ++count;
        input[count].type = INPUT_KEYBOARD;
        input[count].ki.wVk = keycode;
        input[count].ki.dwFlags = KEYEVENTF_KEYUP;
        ++count;
        if (shift)
        {
            input[count].type = INPUT_KEYBOARD;
            input[count].ki.wVk = VK_SHIFT;
            input[count].ki.dwFlags = KEYEVENTF_KEYUP;
            ++count;
        }
    }
    return (SendInput(count, &input[0], sizeof(INPUT)) == count);
}

enum class mk_mouse_button_t : uint8_t
{
    BUTTON_LEFT,
    BUTTON_MIDDLE,
    BUTTON_RIGHT,
    BUTTON_X1,
    BUTTON_X2
};

bool mk_mouse_button(mk_mouse_button_t button, bool down)
{
    INPUT input = { 0x0 };
    input.type = INPUT_MOUSE;
    switch (button)
    {
        case mk_mouse_button_t::BUTTON_LEFT:
        {
            input.mi.dwFlags = (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
            break;
        }
        case mk_mouse_button_t::BUTTON_MIDDLE:
        {
            input.mi.dwFlags = (down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
            break;
        }
        case mk_mouse_button_t::BUTTON_RIGHT:
        {
            input.mi.dwFlags = (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
            break;
        }
        case mk_mouse_button_t::BUTTON_X1:
        {
            input.mi.mouseData = XBUTTON1;
            input.mi.dwFlags = (down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
            break;
        }
        case mk_mouse_button_t::BUTTON_X2:
        {
            input.mi.mouseData = XBUTTON2;
            input.mi.dwFlags = (down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
            break;
        }
        default:
        {
            return (false);
            break;
        }
    }
    return (1 == SendInput(1, &input, sizeof(INPUT)));
}

bool mk_mouse_wheel(int wheel)
{
    INPUT input = { 0x0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = wheel;
    return (1 == SendInput(1, &input, sizeof(INPUT)));
}

bool mk_mouse_move_absolute(int x, int y)
{
    INPUT input = { 0x0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE; // | MOUSEEVENTF_VIRTUALDESK;
    input.mi.dx = static_cast<LONG>(x * 65535.0 / GetSystemMetrics(SM_CXSCREEN) + 0.5);
    input.mi.dy = static_cast<LONG>(y * 65535.0 / GetSystemMetrics(SM_CYSCREEN) + 0.5);
    return (1 == SendInput(1, &input, sizeof(INPUT)));
}

bool mk_mouse_move_relative(int dx, int dy) // something wrong
{
    static int s_screen_physical_width = 0;
    static int s_screen_physical_height = 0;
    static int s_screen_virtual_width = 0;
    static int s_screen_virtual_height = 0;
    if (0 == s_screen_physical_width || 0 == s_screen_physical_height || 0 == s_screen_virtual_width || 0 == s_screen_virtual_height)
    {
        HDC hdcScreen = GetDC(NULL);
        s_screen_physical_width = GetDeviceCaps(hdcScreen, HORZSIZE);
        s_screen_physical_height = GetDeviceCaps(hdcScreen, VERTSIZE);
        ReleaseDC(NULL, hdcScreen);
        s_screen_virtual_width = GetSystemMetrics(SM_CXSCREEN);
        s_screen_virtual_height = GetSystemMetrics(SM_CYSCREEN);
        if (0 == s_screen_physical_width || 0 == s_screen_physical_height || 0 == s_screen_virtual_width || 0 == s_screen_virtual_height)
        {
            return (false);
        }
    }
    INPUT input = { 0x0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
    input.mi.dx = dx; // static_cast<LONG>(1.0 * dx * s_screen_physical_width / s_screen_virtual_width);
    input.mi.dy = dy; // static_cast<LONG>(1.0 * dy * s_screen_physical_height / s_screen_virtual_height);
    return (1 == SendInput(1, &input, sizeof(INPUT)));
}

int main()
{
    Sleep(5000);
    mk_keyboard_string("1234567890\n");
    mk_keyboard_string("abcdefg\thijklmn\topqrst\tuvwxyz\n");
    mk_keyboard_string("~!@#$%^&*()_+`1234567890-=\n");
    mk_keyboard_string(":\";'<>?,./{}|[]\\/*-+.\n");
    mk_mouse_button(mk_mouse_button_t::BUTTON_LEFT, true);
    mk_mouse_button(mk_mouse_button_t::BUTTON_LEFT, false);
    mk_mouse_move_absolute(100, 100);
    return 0;
}
