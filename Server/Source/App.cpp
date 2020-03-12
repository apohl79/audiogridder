/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"

#include <signal.h>

#include "Server.hpp"

namespace e47 {

App::App() : m_menuWindow(this) {}

void App::initialise(const String& commandLineParameters) {
    m_logger = FileLogger::createDateStampedLogger(getApplicationName(), "Main_", ".log", "");
    Logger::setCurrentLogger(m_logger);
    signal(SIGPIPE, SIG_IGN);
    showSplashWindow();
    setSplashInfo("Starting server...");
    m_server = std::make_unique<Server>();
    m_server->startThread();
}

void App::shutdown() {
    m_server->signalThreadShouldExit();
    m_server->waitForThreadToExit(1000);
    Logger::setCurrentLogger(nullptr);
    delete m_logger;
}

void App::restartServer() {
    hideEditor();
    hidePluginList();
    hideServerSettings();

    m_server->signalThreadShouldExit();
    m_server->waitForThreadToExit(1000);
    m_server = std::make_unique<Server>();
    m_server->startThread();
}

const KnownPluginList& App::getPluginList() { return m_server->getPluginList(); }

void App::showEditor(std::shared_ptr<AudioProcessor> proc, Thread::ThreadID tid, WindowCaptureCallback func) {
    if (proc->hasEditor()) {
        std::lock_guard<std::mutex> lock(m_windowMtx);
        // if (nullptr != m_window && m_windowOwner != tid) {
        //    m_window->hide();
        //}
        m_window = std::make_unique<ProcessorWindow>(proc, func);
        m_windowOwner = tid;
    }
}

void App::hideEditor(Thread::ThreadID tid) {
    if (tid == 0 || tid == m_windowOwner) {
        m_window.reset();
    }
}

}  // namespace e47

// This kicks the whole thing off..
START_JUCE_APPLICATION(e47::App)
