/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "Server.hpp"
#include "Utils.hpp"

#ifdef JUCE_MAC
#include <signal.h>
#endif

namespace e47 {

App::App() : m_menuWindow(this) {}

void App::initialise(const String& /* commandLineParameters */) {
    m_logger = FileLogger::createDateStampedLogger(getApplicationName(), "Main_", ".log", "");
    Logger::setCurrentLogger(m_logger);
#ifdef JUCE_MAC
    signal(SIGPIPE, SIG_IGN);
#endif
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

    showSplashWindow();
    setSplashInfo("Restarting server...");

    std::thread([this] {
        // leave message thread context
        m_server->shutdown();
        m_server->waitForThreadToExit(-1);
        m_server.reset();
        m_server = std::make_unique<Server>();
        m_server->startThread();
    }).detach();
}

const KnownPluginList& App::getPluginList() { return m_server->getPluginList(); }

void App::showEditor(std::shared_ptr<AudioProcessor> proc, Thread::ThreadID tid, WindowCaptureCallback func) {
    if (proc->hasEditor()) {
        std::lock_guard<std::mutex> lock(m_windowMtx);
        forgetEditorIfNeeded();
        if (m_window != nullptr) {
            m_window->setVisible(false);
            m_window.reset();
        }
        m_windowOwner = tid;
        m_windowProc = proc;
        m_windowFunc = func;
        m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFunc);
    }
}

void App::hideEditor(Thread::ThreadID tid) {
    if (tid == nullptr || tid == m_windowOwner) {
        std::lock_guard<std::mutex> lock(m_windowMtx);
        forgetEditorIfNeeded();
        if (m_window != nullptr) {
            m_window->setVisible(false);
            m_window.reset();
        }
        m_windowOwner = nullptr;
        m_windowProc.reset();
        m_windowFunc = nullptr;
    }
}

void App::resetEditor() {
    std::lock_guard<std::mutex> lock(m_windowMtx);
    forgetEditorIfNeeded();
    if (m_window != nullptr) {
        m_window->setVisible(false);
        m_window.reset();
    }
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

Point<float> App::localPointToGlobal(Point<float> lp) {
    if (m_windowProc != nullptr) {
        auto* ed = m_windowProc->getActiveEditor();
        if (ed != nullptr) {
            return ed->localPointToGlobal(lp);
        }
    }
    logln("failed to resolve local to global point");
    return lp;
}

}  // namespace e47

// This kicks the whole thing off..
START_JUCE_APPLICATION(e47::App)
