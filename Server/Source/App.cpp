/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "Defaults.hpp"
#include "Server.hpp"
#include "Screen.h"
#include "Signals.hpp"
#include "Sentry.hpp"
#include "Processor.hpp"
#include "MenuBarWindow.hpp"
#include "ServerSettingsWindow.hpp"
#include "PluginListWindow.hpp"
#include "StatisticsWindow.hpp"
#include "SplashWindow.hpp"

#ifdef JUCE_WINDOWS
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#endif

namespace e47 {

App::App() : LogTag("app") { initAsyncFunctors(); }
App::~App() { stopAsyncFunctors(); }

void App::initialise(const String& commandLineParameters) {
    auto args = getCommandLineParameterArray();
    enum Modes { SCAN, MASTER, SERVER, SANDBOX_CHAIN, SANDBOX_PLUGIN };
    Modes mode = MASTER;
    String fileToScan, pluginId, error;
    int workerPort = 0, srvId = -1;
    json jconfig;
    bool log = false, isLocal = false;
    for (int i = 0; i < args.size(); i++) {
        if (!args[i].compare("-scan") && args.size() >= i + 2) {
            fileToScan = args[i + 1];
            mode = SCAN;
        } else if (!args[i].compare("-server")) {
            mode = SERVER;
        } else if (args[i].startsWith("--" + Defaults::SANDBOX_CMD_PREFIX)) {
            mode = SANDBOX_CHAIN;
        } else if (!args[i].compare("-load")) {
            mode = SANDBOX_PLUGIN;
        } else if (!args[i].compare("-log")) {
            log = true;
        } else if (!args[i].compare("-islocal")) {
            isLocal = args[i + 1] == "1";
        } else if (!args[i].compare("-pluginid")) {
            pluginId = args[i + 1];
        } else if (!args[i].compare("-workerport")) {
            workerPort = args[i + 1].getIntValue();
        } else if (!args[i].compare("-id")) {
            srvId = args[i + 1].getIntValue();
        } else if (!args[i].compare("-config")) {
            MemoryBlock config;
            if (config.fromBase64Encoding(args[i + 1])) {
                try {
                    jconfig = json::parse(config.begin(), config.end());
                } catch (json::parse_error& e) {
                    error << "failed to parse -config value: " << e.what();
                }
            } else {
                error << "failed to decode -config value";
            }
        }
    }
    String cfgFile = Defaults::getConfigFileName(Defaults::ConfigServer, {{"id", String(srvId)}});
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
        case SANDBOX_PLUGIN:
            appName = "Sandbox-Plugin";
            logName = pluginId + "_";
            break;
        case SERVER:
            appName = "Server";
            break;
        case SANDBOX_CHAIN:
            appName = "Sandbox-Chain";
            break;
    }
    Logger::initialize(appName, logName, cfgFile);
    Tracer::initialize(appName, logName);
    Signals::initialize();
    Defaults::initServerTheme();

    if (log) {
        Logger::setLogToErr(true);
    }

    logln("commandline: " << commandLineParameters);

    if (error.isNotEmpty()) {
        logln(error);
        setApplicationReturnValue(1);
        quit();
        return;
    }

    switch (mode) {
        case SCAN:
#ifdef JUCE_MAC
            Process::setDockIconVisible(false);
#endif
            Logger::setEnabled(true);
            if (fileToScan.length() > 0) {
                auto parts = StringArray::fromTokens(fileToScan, "|", "");
                String id = parts[0];
                String format = "VST";
                if (parts.size() > 1) {
                    format = parts[1];
                }
                logln("scan mode: format=" << format << " id=" << id);
                bool success = Server::scanPlugin(id, format, srvId > -1 ? srvId : 0);
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
            if (srvId > -1) {
                opts["ID"] = srvId;
            }
            m_server = std::make_shared<Server>(opts);
            m_server->initialize();
            m_server->startThread();
            break;
        }
        case SANDBOX_CHAIN: {
            traceScope();
#ifdef JUCE_MAC
            Process::setDockIconVisible(false);
#endif
            auto cfg = configParseFile(cfgFile);
            bool enableLogAutoclean = jsonGetValue(cfg, "SandboxLogAutoclean", true);
            if (enableLogAutoclean) {
                Logger::deleteFileAtFinish();
                Tracer::deleteFileAtFinish();
            }
            json opts = {
                {"sandboxMode", "chain"}, {"commandLine", commandLineParameters.toStdString()}, {"isLocal", isLocal}};
            if (srvId > -1) {
                opts["ID"] = srvId;
            }
            m_server = std::make_shared<Server>(opts);
            m_server->initialize();
            m_server->startThread();
            break;
        }
        case SANDBOX_PLUGIN: {
            traceScope();
#ifdef JUCE_MAC
            Process::setDockIconVisible(false);
#endif
            json opts = {{"sandboxMode", "plugin"},
                         {"commandLine", commandLineParameters.toStdString()},
                         {"pluginId", pluginId.toStdString()},
                         {"workerPort", workerPort},
                         {"config", jconfig}};
            if (srvId > -1) {
                opts["ID"] = srvId;
            }
            m_server = std::make_shared<Server>(opts);
            m_server->setHost("127.0.0.1");
            m_server->initialize();
            m_server->startThread();
            break;
        }
        case MASTER:
#ifdef JUCE_MAC
            Process::setDockIconVisible(false);
            File appState("~/Library/Saved Application State/com.e47.AudioGridderServer.savedState");
            if (appState.exists()) {
                appState.deleteRecursively();
            }
#endif
            m_child = std::make_unique<std::thread>([this, srvId] {
                ChildProcess proc;
                StringArray proc_args;
                proc_args.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
                proc_args.add("-server");
                if (srvId > -1) {
                    proc_args.add("-id");
                    proc_args.add(String(srvId));
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
                                File serverRunFile(Defaults::getConfigFileName(
                                    Defaults::ConfigServerRun, {{"id", String(srvId > -1 ? srvId : 0)}}));
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
                            logln("error: server failed with exit code " << (int)ec);
                        }
                        File serverRunFile(Defaults::getConfigFileName(Defaults::ConfigServerRun,
                                                                       {{"id", String(srvId > -1 ? srvId : 0)}}));
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
        Thread::setCurrentThreadName("ShutdownThread");

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
    Logger::cleanup();
    Sentry::cleanup();
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

template <typename T>
void App::showEditorInternal(std::shared_ptr<Processor> proc, Thread::ThreadID tid, T func, int x, int y) {
    traceScope();

    if (tid == nullptr) {
        logln("showEditor failed: tid is nullptr");
        return;
    }

    if (proc->hasEditor()) {
        std::lock_guard<std::mutex> lock(m_windowsMtx);

        logln("showing editor: tid=0x" << String::toHexString((uint64)tid));

        auto& helper = m_windows[(uint64)tid];

        if (helper.window != nullptr) {
            logln("showEditor: resetting existing processor window");
            helper.reset();
        }

        helper.processor = proc;
        helper.window = std::make_unique<ProcessorWindow>(proc, func, x, y);

#ifdef JUCE_MAC
        if (getServer()->getSandboxMode() != Server::SANDBOX_PLUGIN ||
            getServer()->getSandboxModeRuntime() == Server::SANDBOX_PLUGIN) {
            Process::setDockIconVisible(true);
        }
#endif
    } else {
        logln("showEditor failed: '" << proc->getName() << "' has no editor");
    }
}

void App::showEditor(std::shared_ptr<Processor> proc, Thread::ThreadID tid,
                     ProcessorWindow::CaptureCallbackFFmpeg func, int x, int y) {
    showEditorInternal(proc, tid, func, x, y);
}

void App::showEditor(std::shared_ptr<Processor> proc, Thread::ThreadID tid,
                     ProcessorWindow::CaptureCallbackNative func, int x, int y) {
    showEditorInternal(proc, tid, func, x, y);
}

void App::hideEditor(Thread::ThreadID tid, bool updateMacOSDock) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_windowsMtx);

    if (tid == nullptr) {
        if (!m_windows.empty()) {
            logln("hiding all editors");

            // hide all windows
            for (auto it = m_windows.begin(); it != m_windows.end();) {
                it->second.reset();
                it = m_windows.erase(it);
            }
        }
    } else {
        logln("hiding editor: tid=0x" << String::toHexString((uint64)tid));

        auto it = m_windows.find((uint64)tid);
        if (it != m_windows.end()) {
            it->second.reset();
            m_windows.erase(it);
        } else {
            logln("failed to hide editor: tid does not match a window owner");
        }
    }

#ifdef JUCE_MAC
    if (updateMacOSDock &&
        (getServer()->getSandboxMode() != Server::SANDBOX_PLUGIN ||
         getServer()->getSandboxModeRuntime() == Server::SANDBOX_PLUGIN) &&
        m_windows.empty()) {
        Process::setDockIconVisible(false);
    }
#else
    ignoreUnused(updateMacOSDock);
#endif
}

void App::ProcessorWindowHelper::reset() {
    if (nullptr != window) {
        if (nullptr != processor && nullptr == processor->getActiveEditor() && window->hasEditor()) {
            window->forgetEditor();
        }
        window->setVisible(false);
    }
    window.reset();
    processor.reset();
}

void App::bringEditorToFront(Thread::ThreadID tid) {
    traceScope();

    logln("bringing editor to front: tid=0x" << String::toHexString((uint64)tid));

    std::lock_guard<std::mutex> lock(m_windowsMtx);

    auto it = m_windows.find((uint64)tid);
    if (it != m_windows.end()) {
        if (it->second.window != nullptr) {
            it->second.window->toTop();
        }
    } else {
        logln("bringEditorToFront failed: no window for tid");
    }
}

std::shared_ptr<Processor> App::getCurrentWindowProc(Thread::ThreadID tid) {
    std::lock_guard<std::mutex> lock(m_windowsMtx);
    auto it = m_windows.find((uint64)tid);
    if (it != m_windows.end()) {
        return it->second.processor;
    }
    return nullptr;
}

void App::moveEditor(Thread::ThreadID tid, int x, int y) {
    traceScope();
    if (getServer()->getScreenLocalMode()) {
        logln("moving editor: tid=0x" << String::toHexString((uint64)tid));

        std::lock_guard<std::mutex> lock(m_windowsMtx);

        auto it = m_windows.find((uint64)tid);
        if (it != m_windows.end()) {
            if (it->second.window != nullptr) {
                logln("moving editor window to " << x << "x" << y);
                it->second.window->move(x, y);
            }
        } else {
            logln("moveEditor failed: no window for tid");
        }
    }
}

void App::resetEditor(Thread::ThreadID tid) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_windowsMtx);

    auto it = m_windows.find((uint64)tid);
    if (it != m_windows.end()) {
        it->second.window.reset();
    }
}

void App::restartEditor(Thread::ThreadID tid) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_windowsMtx);

    auto it = m_windows.find((uint64)tid);
    if (it != m_windows.end()) {
        if (it->second.processor != nullptr) {
            logln("recreating processor window");
            auto& helper = it->second;
            if (nullptr != helper.callbackFFmpeg) {
                helper.window = std::make_unique<ProcessorWindow>(helper.processor, helper.callbackFFmpeg,
                                                                  helper.window->getX(), helper.window->getY());
            } else if (nullptr != helper.callbackNative) {
                helper.window = std::make_unique<ProcessorWindow>(helper.processor, helper.callbackNative,
                                                                  helper.window->getX(), helper.window->getY());
            }
        }
    } else {
        logln("restartEditor failed: no window for tid");
    }
}

void App::addKeyListener(Thread::ThreadID tid, KeyListener* l) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_windowsMtx);

    auto it = m_windows.find((uint64)tid);
    if (it != m_windows.end() && it->second.window != nullptr) {
        it->second.window->addKeyListener(l);
    }
}

void App::updateScreenCaptureArea(Thread::ThreadID tid, int val) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_windowsMtx);

    auto it = m_windows.find((uint64)tid);
    if (it != m_windows.end() && it->second.window != nullptr && it->second.processor != nullptr) {
        if (val != 0) {
            it->second.processor->updateScreenCaptureArea(val);
        }
        it->second.window->updateScreenCaptureArea();
    }
}

Point<float> App::localPointToGlobal(Thread::ThreadID tid, Point<float> lp) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_windowsMtx);

    auto it = m_windows.find((uint64)tid);
    if (it != m_windows.end() && it->second.window != nullptr) {
        auto ret = it->second.window->localPointToGlobal(lp);
        if (!it->second.processor->isFullscreen()) {
            ret.y += it->second.window->getTitleBarHeight();
        } else {
            if (auto* disp = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                ret.x -= disp->userArea.getX();
                ret.y -= disp->userArea.getY();
            }
        }
        return ret;
    } else {
        logln("failed to resolve local to global point: no active window");
    }

    return lp;
}

PopupMenu App::getMenuForIndex(int topLevelMenuIndex, const String& /* menuName */) {
    PopupMenu menu;
    if (topLevelMenuIndex == 0) {  // Settings
        menu.addItem("Settings", [this] {
            if (nullptr == m_srvSettingsWindow) {
                m_srvSettingsWindow = std::make_unique<ServerSettingsWindow>(this);
                updateDockIcon();
            } else {
                windowToFront(m_srvSettingsWindow.get());
            }
        });
        menu.addItem("Plugins", [this] {
            if (nullptr == m_pluginListWindow) {
                m_pluginListWindow = std::make_unique<PluginListWindow>(
                    this, m_server->getPluginList(), Defaults::getConfigFileName(Defaults::ConfigDeadMan));
                updateDockIcon();
            } else {
                windowToFront(m_pluginListWindow.get());
            }
        });
        menu.addSeparator();
        menu.addItem("Statistics", [this] {
            if (nullptr == m_statsWindow) {
                m_statsWindow = std::make_unique<StatisticsWindow>(this);
                updateDockIcon();
            } else {
                windowToFront(m_statsWindow.get());
            }
        });
        menu.addSeparator();
        menu.addItem("Rescan", [this] { restartServer(true); });
        menu.addItem("Wipe Cache & Rescan", [this] {
            m_server->getPluginList().clear();
            m_server->saveKnownPluginList();
            restartServer(true);
        });
    }
    return menu;
}

void App::hidePluginList() {
    traceScope();
    m_pluginListWindow.reset();
    updateDockIcon();
}

void App::hideServerSettings() {
    traceScope();
    m_srvSettingsWindow.reset();
    updateDockIcon();
}

void App::hideStatistics() {
    traceScope();
    m_statsWindow.reset();
    updateDockIcon();
}

void App::showSplashWindow(std::function<void(bool)> onClick) {
    traceScope();
    if (nullptr == m_splashWindow) {
        m_splashWindow = std::make_shared<SplashWindow>();
        updateDockIcon();
    }
    if (onClick) {
        m_splashWindow->onClick = onClick;
    }
}

// called from the server thread
void App::hideSplashWindow(int wait) {
    traceScope();
    auto ptrcpy = m_splashWindow;
    m_splashWindow.reset();
    std::thread([this, ptrcpy, wait] {
        Thread::sleep(wait);
        int step = 10;
        while (step-- > 0) {
            float alpha = 1.0f * (float)step / 10.0f;
            runOnMsgThreadAsync([ptrcpy, alpha] { ptrcpy->setAlpha(alpha); });
            Thread::sleep(40);
        }
        runOnMsgThreadAsync([this, ptrcpy] { updateDockIcon(); });
    }).detach();
}

void App::setSplashInfo(const String& txt) {
    traceScope();
    runOnMsgThreadAsync([this, txt] {
        if (nullptr != m_splashWindow) {
            m_splashWindow->setInfo(txt);
        }
    });
}

}  // namespace e47

#ifndef AG_UNIT_TESTS
// This kicks the whole thing off..
START_JUCE_APPLICATION(e47::App)
#endif
