/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Logger.hpp"

#if defined(JUCE_DEBUG) && defined(JUCE_WINDOWS)
#include <windows.h>
#endif

namespace e47 {

std::shared_ptr<AGLogger> AGLogger::m_inst;
std::mutex AGLogger::m_instMtx;
size_t AGLogger::m_instRefCount = 0;

AGLogger::AGLogger(const String& appName, const String& filePrefix) : Thread("AGLogger") {
#ifdef JUCE_DEBUG
    m_logToErr = juce_isRunningUnderDebugger();
#endif
    m_file = FileLogger::getSystemLogFileFolder()
                 .getChildFile(appName)
                 .getChildFile(filePrefix + Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S"))
                 .withFileExtension(".log")
                 .getNonexistentSibling();
    if (!m_file.exists()) {
        m_file.create();
    }
    m_outstream.open(m_file.getFullPathName().getCharPointer());
    if (m_outstream.is_open()) {
        startThread();
    }
}

AGLogger::~AGLogger() {
    stopThread(3000);
    if (m_outstream.is_open()) {
        m_outstream.close();
    }
}

void AGLogger::run() {
    while (!currentThreadShouldExit()) {
        size_t idx;
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_cv.wait(lock, [this] { return m_msgQ[m_msgQIdx].size() > 0 || currentThreadShouldExit(); });
            idx = m_msgQIdx;
            m_msgQIdx = m_msgQIdx ? 0 : 1;
        }
        while (m_msgQ[idx].size() > 0) {
            auto& msg = m_msgQ[idx].front();
            m_outstream << msg.toStdString() << std::endl;
#ifdef JUCE_DEBUG
            if (m_logToErr) {
#ifdef JUCE_WINDOWS
                OutputDebugStringA((msg + "\n").getCharPointer());
#else
                std::cerr << msg.toStdString() << std::endl;
#endif
            }
#endif
            m_msgQ[idx].pop();
        }
    }
}

void AGLogger::log(String msg) {
    auto inst = getInstance();
    if (nullptr != inst) {
        inst->logReal(std::move(msg));
    }
}

void AGLogger::logReal(String msg) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_msgQ[m_msgQIdx].push(std::move(msg));
    m_cv.notify_one();
}

void AGLogger::initialize(const String& appName, const String& filePrefix) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr == m_inst) {
        m_inst = std::make_shared<AGLogger>(appName, filePrefix);
    }
    m_instRefCount++;
}

std::shared_ptr<AGLogger> AGLogger::getInstance() { return m_inst; }

void AGLogger::cleanup() {
    std::lock_guard<std::mutex> lock(m_instMtx);
    m_instRefCount--;
    if (m_instRefCount == 0) {
        m_inst->signalThreadShouldExit();
        m_inst->log("");
        m_inst.reset();
    }
}
}  // namespace e47
