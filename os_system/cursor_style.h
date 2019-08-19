#include <windows.h>
#include <string>

struct cursor_data_t
{
    uint32_t    x_hotspot;
    uint32_t    y_hotspot;
    uint32_t    mask_type;
    uint32_t    mask_width;
    uint32_t    mask_height;
    uint32_t    mask_width_bytes;
    uint16_t    mask_planes;
    uint16_t    mask_bits_pixel;
    uint32_t    mask_data_size;
    uint32_t    color_type;
    uint32_t    color_width;
    uint32_t    color_height;
    uint32_t    color_width_bytes;
    uint16_t    color_planes;
    uint16_t    color_bits_pixel;
    uint32_t    color_data_size;
};

static bool copy_bitmap_data(HBITMAP bitmap_handle, BITMAP & bitmap, std::string & bitmap_data)
{
    if (nullptr == bitmap_handle || 0 == GetObject(bitmap_handle, sizeof(bitmap), &bitmap))
    {
        bitmap_data.clear();
        return (false);
    }

    LONG bitmap_size = static_cast<LONG>((bitmap.bmHeight * bitmap.bmWidth * bitmap.bmBitsPixel) / 8);
    bitmap_data.resize(bitmap_size, 0x0);

    if (0 == GetBitmapBits(bitmap_handle, bitmap_size, &bitmap_data[0]))
    {
        bitmap_data.clear();
        return (false);
    }

    return (true);
}

static bool copy_icon_data(HICON icon_handle, cursor_data_t & cursor_data, std::string & data)
{
    ICONINFO icon_info = { 0x0 };
    if (nullptr == icon_handle || !GetIconInfo(icon_handle, &icon_info))
    {
        return (false);
    }

    cursor_data.x_hotspot = static_cast<uint32_t>(icon_info.xHotspot);
    cursor_data.y_hotspot = static_cast<uint32_t>(icon_info.yHotspot);

    if (nullptr != icon_info.hbmMask)
    {
        BITMAP mask_bitmap = { 0x0 };
        std::string mask_bitmap_data;
        if (copy_bitmap_data(icon_info.hbmMask, mask_bitmap, mask_bitmap_data))
        {
            cursor_data.mask_type = static_cast<uint32_t>(mask_bitmap.bmType);
            cursor_data.mask_width = static_cast<uint32_t>(mask_bitmap.bmWidth);
            cursor_data.mask_height = static_cast<uint32_t>(mask_bitmap.bmHeight);
            cursor_data.mask_width_bytes = static_cast<uint32_t>(mask_bitmap.bmWidthBytes);
            cursor_data.mask_planes = static_cast<uint32_t>(mask_bitmap.bmPlanes);
            cursor_data.mask_bits_pixel = static_cast<uint32_t>(mask_bitmap.bmBitsPixel);
            cursor_data.mask_data_size = static_cast<uint32_t>(mask_bitmap_data.size());
            if (!mask_bitmap_data.empty())
            {
                data.append(mask_bitmap_data.begin(), mask_bitmap_data.end());
            }
        }
        DeleteObject(icon_info.hbmMask);
    }

    if (nullptr != icon_info.hbmColor)
    {
        BITMAP color_bitmap = { 0x0 };
        std::string color_bitmap_data;
        if (copy_bitmap_data(icon_info.hbmColor, color_bitmap, color_bitmap_data))
        {
            cursor_data.color_type = static_cast<uint32_t>(color_bitmap.bmType);
            cursor_data.color_width = static_cast<uint32_t>(color_bitmap.bmWidth);
            cursor_data.color_height = static_cast<uint32_t>(color_bitmap.bmHeight);
            cursor_data.color_width_bytes = static_cast<uint32_t>(color_bitmap.bmWidthBytes);
            cursor_data.color_planes = static_cast<uint32_t>(color_bitmap.bmPlanes);
            cursor_data.color_bits_pixel = static_cast<uint32_t>(color_bitmap.bmBitsPixel);
            cursor_data.color_data_size = static_cast<uint32_t>(color_bitmap_data.size());
            if (!color_bitmap_data.empty())
            {
                data.append(color_bitmap_data.begin(), color_bitmap_data.end());
            }
        }
        DeleteObject(icon_info.hbmColor);
    }

    return (!data.empty());
}

bool cursor_capture(cursor_data_t & cursor_data, std::string & data)
{
    memset(&cursor_data, 0x0, sizeof(cursor_data));
    data.clear();

    CURSORINFO cursor_info = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&cursor_info))
    {
        return (false);
    }

    HICON icon_handle = CopyIcon(cursor_info.hCursor);
    if (nullptr == icon_handle)
    {
        return (false);
    }

    bool ret = copy_icon_data(icon_handle, cursor_data, data);

    DestroyIcon(icon_handle);

    return (ret);
}

bool cursor_update(const cursor_data_t & cursor_data, const char * data, uint32_t size)
{
    if (nullptr == data || 0 == size)
    {
        return (false);
    }

    if (0 == cursor_data.mask_data_size && 0 == cursor_data.color_data_size)
    {
        return (false);
    }

    if (cursor_data.mask_data_size + cursor_data.color_data_size != size)
    {
        return (false);
    }

    HBITMAP mask_bitmap_handle = nullptr;
    if (cursor_data.mask_width > 0 && cursor_data.mask_height > 0)
    {
        BITMAP mask_bitmap = { 0x0 };
        mask_bitmap.bmType = cursor_data.mask_type;
        mask_bitmap.bmWidth = cursor_data.mask_width;
        mask_bitmap.bmHeight = cursor_data.mask_height;
        mask_bitmap.bmWidthBytes = cursor_data.mask_width_bytes;
        mask_bitmap.bmPlanes = cursor_data.mask_planes;
        mask_bitmap.bmBitsPixel = cursor_data.mask_bits_pixel;
        if (cursor_data.mask_data_size > 0)
        {
            mask_bitmap.bmBits = const_cast<char *>(data);
        }
        else
        {
            mask_bitmap.bmBits = nullptr;
        }
        mask_bitmap_handle = CreateBitmapIndirect(&mask_bitmap);
    }

    HBITMAP color_bitmap_handle = nullptr;
    if (cursor_data.color_width > 0 && cursor_data.color_height > 0)
    {
        BITMAP color_bitmap = { 0x0 };
        color_bitmap.bmType = cursor_data.color_type;
        color_bitmap.bmWidth = cursor_data.color_width;
        color_bitmap.bmHeight = cursor_data.color_height;
        color_bitmap.bmWidthBytes = cursor_data.color_width_bytes;
        color_bitmap.bmPlanes = cursor_data.color_planes;
        color_bitmap.bmBitsPixel = cursor_data.color_bits_pixel;
        if (cursor_data.color_data_size > 0)
        {
            color_bitmap.bmBits = const_cast<char *>(data) + cursor_data.mask_data_size;
        }
        else
        {
            color_bitmap.bmBits = nullptr;
        }
        color_bitmap_handle = CreateBitmapIndirect(&color_bitmap);
    }

    if (nullptr == mask_bitmap_handle && nullptr == color_bitmap_handle)
    {
        return (false);
    }

    ICONINFO icon_info = { 0 };
    icon_info.fIcon = FALSE;
    icon_info.xHotspot = cursor_data.x_hotspot;
    icon_info.yHotspot = cursor_data.y_hotspot;
    icon_info.hbmMask = mask_bitmap_handle;
    icon_info.hbmColor = color_bitmap_handle;
    HCURSOR cursor_handle = reinterpret_cast<HCURSOR>(CreateIconIndirect(&icon_info));

    if (nullptr != mask_bitmap_handle)
    {
        DeleteObject(mask_bitmap_handle);
    }
    if (nullptr != color_bitmap_handle)
    {
        DeleteObject(color_bitmap_handle);
    }

    if (nullptr == cursor_handle)
    {
        return (false);
    }

    SetCursor(cursor_handle);
    DeleteObject(cursor_handle);

    return (true);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            break;
        }
        case WM_PAINT:
        {
            break;
        }
        case WM_SETCURSOR: // need add this when call cursor_update()
        {
            break;
        }
        default:
        {
            return (DefWindowProc(hwnd, message, wparam, lparam));
            break;
        }
    }

    return (0);
}
