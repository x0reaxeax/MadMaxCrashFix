#ifndef _MADMAX_CRASHFIX_CONFIG_HPP
#define _MADMAX_CRASHFIX_CONFIG_HPP

#include <Windows.h>
#include <string>

void LogLine(
    const std::wstring& wsLine
);

namespace LocalConfig {
    constexpr LPCSTR CONFIG_FILE_PATH = "madmaxcfg.json";

    typedef struct _Config {
        DWORD dwDisplayWidth;
        DWORD dwDisplayHeight;
        DWORD dwAdapterIdx;
    } Config;

    Config LoadConfig(void);
}

#endif // _MADMAX_CRASHFIX_CONFIG_HPP