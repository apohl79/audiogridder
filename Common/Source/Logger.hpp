/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Logger_hpp
#define Logger_hpp

#include <JuceHeader.h>
#include <iostream>
#include <fstream>

namespace e47 {

class Logger : public Thread {
  public:
    Logger(const String& appName, const String& filePrefix, bool linkLatest = true);
    ~Logger() override;
    void run() override;

    static void log(String msg);

    static void initialize(const String& appName, const String& filePrefix, const String& configFile,
                           bool linkLatest = true, bool logDirectly = false);

    static void initialize() {
        initialize({}, {}, {}, true, true);
        setLogToErr(true);
    }

    static void deleteFileAtFinish();
    static std::shared_ptr<Logger> getInstance();
    static void cleanup();

    static bool isEnabled() { return m_enabled; }
    static void setEnabled(bool b);
    static File getLogFile();
    static void setLogToErr(bool b);
    static void setLogDirectly(bool b);

  private:
    File m_file;
    std::ofstream m_outstream;
    bool m_deleteFile = false;
    bool m_logDirectly = false;
    std::queue<String> m_msgQ[2];
    size_t m_msgQIdx = 0;
    std::mutex m_mtx;
    std::condition_variable m_cv;

    bool m_logToErr = false;
    bool m_debugger = false;

    void logToQueue(String msg);
    void logMsg(const String& msg);

    static std::shared_ptr<Logger> m_inst;
    static std::mutex m_instMtx;
    static size_t m_instRefCount;

    static std::atomic_bool m_enabled;
};

class LogTag {
  public:
    LogTag(const String& name) : m_tagId((uint64)this), m_tagName(name) {}
    LogTag(const LogTag& other) : m_tagId(other.m_tagId), m_tagName(other.m_tagName), m_tagExtra(other.m_tagExtra) {}
    virtual ~LogTag() {}

    LogTag& operator=(const LogTag& rhs) {
        if (this != &rhs) {
            m_tagId = rhs.m_tagId;
            m_tagName = rhs.m_tagName;
            m_tagExtra = rhs.m_tagExtra;
        }
        return *this;
    }

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

    void setLogTagExtra(const String& s) { m_tagExtra = s; }
    void setLogTagName(const String& s) { m_tagName = s; }
    const String& getLogTagName() const { return m_tagName; }
    const String& getLogTagExtra() const { return m_tagExtra; }
    uint64 getTagId() const { return m_tagId; }

    const LogTag* getLogTagSource() const { return this; }
    String getLogTag() const {
        return m_tagId == 0 ? "" : getTaggedStr(m_tagName, String::toHexString(m_tagId), m_tagExtra, true);
    }
    String getLogTagNoTime() const {
        return m_tagId == 0 ? "" : getTaggedStr(m_tagName, String::toHexString(m_tagId), m_tagExtra, false);
    }

  protected:
    uint64 m_tagId = 0;
    String m_tagName;
    String m_tagExtra;
};

class LogTagDelegate : public LogTag {
  public:
    LogTagDelegate(const LogTag* src = nullptr) : LogTag("unset") { setLogTagSource(src); }
    void setLogTagSource(const LogTag* src) {
        if (nullptr != src) {
            m_tagId = src->getTagId();
            m_tagName = src->getLogTagName();
            m_tagExtra = src->getLogTagExtra();
        }
    }
};

}  // namespace e47

#endif /* Logger_hpp */
