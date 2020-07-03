/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Utils_hpp
#define Utils_hpp

#ifdef AG_SERVER

#define getApp() dynamic_cast<App*>(JUCEApplication::getInstance())

#if (JUCE_DEBUG && !JUCE_DISABLE_ASSERTIONS)
#define dbgln(M) \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << getLogTag() << "] " << M; Logger::writeToLog(__str);)
#else
#define dbgln(M)
#endif

#define logln(M) \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << getLogTag() << "] " << M; Logger::writeToLog(__str);)

#else

#include "Logger.hpp"

#if JUCE_DEBUG
#define dbgln(M) \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << getLogTag() << "] " << M; AGLogger::log(__str);)
#else
#define dbgln(M)
#endif

#define logln(M) \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String __str; __str << "[" << getLogTag() << "] " << M; AGLogger::log(__str);)

#endif

namespace e47 {

class LogTag {
  public:
    LogTag(const String& name) : m_name(name) {}
    String getLogTag() const {
        String tag = m_name;
        tag << ":" << (uint64)this;
        return tag;
    }

  private:
    String m_name;
};

class LogTagDelegate {
  public:
    LogTag* m_logTagSrc;
    void setLogTagSource(LogTag* r) { m_logTagSrc = r; }
    LogTag* getLogTagSource() const { return m_logTagSrc; }
    String getLogTag() const {
        if (nullptr != m_logTagSrc) {
            return m_logTagSrc->getLogTag();
        }
        return "";
    }
};

static inline void waitForThreadAndLog(LogTag* tag, Thread* t, int millisUntilWarning = 3000) {
    auto getLogTag = [tag] { return tag->getLogTag(); };
    if (millisUntilWarning > -1) {
        auto warnTime = Time::getMillisecondCounter() + (uint32)millisUntilWarning;
        while (!t->waitForThreadToExit(1000)) {
            if (Time::getMillisecondCounter() > warnTime) {
                logln("warning: waiting for thread " << t->getThreadName() << " to finish");
            }
        }
    } else {
        t->waitForThreadToExit(-1);
    }
}

}  // namespace e47

#endif /* Utils_hpp */
