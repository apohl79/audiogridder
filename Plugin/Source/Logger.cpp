/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Logger.hpp"

namespace e47 {

std::shared_ptr<AGLogger> AGLogger::m_inst;
std::mutex AGLogger::m_instMtx;
size_t AGLogger::m_instRefCount = 0;

AGLogger::AGLogger(const String& appName, const String& filePrefix) : Thread("AGLogger") {
    m_logger = FileLogger::createDateStampedLogger(appName, filePrefix, ".log", "");
    Logger::setCurrentLogger(m_logger);
    startThread();
}

AGLogger::~AGLogger() {
    stopThread(-1);
    Logger::setCurrentLogger(nullptr);
    delete m_logger;
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
            m_logger->logMessage(msg);
            m_msgQ[idx].pop();
        }
    }
}

void AGLogger::log(String msg) {
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
    if (m_instRefCount > 1) {
        m_instRefCount--;
    } else {
        m_inst->signalThreadShouldExit();
        m_inst->log("");
        m_inst.reset();
    }
}
}  // namespace e47
