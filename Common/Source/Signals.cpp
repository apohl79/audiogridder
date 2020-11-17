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
    signal(signum, orgAction);
    raise(signum);
#endif
}

void initialize() {
    l_orgAbrtAction = signal(SIGABRT, signalHandler);
    l_orgSegvAction = signal(SIGSEGV, signalHandler);
    l_orgFpeAction = signal(SIGFPE, signalHandler);
#ifndef JUCE_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif
}

}  // namespace Signals
}  // namespace e47
