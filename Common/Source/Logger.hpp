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

class AGLogger : public Thread {
  public:
    AGLogger(const String& appName, const String& filePrefix);
    ~AGLogger() override;
    void run() override;

    static void log(String msg);

    static void initialize(const String& appName, const String& filePrefix, const String& configFile);
    static void deleteFileAtFinish();
    static std::shared_ptr<AGLogger> getInstance();
    static void cleanup();

    static bool isEnabled() { return m_enabled; }
    static void setEnabled(bool b);
    static File getLogFile();

  private:
    File m_file;
    std::ofstream m_outstream;
    bool m_deleteFile = false;
    std::queue<String> m_msgQ[2];
    size_t m_msgQIdx = 0;
    std::mutex m_mtx;
    std::condition_variable m_cv;

#ifdef JUCE_DEBUG
    bool m_logToErr = false;
#endif

    void logReal(String msg);

    static std::shared_ptr<AGLogger> m_inst;
    static std::mutex m_instMtx;
    static size_t m_instRefCount;

    static std::atomic_bool m_enabled;
};

class LogTag {
  public:
    LogTag(const String& name) : m_tagId((uint64)this), m_tagName(name) {}
    virtual ~LogTag() {}

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
    uint64 getId() const { return m_tagId; }

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
            m_tagId = src->getId();
            m_tagName = src->getLogTagName();
            m_tagExtra = src->getLogTagExtra();
        }
    }
};

}  // namespace e47

#endif /* Logger_hpp */
