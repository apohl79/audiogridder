/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include <JuceHeader.h>

#include "Utils.hpp"

#ifdef JUCE_WINDOWS
#include <windows.h>
#endif

namespace e47 {

String getLastErrorStr() {
#ifdef JUCE_WINDOWS
    DWORD err = GetLastError();
    LPSTR lpMsgBuf;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                  err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
    return String(lpMsgBuf);
#else
    std::vector<char> buf(512);
    ignoreUnused(strerror_r(errno, buf.data(), buf.size()));
    return String(buf.data());
#endif
}

void windowToFront(Component* c) {
    setLogTagStatic("utils");
    traceScope();
    if (nullptr != c && !c->isAlwaysOnTop()) {
        c->setAlwaysOnTop(true);
        c->toFront(true);
        c->setAlwaysOnTop(false);
    }
}

}  // namespace e47
