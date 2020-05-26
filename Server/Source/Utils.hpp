/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Utils_hpp
#define Utils_hpp

#include "../JuceLibraryCode/JuceHeader.h"
#include "App.hpp"

#if (JUCE_DEBUG && !JUCE_DISABLE_ASSERTIONS)
#define dbgln(M)                                                                                \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)this << "] " << M; \
                                     Logger::writeToLog(__str);)
#else
#define dbgln(M)
#endif

#define logln(M)                                                                                \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << (uint64_t)this << "] " << M; \
                                     Logger::writeToLog(__str);)
#define logln_static(M) \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[static] " << M; Logger::writeToLog(__str);)

namespace e47 {

static inline App& getApp() { return *dynamic_cast<App*>(JUCEApplication::getInstance()); }

}  // namespace e47

#endif /* Utils_hpp */
