#include <windows.h>
#include <cstdlib>
#include <set>
#include <list>
#include <string>
#include <iostream>

struct device_settings_t
{
    std::string     name;
    bool            primary;
    uint32_t        width;
    uint32_t        height;
    uint32_t        x_position;
    uint32_t        y_position;
};

struct display_settings_t
{
    std::string     name;
    bool            primary;
    uint32_t        bits;
    uint32_t        width;
    uint32_t        height;
    uint32_t        flags;
    uint32_t        frequency;
    uint32_t        delta_bits;
    uint32_t        delta_width;
    uint32_t        delta_height;
    uint32_t        delta_flags;
    uint32_t        delta_frequency;
};

void print_display_settings(const display_settings_t & display_settings)
{
    std::cout << "(" 
        << (display_settings.primary ? "true" : "false") << ", "
        << display_settings.name << ", " 
        << display_settings.width << ", " 
        << display_settings.height << ", " 
        << display_settings.frequency << ", " 
        << display_settings.flags << ", " 
        << display_settings.bits 
        << ")" << std::endl;
}

void print_display_settings_list(const std::list<display_settings_t> & display_settings_list)
{
    std::cout << "[primary, device, width, height, frequency, flags, bits]" << std::endl;
    for (std::list<display_settings_t>::const_iterator iter = display_settings_list.begin(); display_settings_list.end() != iter; ++iter)
    {
        print_display_settings(*iter);
    }
    std::cout << std::endl;
}

struct device_settings_value_less_t
{
    device_settings_value_less_t(bool change_primary, const std::string & primary_device)
        : m_change_primary(change_primary)
        , m_primary_device(primary_device)
    {

    }

    bool operator () (const device_settings_t & lhs, const device_settings_t & rhs) const
    {
        if (m_change_primary)
        {
            if (lhs.name != rhs.name)
            {
                if (lhs.name == m_primary_device)
                {
                    return (true);
                }

                if (rhs.name == m_primary_device)
                {
                    return (false);
                }
            }
        }
        else
        {
            if (lhs.primary != rhs.primary)
            {
                return (lhs.primary);
            }
        }

        if (lhs.x_position != rhs.x_position)
        {
            return (lhs.x_position < rhs.x_position);
        }

        if (lhs.y_position != rhs.y_position)
        {
            return (lhs.y_position < rhs.y_position);
        }

        return (false);
    }

    bool            m_change_primary;
    std::string     m_primary_device;
};

struct display_settings_value_equal_t
{
    bool operator () (const display_settings_t & lhs, const display_settings_t & rhs) const
    {
        if (lhs.width != rhs.width)
        {
            return (false);
        }

        if (lhs.height != rhs.height)
        {
            return (false);
        }

        if (lhs.frequency != rhs.frequency)
        {
            return (false);
        }

        if (lhs.flags != rhs.flags)
        {
            return (false);
        }

        if (lhs.bits != rhs.bits)
        {
            return (false);
        }

        return (true);
    }
};

struct display_settings_value_less_t
{
    bool operator () (const display_settings_t & lhs, const display_settings_t & rhs) const
    {
        if (lhs.name != rhs.name)
        {
            return (lhs.name < rhs.name);
        }

        if (lhs.bits != rhs.bits)
        {
            return (lhs.bits < rhs.bits);
        }

        if (lhs.width != rhs.width)
        {
            return (lhs.width < rhs.width);
        }

        if (lhs.height != rhs.height)
        {
            return (lhs.height < rhs.height);
        }

        if (lhs.flags != rhs.flags)
        {
            return (lhs.flags < rhs.flags);
        }

        if (lhs.frequency != rhs.frequency)
        {
            return (lhs.frequency < rhs.frequency);
        }

        return (false);
    }
};

struct display_settings_delta_less_t
{
    bool operator () (const display_settings_t & lhs, const display_settings_t & rhs) const
    {
        if (lhs.delta_width != rhs.delta_width)
        {
            return (lhs.delta_width < rhs.delta_width);
        }

        if (lhs.delta_height != rhs.delta_height)
        {
            return (lhs.delta_height < rhs.delta_height);
        }

        if (lhs.delta_flags != rhs.delta_flags)
        {
            return (lhs.delta_flags < rhs.delta_flags);
        }

        if (lhs.delta_frequency != rhs.delta_frequency)
        {
            return (lhs.delta_frequency < rhs.delta_frequency);
        }

        if (lhs.delta_bits != rhs.delta_bits)
        {
            return (lhs.delta_bits < rhs.delta_bits);
        }

        if (lhs.primary != rhs.primary)
        {
            return (lhs.primary);
        }

        return (false);
    }
};

bool get_all_display_settings(std::list<device_settings_t> & device_settings_list, std::list<display_settings_t> & display_settings_list)
{
    device_settings_list.clear();
    display_settings_list.clear();

    std::set<display_settings_t, display_settings_value_less_t> display_settings_set;

    for (DWORD device_index = 0; true; ++device_index)
    {
        DISPLAY_DEVICEA display_device = { 0x0 };
        display_device.cb = sizeof(display_device);
        if (!EnumDisplayDevicesA(nullptr, device_index, &display_device, EDD_GET_DEVICE_INTERFACE_NAME))
        {
            break;
        }

        if (DISPLAY_DEVICE_MIRRORING_DRIVER & display_device.StateFlags)
        {
            continue;
        }

        if (!(DISPLAY_DEVICE_ATTACHED_TO_DESKTOP & display_device.StateFlags))
        {
            continue;
        }

        DEVMODEA device_mode = { 0x0 };
        device_mode.dmSize = sizeof(device_mode);
        if (!EnumDisplaySettingsA(display_device.DeviceName, ENUM_CURRENT_SETTINGS, &device_mode))
        {
            continue;
        }

        device_settings_t device_settings;
        device_settings.name = display_device.DeviceName;
        device_settings.primary = (DISPLAY_DEVICE_PRIMARY_DEVICE & display_device.StateFlags);
        device_settings.width = device_mode.dmPelsWidth;
        device_settings.height = device_mode.dmPelsHeight;
        device_settings.x_position = device_mode.dmPosition.x;
        device_settings.y_position = device_mode.dmPosition.y;

        device_settings_list.push_back(device_settings);

        display_settings_t display_settings;
        display_settings.name = display_device.DeviceName;
        display_settings.primary = (DISPLAY_DEVICE_PRIMARY_DEVICE & display_device.StateFlags);

        for (DWORD mode_index = 0; true; ++mode_index)
        {
            if (!EnumDisplaySettingsA(display_device.DeviceName, mode_index, &device_mode))
            {
                break;
            }

            if (DM_INTERLACED & device_mode.dmDisplayFlags)
            {
                continue;
            }

            display_settings.bits = device_mode.dmBitsPerPel;
            display_settings.width = device_mode.dmPelsWidth;
            display_settings.height = device_mode.dmPelsHeight;
            display_settings.flags = device_mode.dmDisplayFlags;
            display_settings.frequency = device_mode.dmDisplayFrequency;
            display_settings.delta_bits = 0;
            display_settings.delta_width = 0;
            display_settings.delta_height = 0;
            display_settings.delta_flags = 0;
            display_settings.delta_frequency = 0;

            if (!display_settings_set.insert(display_settings).second)
            {
                continue;
            }

            display_settings_list.push_back(display_settings);
        }
    }

    return (!device_settings_list.empty() && !display_settings_list.empty());
}

bool get_display_settings(const char * device_name, display_settings_t & display_settings)
{
    DEVMODEA device_mode = { 0x0 };
    device_mode.dmSize = sizeof(device_mode);
    if (!EnumDisplaySettingsA(device_name, ENUM_CURRENT_SETTINGS, &device_mode))
    {
        return (false);
    }

    display_settings.bits = device_mode.dmBitsPerPel;
    display_settings.width = device_mode.dmPelsWidth;
    display_settings.height = device_mode.dmPelsHeight;
    display_settings.flags = device_mode.dmDisplayFlags;
    display_settings.frequency = device_mode.dmDisplayFrequency;
    display_settings.delta_bits = 0;
    display_settings.delta_width = 0;
    display_settings.delta_height = 0;
    display_settings.delta_flags = 0;
    display_settings.delta_frequency = 0;

    return (true);
}

bool set_display_settings(std::list<device_settings_t> & device_settings_list, const display_settings_t & display_settings, bool change_primary)
{
    device_settings_value_less_t device_settings_value_less(change_primary, display_settings.name);
    device_settings_list.sort(device_settings_value_less);

    uint32_t x_position = 0;
    uint32_t y_position = 0;
    for (std::list<device_settings_t>::iterator iter = device_settings_list.begin(); device_settings_list.end() != iter; ++iter)
    {
        device_settings_t & device_settings = *iter;
        device_settings.x_position = x_position;
        device_settings.y_position = y_position;
        if (display_settings.name == device_settings.name)
        {
            device_settings.width = display_settings.width;
            device_settings.height = display_settings.height;
            if (change_primary)
            {
                device_settings.primary = true;
            }
        }
        else
        {
            if (change_primary)
            {
                device_settings.primary = false;
            }
        }

        DEVMODEA device_mode = { 0x0 };
        device_mode.dmSize = sizeof(device_mode);
        device_mode.dmPosition.x = x_position;
        device_mode.dmPosition.y = y_position;
        if (display_settings.name == device_settings.name)
        {
            device_mode.dmBitsPerPel = display_settings.bits;
            device_mode.dmPelsWidth = display_settings.width;
            device_mode.dmPelsHeight = display_settings.height;
            device_mode.dmDisplayFlags = display_settings.flags;
            device_mode.dmDisplayFrequency = display_settings.frequency;
            device_mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY | DM_POSITION;
            if (DISP_CHANGE_SUCCESSFUL != ChangeDisplaySettingsExA(device_settings.name.c_str(), &device_mode, nullptr, CDS_UPDATEREGISTRY | CDS_NORESET | (change_primary ? CDS_SET_PRIMARY : 0), nullptr))
            {
                return (false);
            }
        }
        else
        {
            device_mode.dmFields = DM_POSITION;
            if (DISP_CHANGE_SUCCESSFUL != ChangeDisplaySettingsExA(device_settings.name.c_str(), &device_mode, nullptr, CDS_UPDATEREGISTRY | CDS_NORESET, nullptr))
            {
                return (false);
            }
        }

        x_position += device_settings.width;
        y_position = 0;
    }

    return (DISP_CHANGE_SUCCESSFUL != ChangeDisplaySettingsExA(nullptr, nullptr, nullptr, 0, nullptr));
}

bool adjust_display_settings(std::list<device_settings_t> & device_settings_list, std::list<display_settings_t> & display_settings_list, uint32_t bits, uint32_t width, uint32_t height, uint32_t flags, uint32_t frequency)
{
    if (display_settings_list.empty())
    {
        return (false);
    }

    for (std::list<display_settings_t>::iterator iter = display_settings_list.begin(); display_settings_list.end() != iter; ++iter)
    {
        display_settings_t & display_settings = *iter;
        display_settings.delta_bits = (display_settings.bits >= bits ? display_settings.bits - bits : (bits - display_settings.bits) * 2);
        display_settings.delta_width = (display_settings.width >= width ? display_settings.width - width : (width - display_settings.width) + 1);
        display_settings.delta_height = (display_settings.height >= height ? display_settings.height - height : (height - display_settings.height) + 1);
        display_settings.delta_flags = (display_settings.flags >= flags ? display_settings.flags - flags : flags - display_settings.flags);
        display_settings.delta_frequency = (display_settings.frequency >= frequency ? display_settings.frequency - frequency : (frequency - display_settings.frequency) * 2);
    }

    display_settings_list.sort(display_settings_delta_less_t());

    const display_settings_t & dst_display_settings = display_settings_list.front();
    if (!set_display_settings(device_settings_list, dst_display_settings, true))
    {
        return (false);
    }

    display_settings_t src_display_settings;
    if (!get_display_settings(dst_display_settings.name.c_str(), src_display_settings))
    {
        return (false);
    }

    display_settings_value_equal_t display_settings_equal;
    return (display_settings_equal(src_display_settings, dst_display_settings));
}

int main()
{
    std::list<device_settings_t> device_settings_list;
    std::list<display_settings_t> display_settings_list;
    get_all_display_settings(device_settings_list, display_settings_list);

    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    print_display_settings_list(display_settings_list);
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;

    uint32_t test_settings[][5] =
    {
        { 32, 640,  480,  0, 60 },
        { 32, 800,  600,  0, 60 },
        { 32, 1024, 768,  0, 60 },
        { 32, 1400, 1050, 0, 60 },
        { 32, 1600, 1200, 0, 60 },
        { 32, 2048, 1536, 0, 60 } 
    };
    for (uint32_t index = 0; index < sizeof(test_settings) / sizeof(test_settings[0]); ++index)
    {
        std::cout << "------------------------------------------------------" << std::endl;
        std::cout << "need: " << test_settings[index][1] << ", " << test_settings[index][2] << ", " << test_settings[index][4] << std::endl;
        adjust_display_settings(device_settings_list, display_settings_list, test_settings[index][0], test_settings[index][1], test_settings[index][2], test_settings[index][3], test_settings[index][4]);
    }
    std::cout << "------------------------------------------------------" << std::endl;

    return 0;
}
