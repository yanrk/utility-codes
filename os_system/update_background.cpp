#include <windows.h>

bool update_background(const std::string & background)
{
    return (SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, const_cast<char *>(background.c_str()), SPIF_UPDATEINIFILE));
}
