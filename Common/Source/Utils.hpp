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

#define setLogTagByRef(t) auto getLogTagSource = [&] { return &t; }

#define traceln(M)                                                                        \
    do {                                                                                  \
        if (Tracer::isEnabled()) {                                                        \
            String __msg;                                                                 \
            __msg << M;                                                                   \
            Tracer::traceMessage(getLogTagSource(), __FILE__, __LINE__, __func__, __msg); \
        }                                                                                 \
    } while (0)

#define _createUniqueVar_(P, S) P##S
#define _createUniqueVar(P, S) _createUniqueVar_(P, S)
#define traceScope() Tracer::Scope _createUniqueVar(__scope, __LINE__)(getLogTagSource(), __FILE__, __LINE__, __func__)

#define printBusesLayout(l)                                                             \
    do {                                                                                \
        logln("input buses: " << l.inputBuses.size());                                  \
        for (int i = 0; i < l.inputBuses.size(); i++) {                                 \
            logln("  [" << i << "] " << l.inputBuses[i].size() << " channel(s)");       \
            for (auto ct : l.inputBuses[i].getChannelTypes()) {                         \
                logln("    <- " << AudioChannelSet::getAbbreviatedChannelTypeName(ct)); \
            }                                                                           \
        }                                                                               \
        logln("output buses: " << l.outputBuses.size());                                \
        for (int i = 0; i < l.outputBuses.size(); i++) {                                \
            logln("  [" << i << "] " << l.outputBuses[i].size() << " channel(s)");      \
            for (auto ct : l.outputBuses[i].getChannelTypes()) {                        \
                logln("    -> " << AudioChannelSet::getAbbreviatedChannelTypeName(ct)); \
            }                                                                           \
        }                                                                               \
    } while (0)

namespace e47 {

String getLastErrorStr();

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
            if (hostParts.size() > 6) {
                m_uuid = hostParts[6];
            }
        } else {
            m_host = s;
            m_id = 0;
        }
        m_load = 0.0f;
        refresh();
    }

    ServerInfo(const String& host, const String& name, bool ipv6, int id, Uuid uuid, float load, bool localMode,
               const String& version = "")
        : m_host(host),
          m_name(name),
          m_isIpv6(ipv6),
          m_id(id),
          m_uuid(uuid),
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
          m_uuid(other.m_uuid),
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
        m_uuid = other.m_uuid;
        m_load = other.m_load;
        m_localMode = other.m_localMode;
        m_version = other.m_version;
        refresh();
        return *this;
    }

    bool operator==(const ServerInfo& other) const {
        return m_host == other.m_host && m_name == other.m_name && m_id == other.m_id && m_uuid == other.m_uuid &&
               m_localMode == other.m_localMode && m_version == other.m_version;
    }

    bool operator!=(const ServerInfo& other) const { return !operator==(other); }

    bool matches(const ServerInfo& other) const {
        if (m_uuid != Uuid::null() && other.m_uuid != Uuid::null()) {
            return m_uuid == other.m_uuid;
        }
        return getNameAndID() == other.getNameAndID();
    }

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
    Uuid getUUID() const { return m_uuid; }
    void setUUID(Uuid uuid) { m_uuid = uuid; }
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
        ret << "uuid=" << m_uuid.toDashedString() << ", ";
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
        ret << ":" << m_id << ":" << m_name << ":" << m_version << ":" << (int)m_isIpv6 << ":" << (int)m_localMode
            << ":" << m_uuid.toDashedString();
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
    Uuid m_uuid = Uuid::null();
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
                                                                                                             \
    inline void runOnMsgThreadAsync(std::function<void()> fn) { MessageManager::callAsync(safeLambda(fn)); } \
                                                                                                             \
    struct __AsyncContext {                                                                                  \
        std::shared_ptr<std::atomic_bool> shouldExec;                                                        \
        std::shared_ptr<std::atomic_uint32_t> execCnt;                                                       \
                                                                                                             \
        void execute(std::function<void()> fn) {                                                             \
            if (shouldExec->load()) {                                                                        \
                execCnt->fetch_add(1, std::memory_order_relaxed);                                            \
                fn();                                                                                        \
                execCnt->fetch_sub(1, std::memory_order_relaxed);                                            \
            }                                                                                                \
        }                                                                                                    \
    };                                                                                                       \
                                                                                                             \
    inline __AsyncContext getAsyncContext() {                                                                \
        jassert(nullptr == __m_asyncExecFlag);                                                               \
        return {__m_asyncExecFlag, __m_asyncExecCnt};                                                        \
    }                                                                                                        \
                                                                                                             \
    std::shared_ptr<std::atomic_bool> __m_asyncExecFlag;                                                     \
    std::shared_ptr<std::atomic_uint32_t> __m_asyncExecCnt

#define initAsyncFunctors()                                           \
    do {                                                              \
        __m_asyncExecFlag = std::make_shared<std::atomic_bool>(true); \
        __m_asyncExecCnt = std::make_shared<std::atomic_uint32_t>(0); \
    } while (0)

#define stopAsyncFunctors()                                                               \
    do {                                                                                  \
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

#define sleepExitAware(t)                                                    \
    do {                                                                     \
        int __sleepstep = 50;                                                \
        if (t < __sleepstep) {                                               \
            Thread::sleep(t);                                                \
        } else {                                                             \
            int __sleepfor = t / __sleepstep;                                \
            while (!Thread::currentThreadShouldExit() && __sleepfor-- > 0) { \
                Thread::sleep(__sleepstep);                                  \
            }                                                                \
        }                                                                    \
    } while (0)

#define sleepExitAwareWithCondition(t, c)                                            \
    do {                                                                             \
        int __sleepstep = 50;                                                        \
        if (t < __sleepstep) {                                                       \
            Thread::sleep(t);                                                        \
        } else {                                                                     \
            int __sleepfor = t / __sleepstep;                                        \
            while (!Thread::currentThreadShouldExit() && !c() && __sleepfor-- > 0) { \
                Thread::sleep(__sleepstep);                                          \
            }                                                                        \
        }                                                                            \
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
    auto mtx = std::make_shared<std::mutex>();
    auto cv = std::make_shared<std::condition_variable>();
    auto done = std::make_shared<bool>(false);
    MessageManager::callAsync([mtx, cv, done, fn] {
        std::lock_guard<std::mutex> lock(*mtx);
        if (!*done) {
            fn();
            *done = true;
            cv->notify_one();
        }
    });
    bool finished = false;
    do {
        std::unique_lock<std::mutex> lock(*mtx);
        finished = cv->wait_for(lock, 5ms, [&done, &mm] {
            if (mm->hasStopMessageBeenSent()) {
                // make sure we don't call the functor anymore
                *done = true;
            }
            return *done;
        });
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

inline json jsonReadFile(const String& filename, bool binary, String* err = nullptr) {
    setLogTagStatic("utils");
    auto setErr = [&](const String& s) {
        if (nullptr != err) {
            *err = s;
        }
    };
    File file(filename);
    if (file.existsAsFile() && file.getSize() > 0) {
        FileInputStream fis(file);
        if (fis.openedOk()) {
            try {
                if (binary) {
                    std::vector<uint8> data((size_t)fis.getTotalLength());
                    fis.read(data.data(), (int)data.size());
                    return json::from_msgpack(data);
                } else {
                    return json::parse(fis.readEntireStreamAsString().toStdString());
                }
            } catch (json::parse_error& e) {
                logln("parsing json file " << filename << " failed: " << e.what());
                setErr(e.what());
            }
        } else {
            logln("failed to open json file " << filename << ": " << fis.getStatus().getErrorMessage());
            setErr(fis.getStatus().getErrorMessage());
        }
    } else {
        setErr("file does not exists");
    }
    return {};
}

inline void jsonWriteFile(const String& filename, const json& j, bool binary) {
    File file(filename);
    if (file.exists()) {
        file.deleteFile();
    } else {
        file.create();
    }
    try {
        FileOutputStream fos(file);
        if (binary) {
            std::vector<uint8> data;
            if (!j.empty()) {
                json::to_msgpack(j, data);
            }
            fos.write(data.data(), data.size());
        } else {
            fos.writeText(j.dump(4), false, false, "\n");
        }
    } catch (const json::exception& e) {
        setLogTagStatic("utils");
        logln("failed to write json file " << filename << ": " << e.what());
    }
}

inline json configParseFile(const String& configFile, String* err = nullptr) {
    return jsonReadFile(configFile, false, err);
}

inline void configWriteFile(const String& configFile, const json& j) { jsonWriteFile(configFile, j, false); }

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

#if JUCE_MODULE_AVAILABLE_juce_audio_processors
inline String describeLayout(const AudioProcessor::BusesLayout& l, bool withInputs = true, bool withOutputs = true,
                             bool shortFormat = false) {
    String sout;
    StringArray sbuses;

    int count = 1;
    String last;

    auto addLast = [&] {
        if (count > 1) {
            sbuses.add(String(count) + "x" + last);
        } else {
            sbuses.add(last);
        }
        count = 1;
    };

    auto addBuses = [&](const Array<AudioChannelSet>& buses, bool twoBusesAsSC = false) {
        if (buses.isEmpty()) {
            sout << "-";
        } else {
            sbuses.clearQuick();
            last.clear();
            count = 1;
            for (int i = 0; i < buses.size(); i++) {
                auto sbus = buses[i].getDescription().replace(" Surround", "");
                if (sbus.startsWith("Discrete #")) {
                    sbus = sbus.substring(10) + "ch";
                }
                if (last == sbus) {
                    count++;
                } else if (last.isNotEmpty()) {
                    addLast();
                }
                last = sbus;
            }
            addLast();
            if (sbuses.size() == 2 && twoBusesAsSC) {
                sout << sbuses.joinIntoString(",") << " (Sidechain)";
            } else {
                sout << sbuses.joinIntoString(",");
            }
        }
    };

    if (withInputs) {
        if (!shortFormat) {
            sout << (shortFormat ? "" : "Inputs: ");
        }
        addBuses(l.inputBuses, true);
    }

    if (withOutputs) {
        if (withInputs) {
            sout << (shortFormat ? " : " : " / Outputs: ");
        }
        addBuses(l.outputBuses);
    }

    return sout;
}

inline json audioChannelSetsToJson(const Array<AudioChannelSet>& a) {
    json j = json::array();
    for (auto& bus : a) {
        j.push_back(bus.getSpeakerArrangementAsString().toStdString());
    }
    return j;
}

inline String serializeChannelSets(const Array<AudioChannelSet>& a) { return audioChannelSetsToJson(a).dump(); }

inline String serializeLayout(const AudioProcessor::BusesLayout& l, bool withInputs = true, bool withOutputs = true) {
    json j = json::object();
    if (withInputs) {
        j["inputBuses"] = audioChannelSetsToJson(l.inputBuses);
    }
    if (withOutputs) {
        j["outputBuses"] = audioChannelSetsToJson(l.outputBuses);
    }
    return j.dump();
}

inline AudioProcessor::BusesLayout deserializeLayout(const String& s) {
    AudioProcessor::BusesLayout ret;
    try {
        json j = json::parse(s.toStdString());

        for (auto& jbus : j["inputBuses"]) {
            ret.inputBuses.add(AudioChannelSet::fromAbbreviatedString(jbus.get<std::string>()));
        }
        for (auto& jbus : j["outputBuses"]) {
            ret.outputBuses.add(AudioChannelSet::fromAbbreviatedString(jbus.get<std::string>()));
        }
    } catch (const json::parse_error& e) {
        setLogTagStatic("utils");
        logln("failed to deserialize layout: " << e.what());
    }
    return ret;
}

inline int getLayoutNumChannels(const AudioProcessor::BusesLayout& l, bool isInput) {
    int num = 0;
    if (isInput) {
        for (auto& bus : l.inputBuses) {
            num += bus.size();
        }
    } else {
        for (auto& bus : l.outputBuses) {
            num += bus.size();
        }
    }
    return num;
}
#endif

inline String getPluginType(const String& id, const PluginDescription* pdesc) {
    String type;
    if (pdesc != nullptr) {
        if (pdesc->pluginFormatName == "AudioUnit") {
            type = "au";
        } else {
            type = pdesc->pluginFormatName.toLowerCase();
        }
    } else {
        File f(id);
        if (f.exists()) {
            if (f.getFileExtension().toLowerCase() == ".dll") {
                type = "vst";
            } else {
                type = f.getFileExtension().toLowerCase().substring(1);
            }
        } else if (id.startsWith("AudioUnit")) {
            type = "au";
        } else {
            type = "lv2";
        }
    }
    return type;
}

inline String getPluginName(const String& id, const PluginDescription* pdesc, bool withType = true) {
    String name;
    File f(id);
    if (pdesc != nullptr) {
        name = pdesc->name;
    } else if (f.exists()) {
        name = f.getFileNameWithoutExtension();
#if JUCE_MAC && AG_SERVER
    } else if (id.startsWith("AudioUnit")) {
        AudioUnitPluginFormat fmt;
        name = fmt.getNameOfPluginFromIdentifier(id);
#endif
    } else {
        name = id;
    }
    if (withType) {
        name << " (" << getPluginType(id, pdesc) << ")";
    }
    return name;
}

struct FnThread : Thread {
    std::function<void()> fn;

    FnThread(std::function<void()> f = nullptr, const String& n = "FnThread", bool autoStart = false)
        : Thread(n), fn(f) {
        if (autoStart) {
            startThread();
        }
    }

    virtual ~FnThread() override {
        if (isThreadRunning()) {
            stopThread(-1);
        }
    }

    void run() override {
        if (fn != nullptr) {
            fn();
            fn = nullptr;
        }
    }
};

struct FnTimer : Timer {
    std::function<void()> fn;
    bool oneTime = true;

    FnTimer(std::function<void()> f = nullptr, int intervalMs = 0, bool oneTime_ = true, bool autoStart = true)
        : fn(f), oneTime(oneTime_) {
        if (autoStart) {
            startTimer(intervalMs);
        }
    }

    virtual ~FnTimer() override {
        stopTimer();
        fn = nullptr;
    }

    void timerCallback() override {
        if (nullptr != fn) {
            fn();
        }
        if (oneTime) {
            stopTimer();
            fn = nullptr;
        }
    }
};

template <typename K, typename V>
class SafeHashMap {
  public:
    bool contains(const K& key) {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_elements.find(key) != m_elements.end();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_elements.clear();
    }

    V& operator[](const K& key) {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_elements[key];
    }

    V operator[](const K& key) const {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_elements[key];
    }

    bool getAndRemove(const K& key, V& val) {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_elements.find(key);
        if (it != m_elements.end()) {
            val = it->second;
            m_elements.erase(it);
            return true;
        }
        return false;
    }

    void erase(const K& key) {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_elements.erase(key);
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_elements.size();
    }

    auto begin() noexcept { return m_elements.begin(); }
    const auto begin() const noexcept { return m_elements.begin(); }
    auto end() noexcept { return m_elements.end(); }
    const auto end() const noexcept { return m_elements.end(); }

  private:
    std::unordered_map<K, V> m_elements;
    mutable std::mutex m_mtx;
};

}  // namespace e47

#endif /* Utils_hpp */
