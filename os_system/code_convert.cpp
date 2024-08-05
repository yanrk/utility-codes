#include <string>
#include <locale>
#include <codecvt>
#include <iostream>

#if 1

static std::string unicode_to_ansi(const std::wstring & unic)
{
    static std::wstring_convert<std::codecvt_byname<wchar_t, char, std::mbstate_t>> converter(new std::codecvt_byname<wchar_t, char, std::mbstate_t>("chs"));
    return converter.to_bytes(unic);
}

static std::wstring ansi_to_unicode(const std::string & ansi)
{
    static std::wstring_convert<std::codecvt_byname<wchar_t, char, std::mbstate_t>> converter(new std::codecvt_byname<wchar_t, char, std::mbstate_t>("chs"));
    return converter.from_bytes(ansi);
}

static std::string unicode_to_utf8(const std::wstring & unic)
{
    static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.to_bytes(unic);
}

static std::wstring utf8_to_unicode(const std::string & utf8)
{
    static std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.from_bytes(utf8);
}

std::string ansi_to_utf8(const std::string & ansi)
{
    return unicode_to_utf8(ansi_to_unicode(ansi));
}

std::string utf8_to_ansi(const std::string & utf8)
{
    return unicode_to_ansi(utf8_to_unicode(utf8));
}

#else

#ifdef _MSC_VER

#include <windows.h>

std::string ansi_to_utf8(const std::string & ansi)
{
    if (ansi.empty())
    {
        return ansi;
    }

    std::wstring unicode;
    int chars_count = ::MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, nullptr, 0);
    if (chars_count > 1)
    {
        unicode.resize(chars_count - 1);
        ::MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, &unicode[0], chars_count - 1);
    }

    std::string utf8;
    int bytes_count = ::WideCharToMultiByte(CP_UTF8, 0, unicode.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes_count > 1)
    {
        utf8.resize(bytes_count - 1);
        ::WideCharToMultiByte(CP_UTF8, 0, unicode.c_str(), -1, &utf8[0], bytes_count - 1, nullptr, nullptr);
    }

    return utf8;
}

std::string utf8_to_ansi(const std::string & utf8)
{
    if (utf8.empty())
    {
        return utf8;
    }

    std::wstring unicode;
    int chars_count = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (chars_count > 1)
    {
        unicode.resize(chars_count - 1);
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &unicode[0], chars_count - 1);
    }

    std::string ansi;
    int bytes_count = ::WideCharToMultiByte(CP_ACP, 0, unicode.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes_count > 1)
    {
        ansi.resize(bytes_count - 1);
        ::WideCharToMultiByte(CP_ACP, 0, unicode.c_str(), -1, &ansi[0], bytes_count - 1, nullptr, nullptr);
    }

    return ansi;
}

#else

static const char * get_language()
{
    static char s_lang[64] = { 0x0 };
    if (0x0 == s_lang[0])
    {
        const char * lang = getenv("LANG");
        if (nullptr != lang)
        {
            const char * dot = strrchr(lang, '.');
            if (nullptr != dot)
            {
                strncpy(s_lang, dot + 1, sizeof(s_lang) - 1);
            }
            else
            {
                strncpy(s_lang, lang, sizeof(s_lang) - 1);
            }
        }
        else
        {
            strncpy(s_lang, "UTF-8", sizeof(s_lang) - 1);
        }
    }
    return s_lang;
}

static bool convert_string(const std::string & src_code, const std::string & dst_code, const std::string & src, std::string & dst)
{
    if (src.empty() || src_code == dst_code)
    {
        dst = src;
        return true;
    }

    dst.clear();
    iconv_t conv_handle = iconv_open(dst_code.c_str(), src_code.c_str());
    if (reinterpret_cast<iconv_t>(-1) == conv_handle)
    {
        return false;
    }

    char * src_str = const_cast<char *>(src.data());
    size_t src_len = src.size();
    size_t dst_len = src_len * 4;
    dst.resize(dst_len);
    char * dst_str = const_cast<char *>(dst.data());

    if (static_cast<size_t>(-1) != iconv(conv_handle, &src_str, &src_len, &dst_str, &dst_len))
    {
        dst.resize(dst.size() - dst_len);
    }
    else
    {
        dst.clear();
    }

    iconv_close(conv_handle);

    return !dst.empty();
}

std::string ansi_to_utf8(const std::string & ansi)
{
    std::string utf8;
    convert_string(get_language(), "UTF-8", ansi, utf8);
    return utf8;
}

std::string utf8_to_ansi(const std::string & utf8)
{
    std::string ansi;
    convert_string("UTF-8", get_language(), utf8, ansi);
    return ansi;
}

#endif // _MSC_VER

#endif // 1

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
