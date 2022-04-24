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

#ifndef JUCE_WINDOWS
#include <sys/resource.h>
#endif

using namespace std::chrono_literals;

#define logln(M)                                                                          \
    do {                                                                                  \
        String __msg, __str;                                                              \
        __msg << M;                                                                       \
        __str << "[" << getLogTagSource()->getLogTag() << "] " << __msg;                  \
        Logger::log(__str);                                                               \
        if (Tracer::isEnabled()) {                                                        \
            Tracer::traceMessage(getLogTagSource(), __FILE__, __LINE__, __func__, __msg); \
        }                                                                                 \
    } while (0)

#define loglnNoTrace(M)                                                  \
    do {                                                                 \
        String __msg, __str;                                             \
        __msg << M;                                                      \
        __str << "[" << getLogTagSource()->getLogTag() << "] " << __msg; \
        Logger::log(__str);                                              \
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

#define _createUniqueVar_(P, S) P##S
#define _createUniqueVar(P, S) _createUniqueVar_(P, S)
#define traceScope() Tracer::Scope _createUniqueVar(__scope, __LINE__)(getLogTagSource(), __FILE__, __LINE__, __func__)

namespace e47 {

#ifdef JUCE_WINDOWS
String GetLastErrorStr();
#endif

class ServerInfo {
  public:
    ServerInfo() {
        m_id = -1;
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
            if (hostParts.size() > 3) {
                m_version = hostParts[3];
            }
            if (hostParts.size() > 4) {
                m_isIpv6 = hostParts[4] == "1";
            }
            if (hostParts.size() > 5) {
                m_localMode = hostParts[5] == "1";
            }
        } else {
            m_host = s;
            m_id = 0;
        }
        m_load = 0.0f;
        refresh();
    }

    ServerInfo(const String& host, const String& name, bool ipv6, int id, float load, bool localMode,
               const String& version = "")
        : m_host(host),
          m_name(name),
          m_isIpv6(ipv6),
          m_id(id),
          m_load(load),
          m_localMode(localMode),
          m_version(version) {
        refresh();
    }

    ServerInfo(const ServerInfo& other)
        : m_host(other.m_host),
          m_name(other.m_name),
          m_isIpv6(other.m_isIpv6),
          m_id(other.m_id),
          m_load(other.m_load),
          m_localMode(other.m_localMode),
          m_version(other.m_version) {
        refresh();
    }

    ServerInfo& operator=(const ServerInfo& other) {
        m_host = other.m_host;
        m_name = other.m_name;
        m_isIpv6 = other.m_isIpv6;
        m_id = other.m_id;
        m_load = other.m_load;
        m_localMode = other.m_localMode;
        m_version = other.m_version;
        refresh();
        return *this;
    }

    bool operator==(const ServerInfo& other) const {
        return m_host == other.m_host && m_name == other.m_name && m_isIpv6 == other.m_isIpv6 && m_id == other.m_id &&
               m_localMode == other.m_localMode && m_version == other.m_version;
    }

    bool operator!=(const ServerInfo& other) const { return !operator==(other); }

    bool isValid() const { return m_id > -1; }
    const String& getHost() const { return m_host; }
    void setHost(const String& h) { m_host = h; }
    const String& getName() const { return m_name; }
    void setName(const String& n) { m_name = n; }
    bool isIpv6() const { return m_isIpv6; }
    void setIsIpv6(bool b) { m_isIpv6 = b; }
    const String& getVersion() const { return m_version; }
    void setVersion(const String& v) { m_version = v; }
    int getID() const { return m_id; }
    void setID(int id) { m_id = id; }
    float getLoad() const { return m_load; }
    void setLoad(float l) { m_load = l; }
    bool getLocalMode() const { return m_localMode; }
    void setLocalMode(bool b) { m_localMode = b; }

    String getHostAndID() const {
        String ret = m_host;
        if (m_id > 0) {
            ret << ":" << m_id;
        }
        return ret;
    }

    String getNameAndID() const {
        String ret = m_name;
        if (ret.isEmpty()) {
            ret = m_host;
        }
        if (m_id > 0) {
            ret << ":" << m_id;
        }
        return ret;
    }

    String toString() const {
        String ret = "Server(";
        ret << "name=" << m_name << ", ";
        ret << "host=" << m_host << ", ";
        ret << "id=" << m_id << ", ";
        ret << "localmode=" << (int)m_localMode << ", ";
        ret << "version=" << m_version;
        if (m_load > 0.0f) {
            ret << ", load=" << m_load;
        }
        ret << ")";
        return ret;
    }

    String serialize() const {
        String ret = m_host;
        ret << ":" << m_id << ":" << m_name << ":" << m_version << ":" << (int)m_isIpv6 << ":" << (int)m_localMode;
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
    bool m_isIpv6 = false;
    int m_id = -1;
    float m_load = 0.0f;
    bool m_localMode = false;
    String m_version;
    Time m_updated;
};

inline bool msgThreadExistsAndNotLocked() {
    auto mm = MessageManager::getInstanceWithoutCreating();
    return nullptr != mm && !mm->hasStopMessageBeenSent() && !mm->currentThreadHasLockedMessageManager();
}

#define ENABLE_ASYNC_FUNCTORS()                                                                              \
    inline std::function<void()> safeLambda(std::function<void()> fn) {                                      \
        traceScope();                                                                                        \
        if (nullptr == __m_asyncExecFlag) {                                                                  \
            logln("initAsyncFunctors() has to be called in the ctor");                                       \
            return nullptr;                                                                                  \
        }                                                                                                    \
        auto shouldExec = __m_asyncExecFlag;                                                                 \
        auto execCnt = __m_asyncExecCnt;                                                                     \
        return [shouldExec, execCnt, fn] {                                                                   \
            if (shouldExec->load()) {                                                                        \
                execCnt->fetch_add(1, std::memory_order_relaxed);                                            \
                fn();                                                                                        \
                execCnt->fetch_sub(1, std::memory_order_relaxed);                                            \
            }                                                                                                \
        };                                                                                                   \
    }                                                                                                        \
    inline void runOnMsgThreadAsync(std::function<void()> fn) { MessageManager::callAsync(safeLambda(fn)); } \
    std::shared_ptr<std::atomic_bool> __m_asyncExecFlag;                                                     \
    std::shared_ptr<std::atomic_uint32_t> __m_asyncExecCnt

#define initAsyncFunctors()                                           \
    do {                                                              \
        __m_asyncExecFlag = std::make_shared<std::atomic_bool>(true); \
        __m_asyncExecCnt = std::make_shared<std::atomic_uint32_t>(0); \
    } while (0)

#define stopAsyncFunctors()                                                               \
    do {                                                                                  \
        traceScope();                                                                     \
        if (nullptr == __m_asyncExecFlag) {                                               \
            logln("initAsyncFunctors() has to be called in the ctor");                    \
            break;                                                                        \
        }                                                                                 \
        traceln("stop async functors, exec count is " << String(*__m_asyncExecCnt));      \
        *__m_asyncExecFlag = false;                                                       \
        if (msgThreadExistsAndNotLocked()) {                                              \
            runOnMsgThreadSync([] {});                                                    \
            while (__m_asyncExecCnt->load() > 0) {                                        \
                traceln("waiting for async functors, cnt=" << String(*__m_asyncExecCnt)); \
                Thread::sleep(5);                                                         \
            }                                                                             \
        }                                                                                 \
    } while (0)

#define sleepExitAware(t)                                            \
    do {                                                             \
        int __sleepstep = 50;                                        \
        if (t < __sleepstep) {                                       \
            Thread::sleep(t);                                        \
        } else {                                                     \
            int __sleepfor = t / __sleepstep;                        \
            while (!currentThreadShouldExit() && __sleepfor-- > 0) { \
                Thread::sleep(__sleepstep);                          \
            }                                                        \
        }                                                            \
    } while (0)

#define sleepExitAwareWithCondition(t, c)                                    \
    do {                                                                     \
        int __sleepstep = 50;                                                \
        if (t < __sleepstep) {                                               \
            Thread::sleep(t);                                                \
        } else {                                                             \
            int __sleepfor = t / __sleepstep;                                \
            while (!currentThreadShouldExit() && !c() && __sleepfor-- > 0) { \
                Thread::sleep(__sleepstep);                                  \
            }                                                                \
        }                                                                    \
    } while (0)

}  // namespace e47

#include "Tracer.hpp"
#include "json.hpp"

using json = nlohmann::json;

namespace e47 {

inline void runOnMsgThreadSync(std::function<void()> fn) {
    setLogTagStatic("utils");
    auto mm = MessageManager::getInstanceWithoutCreating();
    if (nullptr == mm) {
        logln("error: message thread does not exists");
        return;
    }
    if (mm->isThisTheMessageThread()) {
        fn();
        return;
    }
    if (mm->hasStopMessageBeenSent()) {
        logln("error: dispatch loop has been stopped");
        return;
    }
    if (mm->currentThreadHasLockedMessageManager()) {
        logln("error: current thread has locked the message thread");
        return;
    }
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    MessageManager::callAsync([&] {
        std::lock_guard<std::mutex> lock(mtx);
        fn();
        done = true;
        cv.notify_one();
    });
    bool finished = false;
    do {
        std::unique_lock<std::mutex> lock(mtx);
        finished = cv.wait_for(lock, 5ms, [&done, &mm] { return done || mm->hasStopMessageBeenSent(); });
    } while (!finished);
}

inline void waitForThreadAndLog(const LogTag* tag, Thread* t, int millisUntilWarning = 3000) {
    auto getLogTagSource = [tag] { return tag; };
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

inline json configParseFile(const String& configFile, String* err = nullptr) {
    setLogTagStatic("utils");
    auto setErr = [&](const String& s) {
        if (nullptr != err) {
            *err = s;
        }
    };
    File cfg(configFile);
    if (cfg.exists()) {
        FileInputStream fis(cfg);
        if (fis.openedOk()) {
            try {
                return json::parse(fis.readEntireStreamAsString().toStdString());
            } catch (json::parse_error& e) {
                logln("parsing config file " << configFile << " failed: " << e.what());
                setErr(e.what());
            }
        } else {
            logln("failed to open config file " << configFile << ": " << fis.getStatus().getErrorMessage());
            setErr(fis.getStatus().getErrorMessage());
        }
    } else {
        setErr("file does not exists");
    }
    return {};
}

inline void configWriteFile(const String& configFile, const json& j) {
    File cfg(configFile);
    if (cfg.exists()) {
        cfg.deleteFile();
    } else {
        cfg.create();
    }
    FileOutputStream fos(cfg);
    fos.writeText(j.dump(4), false, false, "\n");
}

inline bool jsonHasValue(const json& j, const String& name) { return j.find(name.toStdString()) != j.end(); }

template <typename T>
inline T jsonGetValue(const json& j, const String& name, const T& def) {
    if (!jsonHasValue(j, name)) {
        return def;
    }
    return j[name.toStdString()].get<T>();
}

template <>
inline String jsonGetValue(const json& cfg, const String& name, const String& def) {
    return jsonGetValue(cfg, name, def.toStdString());
}

void windowToFront(juce::Component* c);

inline void cleanDirectory(const String& path, const String& filePrefix, const String& fileExtension,
                           int filesToKeep = 5) {
    setLogTagStatic("utils");
    File dir(path);
    if (!dir.isDirectory()) {
        return;
    }
    auto files = dir.findChildFiles(File::findFiles, false, filePrefix + "*" + fileExtension);
    if (files.size() > filesToKeep) {
        files.sort();
        for (auto* it = files.begin(); it < files.end() - filesToKeep; it++) {
#ifndef JUCE_WINDOWS
            if (fileExtension == ".log") {
                FileInputStream fis(*it);
                int maxlines = 5;
                while (--maxlines >= 0) {
                    auto line = fis.readNextLine();
                    if (line.contains("matching core file name")) {
                        auto parts = StringArray::fromTokens(line, " ", "");
                        String corepath;
                        for (int i = 5; i < parts.size(); i++) {
                            if (i > 5) {
                                corepath << " ";
                            }
                            corepath << parts[i];
                        }
                        File corefile(corepath);
                        if (corefile.existsAsFile()) {
                            logln("removing old diagnistics file: " << corepath);
                            corefile.deleteFile();
                        }
                        maxlines = 0;
                    }
                }
            }
#endif
            logln("removing old diagnostics file: " << it->getFullPathName());
            it->deleteFile();
        }
    }
}

struct FnThread : Thread {
    std::function<void()> fn;

    FnThread(std::function<void()> f = nullptr, const String& n = "FnThread", bool autoStart = false)
        : Thread(n), fn(f) {
        if (autoStart) {
            startThread();
        }
    }

    virtual ~FnThread() override { stopThread(-1); }

    void run() override {
        if (fn != nullptr) {
            fn();
            fn = nullptr;
        }
    }
};

}  // namespace e47
#endif /* Utils_hpp */
