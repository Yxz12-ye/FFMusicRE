#include "GBKHelper.h"
#include<locale>
#include<codecvt>

std::string wstr_to_GBK(TagLib::String src)
{
    std::wstring temp_str = src.toCWString();
    const char* GBK_LOCALE_NAME = ".936";
	std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
    return convert.to_bytes(temp_str);
}
