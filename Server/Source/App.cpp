/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "Server.hpp"

#ifdef JUCE_MAC
#include <signal.h>
#endif

namespace e47 {

App::App() : LogTag("app") {}

void App::initialise(const String& commandLineParameters) {
    m_logger = FileLogger::createDateStampedLogger(getApplicationName(), "AudioGridderServer_", ".log", "");
    Logger::setCurrentLogger(m_logger);
#ifdef JUCE_MAC
    signal(SIGPIPE, SIG_IGN);
#endif
    logln("commandline: " << commandLineParameters);
    auto args = getCommandLineParameterArray();
    String fileToScan = "";
    for (int i = 0; i < args.size(); i++) {
        if (!args[i].compare("-scan") && args.size() >= i + 2) {
            fileToScan = args[i + 1];
            break;
        }
    }
    if (fileToScan.length() > 0) {
        auto parts = StringArray::fromTokens(fileToScan, "|", "");
        String id = parts[0];
        String format = "VST";
        if (parts.size() > 1) {
            format = parts[1];
        }
        logln("scan mode: format=" << format << " id=" << id);
        bool success = Server::scanPlugin(id, format);
        logln("..." << (success ? "success" : "failed"));
        setApplicationReturnValue(success ? 0 : 1);
        quit();
    } else {
        showSplashWindow();
        setSplashInfo("Starting server...");
        m_menuWindow = std::make_unique<MenuBarWindow>(this);
        m_server = std::make_unique<Server>();
        m_server->startThread();
    }
}

void App::shutdown() {
    if (m_server != nullptr) {
        m_server->shutdown();
        m_server->waitForThreadToExit(-1);
        m_server.reset();
    }
    Logger::setCurrentLogger(nullptr);
    delete m_logger;
}

void App::restartServer() {
    logln("restarting server...");

    hideEditor();
    hidePluginList();
    hideServerSettings();

    showSplashWindow();
    setSplashInfo("Restarting server...");

    std::thread([this] {
        logln("running restart thread");

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
            logln("show editor: resetting existing processor window");
            m_window->setVisible(false);
            m_window.reset();
        }
        m_windowOwner = tid;
        m_windowProc = proc;
        m_windowFunc = func;
        m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFunc);
    } else {
        logln("show editor failed: '" << proc->getName() << "' has no editor");
    }
}

void App::hideEditor(Thread::ThreadID tid) {
    if (tid == nullptr || tid == m_windowOwner) {
        std::lock_guard<std::mutex> lock(m_windowMtx);
        forgetEditorIfNeeded();
        if (m_window != nullptr) {
            m_window->setVisible(false);
            m_window.reset();
        } else {
            logln("hide editor called with no active processor window");
        }
        m_windowOwner = nullptr;
        m_windowProc.reset();
        m_windowFunc = nullptr;
    } else {
        logln("failed to hide editor: tid does not match window owner");
    }
}

void App::resetEditor() {
    std::lock_guard<std::mutex> lock(m_windowMtx);
    forgetEditorIfNeeded();
    if (m_window != nullptr) {
        logln("resetting processor window");
        m_window->setVisible(false);
        m_window.reset();
    }
}

void App::restartEditor() {
    std::lock_guard<std::mutex> lock(m_windowMtx);
    forgetEditorIfNeeded();
    if (m_windowProc != nullptr) {
        logln("recreating processor window");
        m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFunc);
    }
}

void App::forgetEditorIfNeeded() {
    if (m_windowProc != nullptr && m_windowProc->getActiveEditor() == nullptr && m_window != nullptr) {
        logln("forgetting editor");
        m_window->forgetEditor();
    }
}

Point<float> App::localPointToGlobal(Point<float> lp) {
    if (m_windowProc != nullptr) {
        auto* ed = m_windowProc->getActiveEditor();
        if (ed != nullptr) {
            return ed->localPointToGlobal(lp);
        } else {
            logln("failed to resolve local to global point: processor has no active editor, trying to restart editor");
            restartEditor();
        }
    } else {
        logln("failed to resolve local to global point: no active processor");
    }
    return lp;
}

}  // namespace e47

// This kicks the whole thing off..
START_JUCE_APPLICATION(e47::App)
