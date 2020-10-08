/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef ThreadTracer_hpp
#define ThreadTracer_hpp

#include <JuceHeader.h>
#include <queue>
#include <unordered_map>
#include <set>
#include <iostream>
#include <fstream>

#include "SharedInstance.hpp"

namespace e47 {

class LogTag;
class LogTagDelegate;

class Tracer : public Thread, public SharedInstance<Tracer> {
  public:
    Tracer();
    ~Tracer();

    struct MessageBuffer {
        std::queue<String> messages[2];
        size_t idx = 0;
        std::mutex mtx;

        inline size_t getAndUpdateIndex() {
            std::lock_guard<std::mutex> lock(mtx);
            auto ret = idx;
            idx = idx == 0 ? 1 : 0;
            return ret;
        }

        inline void push(const String& msg) {
            std::lock_guard<std::mutex> lock(mtx);
            messages[idx].push(msg);
        }
    };

    struct Scope {
        bool enabled = false;
        String tag;
        String file;
        int line;
        String func;
        int64 start;

        Scope(const LogTag* t, const String& f, int l, const String& ff);
        Scope(const LogTagDelegate* t, const String& f, int l, const String& ff);
        ~Scope() {
            if (enabled) {
                auto end = Time::getHighResolutionTicks();
                double ms = Time::highResolutionTicksToSeconds(end - start) * 1000;
                traceMessage(tag, file, line, func, "exit (took " + String(ms) + "ms)");
            }
        }
    };

    static void initialize(const String& appName, const String& filePrefix);

    static void setEnabled(bool b) { m_enabled = b; }
    static inline bool isEnabled() { return m_enabled; }

    static void traceMessage(const LogTag* tag, const String& file, int line, const String& func, const String& msg);
    static void traceMessage(const String& tag, const String& file, int line, const String& func, const String& msg);

    void run();

  private:
    static std::atomic_bool m_enabled;
    static std::unordered_map<Thread::ThreadID, MessageBuffer> m_messageBuffers;
    static std::mutex m_messageBuffersMtx;
    static std::set<Thread::ThreadID> m_messageBuffersKnownThreadIDs;

    static const int MAX_TRACE_MESSAGES_PER_FILE = 10000;
    static const int NUMBER_OF_TRACE_FILES = 10;

    String m_fileName[NUMBER_OF_TRACE_FILES];
    int m_fileIdx = 0;
    std::ofstream m_outstream;
    int m_currentMsgCount = 0;

    static MessageBuffer& getMessageBuffer(Thread::ThreadID tid);
    static void getMessageBufferKnownThreadIDs(std::vector<Thread::ThreadID>& tids);
};

}  // namespace e47

#define traceScope() Tracer::Scope __scope__LINE__(getLogTagSource(), __FILE__, __LINE__, __func__)
#define traceScope1() Tracer::Scope __scope1__LINE__(getLogTagSource(), __FILE__, __LINE__, __func__)
#define traceScope2() Tracer::Scope __scope2__LINE__(getLogTagSource(), __FILE__, __LINE__, __func__)

#endif /* ThreadTracer_hpp */
