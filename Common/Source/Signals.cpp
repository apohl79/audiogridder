/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Signals.hpp"
#include "Utils.hpp"

#ifdef JUCE_WINDOWS
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#else
#include <execinfo.h>
#endif

#include <signal.h>

namespace e47 {
namespace Signals {

setLogTagStatic("signals");

typedef void (*sigAction)(int);
sigAction l_orgAbrtAction = nullptr;
sigAction l_orgSegvAction = nullptr;
sigAction l_orgFpeAction = nullptr;

void signalHandler(int signum) {
    traceScope();
    sigAction orgAction = nullptr;
    switch (signum) {
        case SIGABRT:
            traceln("SIGABRT");
            orgAction = l_orgAbrtAction;
            break;
        case SIGSEGV:
            traceln("SIGSEGV");
            orgAction = l_orgSegvAction;
            break;
        case SIGFPE:
            traceln("SIGFPE");
            orgAction = l_orgFpeAction;
            break;
        default:
            traceln("signum=" << signum);
            return;
    }
#ifdef JUCE_WINDOWS
    RaiseException(0, 0, 0, NULL);
#else
    void* callstack[128];
    int frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    for (int i = 0; i < frames; ++i) {
        traceln(strs[i]);
    }
    free(strs);
#endif
}

void initialize() {
    l_orgAbrtAction = signal(SIGABRT, signalHandler);
    l_orgFpeAction = signal(SIGFPE, signalHandler);
#ifdef JUCE_WINDOWS
    l_orgSegvAction = signal(SIGSEGV, signalHandler);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}

}  // namespace Signals
}  // namespace e47
