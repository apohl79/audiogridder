/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Sentry.hpp"

#include "Sentry.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"

#ifndef AG_SENTRY_ENABLED
#define AG_SENTRY_ENABLED 0
#endif

#if AG_SENTRY_ENABLED
#define SENTRY_BUILD_STATIC
#include <sentry.h>
#endif

namespace e47 {
namespace Sentry {

#if AG_SENTRY_ENABLED
static std::atomic_bool l_sentryEnabled{true};
static std::atomic_bool l_sentryInitialized{false};
#endif

void initialize() {
#if AG_SENTRY_ENABLED
    auto crashpadPath = Defaults::getSentryCrashpadPath();
    setLogTagStatic("sentry");
    if (l_sentryEnabled && crashpadPath.isNotEmpty() && !l_sentryInitialized.exchange(true)) {
        logln("initializing crash reporting...");
        sentry_options_t* options = sentry_options_new();
        sentry_options_set_dsn(options, AG_SENTRY_DSN);
        sentry_options_set_handler_path(options, crashpadPath.toRawUTF8());
        sentry_options_set_database_path(options, Defaults::getSentryDbPath().toRawUTF8());

        if (AGLogger::isEnabled()) {
            auto logfile = AGLogger::getLogFile().getFullPathName();
            if (logfile.isNotEmpty()) {
                logln("  attaching logfile: " << AGLogger::getLogFile().getFileName());
                sentry_options_add_attachment(options, logfile.toRawUTF8());
            }
        }

        if (Tracer::isEnabled()) {
            auto tracefile = Tracer::getTraceFile().getFullPathName();
            if (tracefile.isNotEmpty()) {
                logln("  attaching tracefile: " << Tracer::getTraceFile().getFileName());
                sentry_options_add_attachment(options, tracefile.toRawUTF8());
            }
        }

        sentry_init(options);
    }
#endif
}

void cleanup() {
#if AG_SENTRY_ENABLED
    if (l_sentryInitialized.exchange(false)) {
        sentry_close();
    }
#endif
}

void setEnabled(bool b) {
#if AG_SENTRY_ENABLED
    l_sentryEnabled = b;
#else
    ignoreUnused(b);
#endif
}

bool isEnabled() {
#if AG_SENTRY_ENABLED
    return l_sentryEnabled;
#else
    return false;
#endif
}

}  // namespace Sentry
}  // namespace e47
