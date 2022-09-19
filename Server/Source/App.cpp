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
    String fileToScan, pluginId, clientId, error;
    int workerPort = 0, srvId = -1;
    json jconfig;
    bool log = false, isLocal = false, secondRun = false;
    for (int i = 0; i < args.size(); i++) {
        if (!args[i].compare("-scan") && args.size() >= i + 2) {
            fileToScan = args[++i];
            mode = SCAN;
        } else if (!args[i].compare("-server")) {
            mode = SERVER;
        } else if (args[i].startsWith("--" + Defaults::SANDBOX_CMD_PREFIX)) {
            mode = SANDBOX_CHAIN;
        } else if (!args[i].compare("-load")) {
            mode = SANDBOX_PLUGIN;
        } else if (!args[i].compare("-log")) {
            log = true;
        } else if (!args[i].compare("-secondrun")) {
            secondRun = true;
        } else if (!args[i].compare("-islocal") && args.size() >= i + 2) {
            isLocal = args[++i] == "1";
        } else if (!args[i].compare("-pluginid") && args.size() >= i + 2) {
            pluginId = args[++i];
        } else if (!args[i].compare("-clientid") && args.size() >= i + 2) {
            clientId = args[++i];
        } else if (!args[i].compare("-workerport") && args.size() >= i + 2) {
            workerPort = args[++i].getIntValue();
        } else if (!args[i].compare("-id") && args.size() >= i + 2) {
            srvId = args[++i].getIntValue();
        } else if (!args[i].compare("-config") && args.size() >= i + 2) {
            MemoryBlock config;
            if (config.fromBase64Encoding(args[++i])) {
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
    bool linkLatest = true;
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
            linkLatest = false;
            break;
        case SANDBOX_CHAIN:
            appName = "Sandbox-Chain";
            logName = clientId + "_";
            linkLatest = false;
            break;
        case SERVER:
            appName = "Server";
            break;
    }
    Logger::initialize(appName, logName, cfgFile, linkLatest);
    Tracer::initialize(appName, logName, linkLatest);
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
                logln("scan mode: format=" << format << " id=" << id << " srvId=" << srvId);
                bool success = Server::scanPlugin(id, format, srvId > -1 ? srvId : 0, secondRun);
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
        case MASTER: {
#ifdef JUCE_MAC
            Process::setDockIconVisible(false);
            File appState("~/Library/Saved Application State/com.e47.AudioGridderServer.savedState");
            if (appState.exists()) {
                appState.deleteRecursively();
            }
#endif
            Array<int> ids;

            if (srvId < 0) {
                auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigServerStartup));
                if (jsonHasValue(cfg, "IDs")) {
                    for (auto& range : StringArray::fromTokens(jsonGetValue(cfg, "IDs", String()), ",", "")) {
                        if (range.containsChar('-')) {
                            auto parts = StringArray::fromTokens(range, "-", "");
                            if (parts.size() == 2) {
                                int start = parts[0].getIntValue();
                                int end = parts[1].getIntValue();
                                for (int i = start; i <= end; i++) {
                                    ids.add(i);
                                }
                            }
                        } else {
                            ids.add(range.getIntValue());
                        }
                    }
                }
            }

            if (ids.isEmpty()) {
                m_child = std::make_unique<std::thread>([this, srvId] {
                    ChildProcess proc;
                    StringArray procArgs;
                    procArgs.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
                    procArgs.add("-server");
                    if (srvId > -1) {
                        procArgs.add("-id");
                        procArgs.add(String(srvId));
                    }
                    uint32 ec = 0;
                    bool done = false;
                    do {
                        if (proc.start(procArgs)) {
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
            } else {
                Tracer::cleanup();
                Logger::cleanup();
                m_child = std::make_unique<std::thread>([ids, args] {
                    std::vector<ChildProcess> masters((size_t)ids.size());
                    for (int i = 0; i < ids.size(); i++) {
                        int id = ids[i];
                        auto& proc = masters[(size_t)i];
                        StringArray procArgs;
                        procArgs.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
                        procArgs.addArray(args);
                        procArgs.addArray({"-id", String(id)});
                        proc.start(procArgs);
                    }
                    for (int i = 0; i < ids.size(); i++) {
                        auto& proc = masters[(size_t)i];
                        proc.waitForProcessToFinish(-1);
                    }
                    quit();
                });
            }
            break;
        }
    }
    logln("initialise complete");
}

void App::prepareShutdown(uint32 exitCode) {
    traceScope();

    m_exitCode = exitCode;

    if (!m_preparingShutdown.exchange(true)) {
        logln("preparing shutdown");

        std::thread([this] {
            Thread::setCurrentThreadName("ShutdownThread");

            traceScope();

            if (m_server != nullptr) {
                runOnMsgThreadSync([this] {
                    hideEditor();
                    hidePluginList();
                    hideServerSettings();
                });
                m_server->shutdown();
                m_server->waitForThreadToExit(-1);
                m_server.reset();
            }

            quit();
        }).detach();
    } else {
        logln("shutdown initiated already, quitting immediately");
        quit();
    }
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
        hideEditor();
        hidePluginList();
        hideServerSettings();

        m_server->shutdown();
        m_server->waitForThreadToExit(-1);
        m_server.reset();
    }

    logln("exit code = " << String(m_exitCode));

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
void App::showEditorInternal(Thread::ThreadID tid, std::shared_ptr<Processor> proc, T func,
                             std::function<void()> onHide, int x, int y) {
    traceScope();

    if (tid == nullptr) {
        logln("showEditor failed: tid is nullptr");
        return;
    }

    if (proc->hasEditor()) {
        std::lock_guard<std::mutex> lock(m_processorsMtx);

        logln("showing editor: tid=0x" << String::toHexString((uint64)tid));

        if (auto currProc = getCurrentWindowProcInternal(tid)) {
            if (auto currWindow = currProc->getEditorWindow()) {
                currWindow->setVisible(false);
            }
        }

        if (auto window = proc->getOrCreateEditorWindow(tid, func, onHide, x, y)) {
            window->setVisible(true);
            m_processors[(uint64)tid] = proc;
        }

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

void App::showEditor(Thread::ThreadID tid, std::shared_ptr<Processor> proc, ProcessorWindow::CaptureCallbackFFmpeg func,
                     std::function<void()> onHide, int x, int y) {
    showEditorInternal(tid, proc, func, onHide, x, y);
}

void App::showEditor(Thread::ThreadID tid, std::shared_ptr<Processor> proc, ProcessorWindow::CaptureCallbackNative func,
                     std::function<void()> onHide, int x, int y) {
    showEditorInternal(tid, proc, func, onHide, x, y);
}

void App::hideEditor(Thread::ThreadID tid, bool updateMacOSDock) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_processorsMtx);

    if (tid == nullptr) {
        if (!m_processors.empty()) {
            logln("hiding all editors");

            // hide all windows
            for (auto it = m_processors.begin(); it != m_processors.end(); it++) {
                if (auto window = it->second->getEditorWindow()) {
                    if (window->isShowingPlugin()) {
                        window->setVisible(false);
                    }
                }
            }
        }
    } else {
        logln("hiding editor: tid=0x" << String::toHexString((uint64)tid));

        if (auto window = getCurrentWindow(tid)) {
            if (window->isShowingPlugin()) {
                window->setVisible(false);
            } else {
                logln("window not visible");
            }
        } else {
            logln("failed to hide editor: tid does not match a window owner");
        }
    }

#ifdef JUCE_MAC
    if (updateMacOSDock && (getServer()->getSandboxMode() != Server::SANDBOX_PLUGIN ||
                            getServer()->getSandboxModeRuntime() == Server::SANDBOX_PLUGIN)) {
        bool windowVisible = false;
        for (auto& pair : m_processors) {
            if (auto window = pair.second->getEditorWindow()) {
                if (window->isShowingPlugin()) {
                    windowVisible = true;
                    break;
                }
            }
        }
        if (!windowVisible) {
            Process::setDockIconVisible(false);
        }
    }
#else
    ignoreUnused(updateMacOSDock);
#endif
}

void App::bringEditorToFront(Thread::ThreadID tid) {
    traceScope();

    logln("bringing editor to front: tid=0x" << String::toHexString((uint64)tid));

    std::lock_guard<std::mutex> lock(m_processorsMtx);

    if (auto window = getCurrentWindow(tid)) {
        window->toTop();
    } else {
        logln("bringEditorToFront failed: no window for tid");
    }
}

std::shared_ptr<Processor> App::getCurrentWindowProc(Thread::ThreadID tid) {
    std::lock_guard<std::mutex> lock(m_processorsMtx);
    return getCurrentWindowProcInternal(tid);
}

std::shared_ptr<Processor> App::getCurrentWindowProcInternal(Thread::ThreadID tid) {
    auto it = m_processors.find((uint64)tid);
    if (it != m_processors.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<ProcessorWindow> App::getCurrentWindow(Thread::ThreadID tid) {
    if (auto proc = getCurrentWindowProcInternal(tid)) {
        return proc->getEditorWindow();
    }
    return nullptr;
}

void App::moveEditor(Thread::ThreadID tid, int x, int y) {
    traceScope();

    if (getServer()->getScreenLocalMode()) {
        logln("moving editor: tid=0x" << String::toHexString((uint64)tid));

        std::lock_guard<std::mutex> lock(m_processorsMtx);

        if (auto window = getCurrentWindow(tid)) {
            logln("moving editor window to " << x << "x" << y);
            window->move(x, y);
        } else {
            logln("moveEditor failed: no window for tid");
        }
    }
}

void App::resetEditor(Thread::ThreadID tid) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_processorsMtx);

    if (auto proc = getCurrentWindowProcInternal(tid)) {
        proc->resetEditorWindow();
    }
}

void App::restartEditor(Thread::ThreadID tid) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_processorsMtx);

    if (auto proc = getCurrentWindowProcInternal(tid)) {
        logln("recreating processor window");
        proc->recreateEditorWindow();
    } else {
        logln("restartEditor failed: no window for tid");
    }
}

void App::addKeyListener(Thread::ThreadID tid, KeyListener* l) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_processorsMtx);

    if (auto window = getCurrentWindow(tid)) {
        window->addKeyListener(l);
    }
}

void App::updateScreenCaptureArea(Thread::ThreadID tid, int val) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_processorsMtx);

    if (auto proc = getCurrentWindowProcInternal(tid)) {
        if (val != 0) {
            proc->updateScreenCaptureArea(val);
        }
        if (auto window = proc->getEditorWindow()) {
            window->updateScreenCaptureArea();
        }
    }
}

Point<float> App::localPointToGlobal(Thread::ThreadID tid, Point<float> lp) {
    traceScope();

    std::lock_guard<std::mutex> lock(m_processorsMtx);

    if (auto proc = getCurrentWindowProcInternal(tid)) {
        if (auto window = proc->getEditorWindow()) {
            auto ret = window->localPointToGlobal(lp);
            if (!proc->isFullscreen()) {
                ret.y += window->getTitleBarHeight();
            } else {
                if (auto* disp = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                    ret.x -= disp->userArea.getX();
                    ret.y -= disp->userArea.getY();
                }
            }
            if (auto srv = getApp()->getServer()) {
                ret.x += srv->getScreenMouseOffsetX();
                ret.y += srv->getScreenMouseOffsetY();
            }
            return ret;
        } else {
            logln("failed to resolve local to global point: no active window");
        }
    } else {
        logln("failed to resolve local to global point: no active processor");
    }

    return lp;
}

App::ErrorCallback App::getWorkerErrorCallback(Thread::ThreadID tid) {
    std::lock_guard<std::mutex> lock(m_workerErrorCallbacksMtx);

    auto it = m_workerErrorCallbacks.find((uint64)tid);
    if (it != m_workerErrorCallbacks.end()) {
        return it->second;
    }

    return nullptr;
}

void App::setWorkerErrorCallback(Thread::ThreadID tid, ErrorCallback fn) {
    std::lock_guard<std::mutex> lock(m_workerErrorCallbacksMtx);

    if (nullptr != fn) {
        m_workerErrorCallbacks[(uint64)tid] = fn;
    } else {
        m_workerErrorCallbacks.erase((uint64)tid);
    }
}

PopupMenu App::getMenuForIndex(int topLevelMenuIndex, const String& /* menuName */) {
    PopupMenu menu;
    if (topLevelMenuIndex == 0) {  // Settings
        bool enabled = m_splashWindow == nullptr;
        if (auto srv = getApp()->getServer()) {
            String n = srv->getName();
            int id = srv->getId();
            if (id > 0) {
                n << ":" << id;
            }
            menu.addItem(n, false, false, nullptr);
            menu.addSeparator();
        }
        menu.addItem("Settings", enabled, false, [this] {
            if (nullptr == m_srvSettingsWindow) {
                m_srvSettingsWindow = std::make_unique<ServerSettingsWindow>(this);
                updateDockIcon();
            } else {
                windowToFront(m_srvSettingsWindow.get());
            }
        });
        menu.addItem("Plugins", enabled, false, [this] {
            if (nullptr == m_pluginListWindow) {
                m_pluginListWindow = std::make_unique<PluginListWindow>(
                    this, m_server->getPluginList(), Defaults::getConfigFileName(Defaults::ConfigDeadMan));
                updateDockIcon();
            } else {
                windowToFront(m_pluginListWindow.get());
            }
        });
        menu.addSeparator();
        menu.addItem("Statistics", enabled, false, [this] {
            if (nullptr == m_statsWindow) {
                m_statsWindow = std::make_unique<StatisticsWindow>(this);
                updateDockIcon();
            } else {
                windowToFront(m_statsWindow.get());
            }
        });
        menu.addSeparator();
        menu.addItem("Rescan", enabled, false, [this] { restartServer(true); });
        menu.addItem("Wipe Cache & Rescan", enabled, false, [this] {
            m_server->saveKnownPluginList(true);
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

void App::enableCancelScan(int srvId, std::function<void()> onCancel) {
    traceScope();
    runOnMsgThreadAsync([this, srvId, onCancel] {
        if (nullptr != m_splashWindow) {
            m_splashWindow->setOnCancel(srvId, onCancel);
        }
    });
}

void App::disableCancelScan(int srvId) {
    traceScope();
    runOnMsgThreadAsync([this, srvId] {
        if (nullptr != m_splashWindow) {
            m_splashWindow->removeOnCancel(srvId);
        }
    });
}

}  // namespace e47

#ifndef AG_UNIT_TESTS
// This kicks the whole thing off..
START_JUCE_APPLICATION(e47::App)
#endif
