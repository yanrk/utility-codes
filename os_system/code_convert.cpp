#include <string>
#include <locale>
#include <codecvt>
#include <iostream>

static std::string unicode_to_ansi(const std::wstring & unic)
{
    static std::wstring_convert<std::codecvt_byname<wchar_t, char, std::mbstate_t>> converter(new std::codecvt_byname<wchar_t, char, std::mbstate_t>("chs"));
    return (converter.to_bytes(unic));
}

static std::wstring ansi_to_unicode(const std::string & ansi)
{
    static std::wstring_convert<std::codecvt_byname<wchar_t, char, std::mbstate_t>> converter(new std::codecvt_byname<wchar_t, char, std::mbstate_t>("chs"));
    return (converter.from_bytes(ansi));
}

static std::string unicode_to_utf8(const std::wstring & unic)
{
    static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return (converter.to_bytes(unic));
}

static std::wstring utf8_to_unicode(const std::string & utf8)
{
    static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return (converter.from_bytes(utf8));
}

static std::string ansi_to_utf8(const std::string & ansi)
{
    return (unicode_to_utf8(ansi_to_unicode(ansi)));
}

static std::string utf8_to_ansi(const std::string & utf8)
{
    return (unicode_to_ansi(utf8_to_unicode(utf8)));
}

int main()
{
    std::string ansi("我爱你，亲爱的中国！Yes, I Do!");

    std::cout << "test" << std::endl;

    std::string utf8 = ansi_to_utf8(ansi);
    if (utf8_to_ansi(utf8) != ansi)
    {
        return (1);
    }

    std::wstring unic = ansi_to_unicode(ansi);
    if (unicode_to_ansi(unic) != ansi)
    {
        return (2);
    }

    if (utf8_to_unicode(utf8) != unic)
    {
        return (3);
    }

    if (unicode_to_utf8(unic) != utf8)
    {
        return (4);
    }

    std::cout << "done" << std::endl;
    return (0);
}
