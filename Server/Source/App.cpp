/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "Server.hpp"
#include "Screen.h"
#include "Signals.hpp"
#include "CoreDump.hpp"

#ifdef JUCE_WINDOWS
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#endif

namespace e47 {

void App::initialise(const String& commandLineParameters) {
    auto args = getCommandLineParameterArray();
    enum Modes { SCAN, MASTER, SERVER };
    Modes mode = MASTER;
    String fileToScan = "";
    int srvid = -1;
    for (int i = 0; i < args.size(); i++) {
        if (!args[i].compare("-scan") && args.size() >= i + 2) {
            fileToScan = args[i + 1];
            mode = SCAN;
        } else if (!args[i].compare("-server")) {
            mode = SERVER;
        } else if (!args[i].compare("-id")) {
            srvid = args[i + 1].getIntValue();
        }
    }
    String appName;
    String logName = getApplicationName() + "_";
    switch (mode) {
        case MASTER:
            appName = "Master";
            break;
        case SCAN:
            appName = "Scan";
            logName = fileToScan + "_";
            logName = logName.replaceCharacters(":/\\|. ", "------").trimCharactersAtStart("-");
            break;
        case SERVER:
            appName = "Server";
            break;
    }
    AGLogger::initialize(appName, logName, Defaults::getConfigFileName(Defaults::ConfigServer));
    Tracer::initialize(appName, logName);
    Signals::initialize();

    logln("commandline: " << commandLineParameters);
    switch (mode) {
        case SCAN:
#ifdef JUCE_MAC
            Process::setDockIconVisible(false);
#endif
            AGLogger::setEnabled(true);
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
                logln("error: fileToScan missing");
                setApplicationReturnValue(1);
                quit();
            }
            break;
        case SERVER: {
            traceScope();
            CoreDump::initialize(appName, logName, false);
            showSplashWindow();
            setSplashInfo("Starting server...");
            m_menuWindow = std::make_unique<MenuBarWindow>(this);
#ifdef JUCE_MAC
            if (!askForAccessibilityPermission()) {
                AlertWindow::showMessageBox(
                    AlertWindow::WarningIcon, "Warning",
                    "AudioGridder needs the Accessibility permission to remote control plugins.", "OK");
            }
#endif
            json opts;
            if (srvid > -1) {
                opts["ID"] = srvid;
            }
            m_server = std::make_shared<Server>(opts);
            m_server->initialize();
            m_server->startThread();
            break;
        }
        case MASTER:
#ifdef JUCE_MAC
            Process::setDockIconVisible(false);
#endif
            m_child = std::make_unique<std::thread>([this, srvid] {
                ChildProcess proc;
                StringArray proc_args;
                proc_args.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
                proc_args.add("-server");
                if (srvid > -1) {
                    proc_args.add("-id");
                    proc_args.add(String(srvid));
                }
                uint32 ec = 0;
                bool done = false;
                do {
                    if (proc.start(proc_args)) {
                        while (proc.isRunning()) {
                            Thread::sleep(100);
                            if (m_stopChild) {
                                logln("killing child process");
                                proc.kill();
                                proc.waitForProcessToFinish(-1);
                                File serverRunFile(Defaults::getConfigFileName(Defaults::ConfigServerRun));
                                if (serverRunFile.exists()) {
                                    serverRunFile.deleteFile();
                                }
                                done = true;
                                break;
                            }
                        }
                        ec = proc.getExitCode();
                        if (ec == EXIT_RESTART) {
                            logln("restarting server");
                            continue;
                        } else if (ec != 0) {
                            logln("error: server failed with exit code " << as<int>(ec));
                        }
                        File serverRunFile(Defaults::getConfigFileName(Defaults::ConfigServerRun));
                        if (serverRunFile.exists()) {
                            logln("error: server did non shutdown properly");
                            serverRunFile.deleteFile();
                        } else {
                            done = true;
                        }
                    } else {
                        logln("error: failed to start server process");
                        setApplicationReturnValue(1);
                        quit();
                        done = true;
                    }
                } while (!done);
                quit();
            });
            break;
    }
    logln("initialise complete");
}

void App::prepareShutdown(uint32 exitCode) {
    traceScope();
    logln("preparing shutdown");

    m_exitCode = exitCode;

    std::thread([this] {
        traceScope();

        if (m_server != nullptr) {
            m_server->shutdown();
            m_server->waitForThreadToExit(-1);
            m_server.reset();
        }

        quit();
    }).detach();
}

void App::shutdown() {
    traceScope();
    logln("shutdown");

    if (m_child != nullptr) {
        m_stopChild = true;
        if (m_child->joinable()) {
            m_child->join();
        }
    }

    if (m_server != nullptr) {
        m_server->shutdown();
        m_server->waitForThreadToExit(-1);
        m_server.reset();
    }

    Tracer::cleanup();
    AGLogger::cleanup();
    setApplicationReturnValue((int)m_exitCode);
}

void App::restartServer(bool rescan) {
    traceScope();

    logln("restarting server...");

    hideEditor();
    hidePluginList();
    hideServerSettings();

    showSplashWindow();
    setSplashInfo("Restarting server...");

    std::thread([this, rescan] {
        traceScope();

        logln("running restart thread");

        // leave message thread context
        int id = m_server->getId();
        m_server->shutdown();
        m_server->waitForThreadToExit(-1);
        m_server.reset();
        json opts;
        opts["ID"] = id;
        if (rescan) {
            opts["ScanForPlugins"] = true;
        } else {
            opts["NoScanForPlugins"] = true;
        }
        m_server = std::make_unique<Server>(opts);
        m_server->initialize();
        m_server->startThread();
    }).detach();
}

const KnownPluginList& App::getPluginList() { return m_server->getPluginList(); }

void App::showEditor(std::shared_ptr<AGProcessor> proc, Thread::ThreadID tid, WindowCaptureCallbackNative func) {
    traceScope();
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
        m_windowFuncNative = func;
        m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFuncNative);
    } else {
        logln("show editor failed: '" << proc->getName() << "' has no editor");
    }
}

void App::showEditor(std::shared_ptr<AGProcessor> proc, Thread::ThreadID tid, WindowCaptureCallbackFFmpeg func) {
    traceScope();
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
        m_windowFuncFFmpeg = func;
        m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFuncFFmpeg);
    } else {
        logln("show editor failed: '" << proc->getName() << "' has no editor");
    }
}

void App::hideEditor(Thread::ThreadID tid) {
    traceScope();
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
        m_windowFuncNative = nullptr;
        m_windowFuncFFmpeg = nullptr;
    } else {
        logln("failed to hide editor: tid does not match window owner");
    }
}

void App::resetEditor() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_windowMtx);
    forgetEditorIfNeeded();
    if (m_window != nullptr) {
        logln("resetting processor window");
        m_window->setVisible(false);
        m_window.reset();
    }
}

void App::restartEditor() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_windowMtx);
    forgetEditorIfNeeded();
    if (m_windowProc != nullptr) {
        logln("recreating processor window");
        m_window = std::make_unique<ProcessorWindow>(m_windowProc, m_windowFuncNative);
    }
}

void App::forgetEditorIfNeeded() {
    traceScope();
    // No lock, locked already
    if (m_windowProc != nullptr && m_windowProc->getActiveEditor() == nullptr && m_window != nullptr) {
        logln("forgetting editor");
        m_window->forgetEditor();
    }
}

void App::updateScreenCaptureArea(int val) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_windowMtx);
    if (m_windowProc != nullptr && m_window != nullptr) {
        m_windowProc->updateScreenCaptureArea(val);
        m_window->updateScreenCaptureArea();
    }
}

Point<float> App::localPointToGlobal(Point<float> lp) {
    traceScope();
    std::lock_guard<std::mutex> lock(m_windowMtx);
    if (m_windowProc != nullptr) {
        auto* ed = m_windowProc->getActiveEditor();
        if (ed != nullptr) {
            return ed->localPointToGlobal(lp);
        } else {
            logln("failed to resolve local to global point: processor has no active editor, trying to restart editor");
            m_windowMtx.unlock();
            restartEditor();
        }
    } else {
        logln("failed to resolve local to global point: no active processor");
    }
    return lp;
}

void App::ProcessorWindow::createEditor() {
    traceScope();
    setTitleBarHeight(30);
    auto disp = Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (nullptr != disp) {
        m_totalRect = disp->totalArea;
    }
    m_editor = m_processor->createEditorIfNeeded();
    if (nullptr != m_editor) {
#ifdef JUCE_WINDOWS
        // try to figure out, if the HWND is valid, this is a temp hack to investigate some user crash reports
        auto peer = ComponentPeer::getPeerFor(this);
        if (nullptr != peer) {
            auto hwnd = (HWND)peer->getNativeHandle();
            if (!IsWindow(hwnd)) {
                logln("failed to create editor: invalid HWND of processor window");
                return;
            }
        }
#endif
        setContentNonOwned(m_editor, true);
        Component::setVisible(true);
        startCapturing();
    } else {
        logln("failed to create editor");
    }
}

}  // namespace e47

// This kicks the whole thing off..
START_JUCE_APPLICATION(e47::App)
