/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Tracer.hpp"
#include "Utils.hpp"

namespace e47 {

std::atomic_bool Tracer::m_enabled{false};
std::unordered_map<Thread::ThreadID, Tracer::MessageBuffer> Tracer::m_messageBuffers;
std::mutex Tracer::m_messageBuffersMtx;
std::set<Thread::ThreadID> Tracer::m_messageBuffersKnownThreadIDs;

Tracer::Tracer() : Thread("ThreadTracer") {}

Tracer::~Tracer() { stopThread(-1); }

Tracer::Scope::Scope(const LogTag* t, const String& f, int l, const String& ff) {
    if (Tracer::isEnabled()) {
        enabled = true;
        tag = t->getLogTagNoTime();
        file = f;
        line = l;
        func = ff;
        start = Time::getHighResolutionTicks();
        traceMessage(tag, file, line, func, "enter");
    }
}

Tracer::Scope::Scope(const LogTagDelegate* t, const String& f, int l, const String& ff)
    : Scope(t->getLogTagSource(), f, l, ff) {}

void Tracer::initialize(const String& appName, const String& filePrefix) {
    SharedInstance<Tracer>::initialize([&appName, &filePrefix](auto inst) {
        for (int i = 0; i < NUMBER_OF_TRACE_FILES; i++) {
            auto file = FileLogger::getSystemLogFileFolder()
                            .getChildFile(appName)
                            .getChildFile(filePrefix + Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S"))
                            .withFileExtension(".trace." + String(i))
                            .getNonexistentSibling();
            inst->m_fileName[i] = file.getFullPathName();
        }
        inst->startThread();
    });
}

void Tracer::traceMessage(const LogTag* tag, const String& file, int line, const String& func, const String& msg) {
    if (m_enabled) {
        traceMessage(tag->getLogTagNoTime(), file, line, func, msg);
    }
}

void Tracer::traceMessage(const String& tag, const String& file, int line, const String& func, const String& msg) {
    if (m_enabled) {
        auto tid = Thread::getCurrentThreadId();
        auto thread = Thread::getCurrentThread();
        String threadTag = "unknown";
        if (nullptr != thread) {
            threadTag = thread->getThreadName();
        } else {
            auto mm = MessageManager::getInstanceWithoutCreating();
            if (nullptr != mm && mm->isThisTheMessageThread()) {
                threadTag = "message_thread";
            }
        }
        threadTag << ":0x" << String::toHexString((uint64)tid);
        String out = LogTag::getTimeStr();
        out << "|" << threadTag << "|" << tag << "|" << File::createFileWithoutCheckingPath(file).getFileName() << ":"
            << line << "|" << func << "|" << msg;
        getMessageBuffer(tid).push(out);
    }
}

void Tracer::run() {
    std::vector<Thread::ThreadID> knownThreadIDs;
    int emptyCount = 0;
    while (!currentThreadShouldExit()) {
        if (!m_enabled) {
            sleep(500);
            continue;
        }
        if (m_currentMsgCount > MAX_TRACE_MESSAGES_PER_FILE) {
            m_outstream.close();
            ++m_fileIdx %= NUMBER_OF_TRACE_FILES;
            m_currentMsgCount = 0;
        }
        if (!m_outstream.is_open()) {
            m_outstream.open(m_fileName[m_fileIdx].getCharPointer());
            if (!m_outstream.is_open()) {
                m_enabled = false;
                return;
            }
        }
        knownThreadIDs.clear();
        getMessageBufferKnownThreadIDs(knownThreadIDs);
        bool hadMessages = false;
        for (auto tid : knownThreadIDs) {
            auto& msgBuf = getMessageBuffer(tid);
            auto idx = msgBuf.getAndUpdateIndex();
            hadMessages = !msgBuf.messages[idx].empty() || hadMessages;
            while (!msgBuf.messages[idx].empty()) {
                auto& msg = msgBuf.messages[idx].front();
                m_outstream << msg.toStdString() << std::endl;
                msgBuf.messages[idx].pop();
                m_currentMsgCount++;
            }
        }
        emptyCount += hadMessages ? 0 : 1;
        if (emptyCount % 2 == 0) {
            sleep(5);
        }
    }
}

Tracer::MessageBuffer& Tracer::getMessageBuffer(Thread::ThreadID tid) {
    std::lock_guard<std::mutex> lock(m_messageBuffersMtx);
    m_messageBuffersKnownThreadIDs.insert(tid);
    return m_messageBuffers[tid];
}

void Tracer::getMessageBufferKnownThreadIDs(std::vector<Thread::ThreadID>& tids) {
    std::lock_guard<std::mutex> lock(m_messageBuffersMtx);
    if (tids.size() < m_messageBuffersKnownThreadIDs.size()) {
        tids.resize(m_messageBuffersKnownThreadIDs.size());
    }
    for (auto tid : m_messageBuffersKnownThreadIDs) {
        tids.push_back(tid);
    }
}

}  // namespace e47
