/*
 * Copyright (c) 2022 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Defaults.hpp"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace e47 {
namespace Defaults {

bool unixDomainSocketsSupported() noexcept {
#ifdef JUCE_WINDOWS
    if (auto* hndl = GetModuleHandleW(L"ntdll.dll")) {
        using RtlGetVersionPtr = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        if (auto* rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hndl, "RtlGetVersion")) {
            RTL_OSVERSIONINFOW osInfo = {};
            osInfo.dwOSVersionInfoSize = sizeof(osInfo);
            if (rtlGetVersion(&osInfo) == 0) {
                return osInfo.dwBuildNumber >= 17134;
            }
        }
    }
    return false;
#else
    return true;
#endif
}

}  // namespace Defaults
}  // namespace e47
