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

    String getStrWithLeadingZero(int n, int digits = 2) const {
        String s = "";
        while (--digits > 0) {
            if (n < pow(10, digits)) {
                s << "0";
            }
        }
        s << n;
        return s;
    }

    String getLogTag() const {
        auto now = Time::getCurrentTime();
        auto H = getStrWithLeadingZero(now.getHours());
        auto M = getStrWithLeadingZero(now.getMinutes());
        auto S = getStrWithLeadingZero(now.getSeconds());
        auto m = getStrWithLeadingZero(now.getMilliseconds(), 3);
        String tag = "";
        tag << H << ":" << M << ":" << S << "." << m << "|" << m_name << "|" << (uint64)this;
        return tag;
    }

  private:
    String m_name;
};

class LogTagDelegate {
  public:
    LogTagDelegate() {}
    LogTagDelegate(LogTag* r) : m_logTagSrc(r) {}
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

class ServerString {
  public:
    ServerString() {}

    ServerString(const String& s) {
        auto hostParts = StringArray::fromTokens(s, ":", "");
        if (hostParts.size() > 1) {
            m_host = hostParts[0];
            m_id = hostParts[1].getIntValue();
            if (hostParts.size() > 2) {
                m_name = hostParts[2];
            }
        } else {
            m_host = s;
            m_id = 0;
        }
    }

    ServerString(const String& host, const String& name, int id) : m_host(host), m_name(name), m_id(id) {}

    ServerString(const ServerString& other) : m_host(other.m_host), m_name(other.m_name), m_id(other.m_id) {}

    bool operator==(const ServerString& other) const {
        return m_host == other.m_host && m_name == other.m_name && m_id == other.m_id;
    }

    const String& getHost() const { return m_host; }
    const String& getName() const { return m_name; }
    int getID() const { return m_id; }

    String getHostAndID() const {
        String ret = m_host;
        if (m_id > 0) {
            ret << ":" << m_id;
        }
        return ret;
    }

    String getNameAndID() const {
        String ret = m_name;
        if (m_id > 0) {
            ret << ":" << m_id;
        }
        return ret;
    }

    String toString() const {
        String ret = "Server(";
        ret << "name=" << m_name << ", ";
        ret << "host=" << m_host << ", ";
        ret << "id=" << m_id << ")";
        return ret;
    }

    String serialize() const {
        String ret = m_host;
        ret << ":" << m_id << ":" << m_name;
        return ret;
    }

  private:
    String m_host, m_name;
    int m_id;
};

}  // namespace e47

#endif /* Utils_hpp */
