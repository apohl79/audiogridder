/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef MiniDump_hpp
#define MiniDump_hpp

#ifdef JUCE_WINDOWS

#include <windows.h>

namespace e47 {
namespace MiniDump {

void initialize(const WCHAR* path, const WCHAR* appName, const WCHAR* fileName, const WCHAR* version, bool showMessage);

LONG WINAPI dump(EXCEPTION_POINTERS* pExceptionPointers);

}  // namespace MiniDump
}  // namespace e47

#endif

#endif /* MiniDump_hpp */
