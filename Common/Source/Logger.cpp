/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Logger.hpp"
#include "Defaults.hpp"
#include "json.hpp"

#if defined(JUCE_DEBUG) && defined(JUCE_WINDOWS)
#include <windows.h>
#endif

using json = nlohmann::json;

namespace e47 {

std::shared_ptr<Logger> Logger::m_inst;
std::mutex Logger::m_instMtx;
size_t Logger::m_instRefCount = 0;

std::atomic_bool Logger::m_enabled{true};

Logger::Logger(const String& appName, const String& filePrefix) : Thread("Logger") {
#ifdef JUCE_DEBUG
    m_logToErr = juce_isRunningUnderDebugger();
#endif
    if (appName.isNotEmpty()) {
        m_file = File(Defaults::getLogFileName(appName, filePrefix, ".log")).getNonexistentSibling();
        // create dir if needed
        auto d = m_file.getParentDirectory();
        if (!d.exists()) {
            d.createDirectory();
        }
        // create a latest link
        auto latestLnk = File(Defaults::getLogFileName(appName, filePrefix, ".log", true));
        m_file.createSymbolicLink(latestLnk, true);
        // cleanup
        int filesToKeep = appName == "Sandbox-Chain" ? 50 : 5;
        cleanDirectory(d.getFullPathName(), filePrefix, ".log", filesToKeep);
    }
}

Logger::~Logger() {
    if (isThreadRunning()) {
        stopThread(3000);
    }
    if (m_outstream.is_open()) {
        m_outstream.close();
    }
    if (m_deleteFile) {
        m_file.deleteFile();
    }
}

void Logger::run() {
    while (!threadShouldExit()) {
        size_t idx;
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_cv.wait(lock, [this] { return m_msgQ[m_msgQIdx].size() > 0 || threadShouldExit(); });
            idx = m_msgQIdx;
            m_msgQIdx = m_msgQIdx ? 0 : 1;
        }
        while (m_msgQ[idx].size() > 0) {
            auto& msg = m_msgQ[idx].front();
            logMsg(msg);
            m_msgQ[idx].pop();
        }
    }
}

void Logger::log(String msg) {
    if (m_enabled) {
        auto inst = getInstance();
        if (nullptr != inst) {
            if (inst->m_logDirectly) {
                inst->logMsg(msg);
            } else {
                inst->logToQueue(std::move(msg));
            }
        }
    }
}

void Logger::logMsg(const String& msg) {
    if (m_outstream.is_open()) {
        m_outstream << msg.toStdString() << std::endl;
    }
    if (m_logToErr) {
#ifdef JUCE_WINDOWS
#if JUCE_DEBUG
        if (m_debugger) {
            OutputDebugStringA((msg + "\n").getCharPointer());
        }
#endif
        if (!m_debugger) {
            std::cerr << msg.toStdString() << std::endl;
        }
#else
        ignoreUnused(m_debugger);
        std::cerr << msg.toStdString() << std::endl;
#endif
    }
}

void Logger::logToQueue(String msg) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_msgQ[m_msgQIdx].push(std::move(msg));
    m_cv.notify_one();
}

void Logger::initialize(const String& appName, const String& filePrefix, const String& configFile, bool logDirectly) {
    bool checkConfig = false;
    {
        std::lock_guard<std::mutex> lock(m_instMtx);
        if (nullptr == m_inst) {
            m_inst = std::make_shared<Logger>(appName, filePrefix);
            m_inst->m_logDirectly = logDirectly;
            checkConfig = true;
        }
        m_instRefCount++;
    }
    if (checkConfig) {
        bool enable = jsonGetValue(configParseFile(configFile), "Logger", m_enabled.load());
        setEnabled(enable);
    }
}

std::shared_ptr<Logger> Logger::getInstance() { return m_inst; }

void Logger::deleteFileAtFinish() {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr != m_inst) {
        m_inst->m_deleteFile = true;
    }
}

void Logger::cleanup() {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (m_instRefCount > 0) {
        m_instRefCount--;
        if (m_instRefCount == 0) {
            if (nullptr != m_inst && m_inst->isThreadRunning()) {
                m_inst->signalThreadShouldExit();
                m_inst->logToQueue("");
            }
            m_inst.reset();
        }
    }
}

void Logger::setEnabled(bool b) {
    if (b) {
        std::lock_guard<std::mutex> lock(m_instMtx);
        if (nullptr != m_inst && !m_inst->m_outstream.is_open()) {
            if (m_inst->m_file.getFileName().isNotEmpty()) {
                if (!m_inst->m_file.exists()) {
                    m_inst->m_file.create();
                }
                m_inst->m_outstream.open(m_inst->m_file.getFullPathName().getCharPointer());
            }
            if (!m_inst->m_logDirectly) {
                m_inst->startThread();
            }
        }
    }
    m_enabled = b;
}

File Logger::getLogFile() {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr != m_inst) {
        return m_inst->m_file;
    }
    return {};
}

void Logger::setLogToErr(bool b) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr != m_inst) {
        m_inst->m_logToErr = b;
    }
}

void Logger::setLogDirectly(bool b) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr != m_inst) {
        m_inst->m_logDirectly = b;
    }
}

}  // namespace e47
