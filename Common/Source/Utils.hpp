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
#endif

#include "Logger.hpp"

#define logln(M)                                                                          \
    do {                                                                                  \
        String __msg, __str;                                                              \
        __msg << M;                                                                       \
        __str << "[" << getLogTagSource()->getLogTag() << "] " << __msg;                  \
        AGLogger::log(__str);                                                             \
        if (Tracer::isEnabled()) {                                                        \
            Tracer::traceMessage(getLogTagSource(), __FILE__, __LINE__, __func__, __msg); \
        }                                                                                 \
    } while (0)

#define loglnNoTrace(M)                                                  \
    do {                                                                 \
        String __msg, __str;                                             \
        __msg << M;                                                      \
        __str << "[" << getLogTagSource()->getLogTag() << "] " << __msg; \
        AGLogger::log(__str);                                            \
    } while (0)

#define setLogTagStatic(t)  \
    static LogTag __tag(t); \
    auto getLogTagSource = [] { return &__tag; }

#define traceln(M)                                                                        \
    do {                                                                                  \
        if (Tracer::isEnabled() && nullptr != getLogTagSource()) {                        \
            String __msg;                                                                 \
            __msg << M;                                                                   \
            Tracer::traceMessage(getLogTagSource(), __FILE__, __LINE__, __func__, __msg); \
        }                                                                                 \
    } while (0)

namespace e47 {

class LogTag {
  public:
    LogTag(const String& name) : m_name(name) {}

    static inline String getStrWithLeadingZero(int n, int digits = 2) {
        String s = "";
        while (--digits > 0) {
            if (n < pow(10, digits)) {
                s << "0";
            }
        }
        s << n;
        return s;
    }

    static inline String getTimeStr() {
        auto now = Time::getCurrentTime();
        auto H = getStrWithLeadingZero(now.getHours());
        auto M = getStrWithLeadingZero(now.getMinutes());
        auto S = getStrWithLeadingZero(now.getSeconds());
        auto m = getStrWithLeadingZero(now.getMilliseconds(), 3);
        String ret = "";
        ret << H << ":" << M << ":" << S << "." << m;
        return ret;
    }

    static inline String getTaggedStr(const String& name, const String& ptr, const String& extra, bool withTime) {
        String tag = "";
        if (withTime) {
            tag << getTimeStr() << "|";
        }
        tag << name << "|" << ptr;
        if (extra.isNotEmpty()) {
            tag << "|" << extra;
        }
        return tag;
    }

    void setLogTagExtra(const String& s) { m_extra = s; }

    const LogTag* getLogTagSource() const { return this; }
    String getLogTag() const { return getTaggedStr(m_name, String::toHexString((uint64)this), m_extra, true); }
    String getLogTagNoTime() const { return getTaggedStr(m_name, String::toHexString((uint64)this), m_extra, false); }

  private:
    String m_name;
    String m_extra;
};

class LogTagDelegate {
  public:
    LogTagDelegate() {}
    LogTagDelegate(const LogTag* r) : m_logTagSrc(r) {}
    const LogTag* m_logTagSrc;
    void setLogTagSource(const LogTag* r) { m_logTagSrc = r; }
    const LogTag* getLogTagSource() const {
        if (nullptr != m_logTagSrc) {
            return m_logTagSrc;
        }
        // make sure there is always a tag source
        static auto fallbackTag = std::make_unique<LogTag>("unset");
        return fallbackTag.get();
    }
    String getLogTag() const {
        if (nullptr != m_logTagSrc) {
            return m_logTagSrc->getLogTag();
        }
        return "";
    }
};

static inline void waitForThreadAndLog(const LogTag* tag, Thread* t, int millisUntilWarning = 3000) {
    auto getLogTagSource = [tag] { return tag; };
    if (millisUntilWarning > -1) {
        auto warnTime = Time::getMillisecondCounter() + (uint32)millisUntilWarning;
        while (!t->waitForThreadToExit(1000)) {
            if (Time::getMillisecondCounter() > warnTime) {
                loglnNoTrace("warning: waiting for thread " << t->getThreadName() << " to finish");
            }
        }
    } else {
        t->waitForThreadToExit(-1);
    }
}

class ServerInfo {
  public:
    ServerInfo() {
        m_id = 0;
        m_load = 0.0f;
        refresh();
    }

    ServerInfo(const String& s) {
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
        m_load = 0.0f;
        refresh();
    }

    ServerInfo(const String& host, const String& name, int id, float load)
        : m_host(host), m_name(name), m_id(id), m_load(load) {
        refresh();
    }

    ServerInfo(const ServerInfo& other)
        : m_host(other.m_host), m_name(other.m_name), m_id(other.m_id), m_load(other.m_load) {
        refresh();
    }

    bool operator==(const ServerInfo& other) const {
        return m_host == other.m_host && m_name == other.m_name && m_id == other.m_id;
    }

    const String& getHost() const { return m_host; }
    const String& getName() const { return m_name; }
    int getID() const { return m_id; }
    float getLoad() const { return m_load; }

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
        ret << "id=" << m_id;
        if (m_load > 0.0f) {
            ret << ", load=" << m_load;
        }
        ret << ")";
        return ret;
    }

    String serialize() const {
        String ret = m_host;
        ret << ":" << m_id << ":" << m_name;
        return ret;
    }

    Time getUpdated() const { return m_updated; }

    void refresh() { m_updated = Time::getCurrentTime(); }

    void refresh(float load) {
        refresh();
        m_load = load;
    }

  private:
    String m_host, m_name;
    int m_id;
    float m_load;
    Time m_updated;
};

inline void callOnMessageThread(std::function<void()> fn) {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    MessageManager::callAsync([&] {
        std::lock_guard<std::mutex> lock(mtx);
        fn();
        done = true;
        cv.notify_one();
    });
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&done] { return done; });
}

}  // namespace e47

#include "Tracer.hpp"

#endif /* Utils_hpp */
