/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "Server.hpp"

#include <signal.h>

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
    m_server->shutdown();
    m_server->waitForThreadToExit(-1);
    m_server.reset();
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
        forgetEditorIfNeeded();
        m_window.reset();
        m_windowOwner = tid;
        m_windowProc = proc;
        m_windowFunc = func;
        m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFunc);
    }
}

void App::hideEditor(Thread::ThreadID tid) {
    if (tid == 0 || tid == m_windowOwner) {
        std::lock_guard<std::mutex> lock(m_windowMtx);
        forgetEditorIfNeeded();
        m_window.reset();
        m_windowOwner = 0;
        m_windowProc.reset();
        m_windowFunc = nullptr;
    }
}

void App::resetEditor() {
    std::lock_guard<std::mutex> lock(m_windowMtx);
    forgetEditorIfNeeded();
    m_window.reset();
}

void App::restartEditor() {
    std::lock_guard<std::mutex> lock(m_windowMtx);
    forgetEditorIfNeeded();
    m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFunc);
}

void App::forgetEditorIfNeeded() {
    if (m_windowProc != nullptr && m_windowProc->getActiveEditor() == nullptr && m_window != nullptr) {
        m_window->forgetEditor();
    }
}

}  // namespace e47

// This kicks the whole thing off..
START_JUCE_APPLICATION(e47::App)
