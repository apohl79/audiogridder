/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifdef JUCE_WINDOWS

#include "MiniDump.hpp"

#include <dbghelp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>

#include <atomic>

namespace e47 {
namespace MiniDump {

constexpr size_t MAX_NAME = 128;
constexpr size_t MAX_VERSION = 32;

std::atomic_bool l_initialized{false};

WCHAR l_path[MAX_PATH];
WCHAR l_appName[MAX_NAME];
WCHAR l_fileName[MAX_NAME];
WCHAR l_version[MAX_VERSION];
bool l_showMessage;

void initialize(const WCHAR* path, const WCHAR* appName, const WCHAR* fileName, const WCHAR* version,
                bool showMessage) {
    if (!l_initialized.exchange(true)) {
        StringCchCopyW(l_path, MAX_PATH, path);
        StringCchCopyW(l_appName, MAX_NAME, appName);
        StringCchCopyW(l_fileName, MAX_NAME, fileName);
        StringCchCopyW(l_version, MAX_VERSION, version);
        l_showMessage = showMessage;
        SetUnhandledExceptionFilter(dump);
    }
}

LONG dump(EXCEPTION_POINTERS* pExceptionPointers) {
    SYSTEMTIME time;
    GetLocalTime(&time);

    WCHAR fileName[MAX_PATH];

    // create direcory
    StringCchPrintfW(fileName, MAX_PATH, L"%s\\%s", l_path, l_appName);
    CreateDirectoryW(fileName, NULL);

    // create dump
    StringCchPrintfW(fileName, MAX_PATH, L"%s\\%s\\%s%s_%04d-%02d-%02d_%02d-%02d-%02d_%ld_%ld.dmp", l_path, l_appName,
                     l_fileName, l_version, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
                     GetCurrentProcessId(), GetCurrentThreadId());
    HANDLE hDumpFile =
        CreateFileW(fileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    MINIDUMP_EXCEPTION_INFORMATION exceptInfo;
    exceptInfo.ThreadId = GetCurrentThreadId();
    exceptInfo.ExceptionPointers = pExceptionPointers;
    exceptInfo.ClientPointers = TRUE;
    BOOL success = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithDataSegs,
                                     &exceptInfo, NULL, NULL);

    if (success && l_showMessage) {
        WCHAR info[MAX_PATH + 128];
        StringCchPrintfW(info, MAX_PATH + 128,
                         L"%s crashed! A minidump has been created at '%s'. Please submit a bug report including this "
                         L"file togther with the logfiles! Thanks!",
                         l_appName, fileName);
        MessageBoxW(NULL, info, l_appName, MB_OK | MB_ICONERROR);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace MiniDump
}  // namespace e47

#endif
