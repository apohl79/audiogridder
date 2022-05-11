/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Server.hpp"
#include "Version.hpp"
#include "App.hpp"
#include "Metrics.hpp"
#include "ServiceResponder.hpp"
#include "CPUInfo.hpp"
#include "WindowPositions.hpp"
#include "ChannelSet.hpp"
#include "Sentry.hpp"
#include "Processor.hpp"

#ifdef JUCE_MAC
#include <sys/socket.h>
#endif

#include <regex>

namespace e47 {

Server::Server(const json& opts) : Thread("Server"), LogTag("server"), m_opts(opts) { initAsyncFunctors(); }

void Server::initialize() {
    traceScope();
    String mode;
    String optMode = getOpt("sandboxMode", String());
    if (optMode.isNotEmpty()) {
        mode = "sandbox";
        m_sandboxModeRuntime = optMode == "chain" ? SANDBOX_CHAIN : optMode == "plugin" ? SANDBOX_PLUGIN : SANDBOX_NONE;
    } else {
        mode = "server";
        File runFile(Defaults::getConfigFileName(Defaults::ConfigServerRun, {{"id", String(getId())}}));
        runFile.create();
    }
    setLogTagName(mode);
    logln("starting " << mode << " (version: " << AUDIOGRIDDER_VERSION << ", build date: " << AUDIOGRIDDER_BUILD_DATE
                      << ")...");
    loadConfig();
    Metrics::initialize();
    CPUInfo::initialize();
    WindowPositions::initialize();

    if (m_sandboxModeRuntime == SANDBOX_NONE) {
        Metrics::getStatistic<TimeStatistic>("audio")->enableExtData(true);
        Metrics::getStatistic<TimeStatistic>("audio")->getMeter().enableExtData(true);
        Metrics::getStatistic<Meter>("NetBytesOut")->enableExtData(true);
        Metrics::getStatistic<Meter>("NetBytesIn")->enableExtData(true);
    }
}

void Server::loadConfig() {
    traceScope();
    auto file = Defaults::getConfigFileName(Defaults::ConfigServer, {{"id", String(getId())}});

#ifndef AG_UNIT_TESTS
    if (!File(file).exists()) {
        file = Defaults::getConfigFileName(Defaults::ConfigServer, {{"id", "0"}});
    }
#endif

    logln("loading config from " << file);
    auto cfg = configParseFile(file);
    Tracer::setEnabled(jsonGetValue(cfg, "Tracer", Tracer::isEnabled()));
    Logger::setEnabled(jsonGetValue(cfg, "Logger", Logger::isEnabled()));
    m_id = jsonGetValue(cfg, "ID", m_id);
    m_name = jsonGetValue(cfg, "NAME", m_name);
#ifdef JUCE_MAC
    m_enableAU = jsonGetValue(cfg, "AU", m_enableAU);
    logln("AudioUnit support " << (m_enableAU ? "enabled" : "disabled"));
#endif
    m_enableVST3 = jsonGetValue(cfg, "VST", m_enableVST3);
    logln("VST3 support " << (m_enableVST3 ? "enabled" : "disabled"));
    m_vst3Folders.clear();
    if (jsonHasValue(cfg, "VST3Folders") && cfg["VST3Folders"].size() > 0) {
        logln("VST3 custom folders:");
        for (auto& s : cfg["VST3Folders"]) {
            if (s.get<std::string>().length() > 0) {
                logln("  " << s.get<std::string>());
                m_vst3Folders.add(s.get<std::string>());
            }
        }
    }
    m_enableVST2 = jsonGetValue(cfg, "VST2", m_enableVST2);
    logln("VST2 support " << (m_enableVST2 ? "enabled" : "disabled"));
    m_vst2Folders.clear();
    if (jsonHasValue(cfg, "VST2Folders") && cfg["VST2Folders"].size() > 0) {
        logln("VST2 custom folders:");
        for (auto& s : cfg["VST2Folders"]) {
            if (s.get<std::string>().length() > 0) {
                logln("  " << s.get<std::string>());
                m_vst2Folders.add(s.get<std::string>());
            }
        }
    }
    m_vstNoStandardFolders = jsonGetValue(cfg, "VSTNoStandardFolders", false);
    logln("include VST standard folders is " << (m_vstNoStandardFolders ? "disabled" : "enabled"));
    m_screenCapturingFFmpeg = jsonGetValue(cfg, "ScreenCapturingFFmpeg", m_screenCapturingFFmpeg);
    String encoder = "webp";
    m_screenCapturingFFmpegEncMode = ScreenRecorder::WEBP;
    //if (jsonHasValue(cfg, "ScreenCapturingFFmpegEncoder")) {
    //    encoder = jsonGetValue(cfg, "ScreenCapturingFFmpegEncoder", encoder);
    //    if (encoder == "webp") {
    //        m_screenCapturingFFmpegEncMode = ScreenRecorder::WEBP;
    //    } else if (encoder == "mjpeg") {
    //        m_screenCapturingFFmpegEncMode = ScreenRecorder::MJPEG;
    //    } else {
    //        logln("unknown ffmpeg encoder mode " << encoder << "! falling back to webp.");
    //        m_screenCapturingFFmpegEncMode = ScreenRecorder::WEBP;
    //        encoder = "webp";
    //    }
    //}
    m_screenCapturingOff = jsonGetValue(cfg, "ScreenCapturingOff", m_screenCapturingOff);
    m_screenCapturingFFmpegQuality = jsonGetValue(cfg, "ScreenCapturingFFmpegQual", m_screenCapturingFFmpegQuality);
    String scmode;
    if (m_screenCapturingOff || m_sandboxModeRuntime == SANDBOX_PLUGIN) {
        scmode = "off";
    } else if (m_screenCapturingFFmpeg) {
        scmode = "ffmpeg (" + encoder + ")";
        runOnMsgThreadAsync([this] {
            traceScope();
            ScreenRecorder::initialize(m_screenCapturingFFmpegEncMode, m_screenCapturingFFmpegQuality);
        });
    } else {
        scmode = "native";
    }
    logln("screen capturing mode: " << scmode);
    m_screenDiffDetection = jsonGetValue(cfg, "ScreenDiffDetection", m_screenDiffDetection);
    if (!m_screenCapturingFFmpeg && !m_screenCapturingOff) {
        logln("screen capture difference detection " << (m_screenDiffDetection ? "enabled" : "disabled"));
    }
    m_screenJpgQuality = jsonGetValue(cfg, "ScreenQuality", m_screenJpgQuality);
    m_screenLocalMode = jsonGetValue(cfg, "ScreenLocalMode", m_screenLocalMode);
    m_pluginWindowsOnTop = jsonGetValue(cfg, "PluginWindowsOnTop", m_pluginWindowsOnTop);
    m_pluginexclude.clear();
    if (jsonHasValue(cfg, "ExcludePlugins")) {
        for (auto& s : cfg["ExcludePlugins"]) {
            m_pluginexclude.insert(s.get<std::string>());
        }
    }
    m_scanForPlugins = jsonGetValue(cfg, "ScanForPlugins", m_scanForPlugins);
    m_crashReporting = jsonGetValue(cfg, "CrashReporting", m_crashReporting);
    logln("crash reporting is " << (m_crashReporting ? "enabled" : "disabled"));
    m_sandboxMode = (SandboxMode)jsonGetValue(cfg, "SandboxMode", m_sandboxMode);
    logln("sandbox mode is " << (m_sandboxMode == SANDBOX_CHAIN    ? "chain isolation"
                                 : m_sandboxMode == SANDBOX_PLUGIN ? "plugin isolation"
                                                                   : "disabled"));
    m_sandboxLogAutoclean = jsonGetValue(cfg, "SandboxLogAutoclean", m_sandboxLogAutoclean);
}

void Server::saveConfig() {
    traceScope();
    json j;
    j["Tracer"] = Tracer::isEnabled();
    j["Logger"] = Logger::isEnabled();
    j["ID"] = getId();
    j["NAME"] = m_name.toStdString();
#ifdef JUCE_MAC
    j["AU"] = m_enableAU;
#endif
    j["VST"] = m_enableVST3;
    j["VST3Folders"] = json::array();
    for (auto& f : m_vst3Folders) {
        j["VST3Folders"].push_back(f.toStdString());
    }
    j["VST2"] = m_enableVST2;
    j["VST2Folders"] = json::array();
    for (auto& f : m_vst2Folders) {
        j["VST2Folders"].push_back(f.toStdString());
    }
    j["VSTNoStandardFolders"] = m_vstNoStandardFolders;
    j["ScreenCapturingFFmpeg"] = m_screenCapturingFFmpeg;
    switch (m_screenCapturingFFmpegEncMode) {
        case ScreenRecorder::WEBP:
            j["ScreenCapturingFFmpegEncoder"] = "webp";
            break;
        case ScreenRecorder::MJPEG:
            j["ScreenCapturingFFmpegEncoder"] = "mjpeg";
            break;
    }
    j["ScreenCapturingFFmpeg"] = m_screenCapturingFFmpeg;
    j["ScreenCapturingOff"] = m_screenCapturingOff;
    j["ScreenCapturingFFmpegQual"] = m_screenCapturingFFmpegQuality;
    j["ScreenQuality"] = m_screenJpgQuality;
    j["ScreenDiffDetection"] = m_screenDiffDetection;
    j["ScreenLocalMode"] = m_screenLocalMode;
    j["PluginWindowsOnTop"] = m_pluginWindowsOnTop;
    j["ExcludePlugins"] = json::array();
    for (auto& p : m_pluginexclude) {
        j["ExcludePlugins"].push_back(p.toStdString());
    }
    j["ScanForPlugins"] = m_scanForPlugins;
    j["CrashReporting"] = m_crashReporting;
    j["SandboxMode"] = m_sandboxMode;
    j["SandboxLogAutoclean"] = m_sandboxLogAutoclean;

    File cfg(Defaults::getConfigFileName(Defaults::ConfigServer, {{"id", String(getId())}}));
    logln("saving config to " << cfg.getFullPathName());
    if (cfg.exists()) {
        cfg.deleteFile();
    }
    cfg.create();
    FileOutputStream fos(cfg);
    fos.writeText(j.dump(4), false, false, "\n");
}

int Server::getId(bool ignoreOpts) const {
    if (ignoreOpts) {
        return m_id;
    }
    return getOpt("ID", m_id);
}

void Server::loadKnownPluginList() {
    traceScope();

    loadKnownPluginList(m_pluginlist, getId());

    std::map<String, PluginDescription> dedupMap;

    for (auto& desc : m_pluginlist.getTypes()) {
        std::unique_ptr<AudioPluginFormat> fmt;

        if (desc.pluginFormatName == "AudioUnit") {
#if JUCE_MAC
            fmt = std::make_unique<AudioUnitPluginFormat>();
#endif
        } else if (desc.pluginFormatName == "VST") {
#if JUCE_PLUGINHOST_VST
            fmt = std::make_unique<VSTPluginFormat>();
#endif
        } else if (desc.pluginFormatName == "VST3") {
            fmt = std::make_unique<VST3PluginFormat>();
        }

        if (nullptr != fmt) {
            auto name = fmt->getNameOfPluginFromIdentifier(desc.fileOrIdentifier);
            if (File(name).exists()) {
                name = File(name).getFileName();
            }
            if (shouldExclude(name)) {
                m_pluginlist.removeType(desc);
            }
        }

        String id = Processor::createPluginID(desc);
        bool updateDedupMap = true;

        auto it = dedupMap.find(id);
        if (it != dedupMap.end()) {
            auto& descExists = it->second;
            if (desc.version.compare(descExists.version) < 0) {
                // existing one is newer, remove current
                updateDedupMap = false;
                m_pluginlist.removeType(desc);
                logln("  info: ignoring " << desc.descriptiveName << " (" << desc.version << ") due to newer version");
            } else {
                // existing one is older, keep current
                m_pluginlist.removeType(descExists);
                logln("  info: ignoring " << descExists.descriptiveName << " (" << descExists.version
                                          << ") due to newer version");
            }
        }

        if (updateDedupMap) {
            dedupMap[id] = desc;
        }
    }

    if (m_sandboxModeRuntime == SANDBOX_NONE) {
        std::set<String> blackList;

        for (int scanId = Defaults::SCAN_ID_START; scanId < Defaults::SCAN_ID_START + Defaults::SCAN_WORKERS;
             scanId++) {
            File deadmanfile(Defaults::getConfigFileName(Defaults::ConfigDeadMan, {{"id", String(scanId)}}));

            if (deadmanfile.existsAsFile()) {
                logln("reading scan crash file " << deadmanfile.getFullPathName());

                StringArray lines;
                deadmanfile.readLines(lines);

                for (auto& line : lines) {
                    blackList.insert(line);
                }

                deadmanfile.deleteFile();
            }
        }

        if (!blackList.empty()) {
            for (auto& p : blackList) {
                logln("  adding " << p << " to blacklist");
                m_pluginlist.addToBlacklist(p);
            }

            saveConfig();
            saveKnownPluginList();
        }
    }
}

void Server::loadKnownPluginList(KnownPluginList& plist, int srvId) {
    setLogTagStatic("server");
    traceScope();

    File file(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", String(srvId)}}));

#ifndef AG_UNIT_TESTS
    if (!file.exists()) {
        file = File(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", "0"}}));
    }
#endif

    if (file.exists()) {
        logln("loading plugins cache from " << file.getFullPathName());
        auto xml = XmlDocument::parse(file);
        plist.recreateFromXml(*xml);
    } else {
        logln("no plugins cache found");
    }
}

void Server::saveKnownPluginList() {
    traceScope();
    saveKnownPluginList(m_pluginlist, getId());
}

void Server::saveKnownPluginList(KnownPluginList& plist, int srvId) {
    setLogTagStatic("server");
    traceScope();

    // dedup blacklist
    std::set<String> blackList;
    for (auto& entry : plist.getBlacklistedFiles()) {
        blackList.insert(entry);
    }
    plist.clearBlacklistedFiles();
    for (auto& entry : blackList) {
        plist.addToBlacklist(entry);
    }

    File file(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", String(srvId)}}));
    logln("writing plugins cache to " << file.getFullPathName());
    auto xml = plist.createXml();
    if (!xml->writeTo(file)) {
        logln("failed to store plugin cache");
    }
}

Server::~Server() {
    traceScope();

    stopAsyncFunctors();

    if (m_sandboxModeRuntime == SANDBOX_NONE) {
        m_masterSocket.close();
    }

    waitForThreadAndLog(this, this);

    m_pluginlist.clear();
    ScreenRecorder::cleanup();
    Metrics::cleanup();
    ServiceResponder::cleanup();
    CPUInfo::cleanup();
    WindowPositions::cleanup();

    logln("server terminated");

    if (m_sandboxModeRuntime == SANDBOX_NONE) {
        logln("removing run file");
        File runFile(Defaults::getConfigFileName(Defaults::ConfigServerRun, {{"id", String(getId())}}));
        runFile.deleteFile();
    }
}

void Server::shutdown() {
    traceScope();

    logln("shutting down server");

    if (m_sandboxModeRuntime == SANDBOX_NONE) {
        m_masterSocket.close();
        m_masterSocketLocal.close();
    }

    signalThreadShouldExit();

    logln("thread signaled");
}

void Server::setName(const String& name) {
    traceScope();
    m_name = name;
    ServiceResponder::setHostName(name);
    logln("setting server name to " << name);
}

void Server::shutdownWorkers() {
    logln("shutting down " << m_workers.size() << " workers");
    for (auto& w : m_workers) {
        logln("shutting down worker " << String::toHexString(w->getTagId())
                                      << ", isRunning=" << (int)w->isThreadRunning());
        w->shutdown();
    }

    logln("waiting for " << m_workers.size() << " workers");
    for (auto& w : m_workers) {
        w->waitForThreadToExit(-1);
    }

    m_workers.clear();
}

bool Server::shouldExclude(const String& name) {
    traceScope();
    return shouldExclude(name, {});
}

bool Server::shouldExclude(const String& name, const std::vector<String>& include) {
    traceScope();
    if (name.containsIgnoreCase("AGridder") || name.containsIgnoreCase("AudioGridder")) {
        return true;
    }
    if (include.size() > 0) {
        for (auto& incl : include) {
            if (!name.compare(incl)) {
                return false;
            }
        }
        return true;
    } else {
        for (auto& excl : m_pluginexclude) {
            if (!name.compare(excl)) {
                return true;
            }
        }
    }
    return false;
}

void Server::addPlugins(const std::vector<String>& names, std::function<void(bool)> fn) {
    traceScope();
    std::thread([this, names, fn] {
        traceScope();
        scanForPlugins(names);
        saveConfig();
        saveKnownPluginList();
        if (fn) {
            for (auto& name : names) {
                bool found = false;
                for (auto& p : m_pluginlist.getTypes()) {
                    if (!name.compare(p.descriptiveName)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    fn(false);
                    return;
                }
            }
            fn(true);
        }
    }).detach();
}

bool Server::scanPlugin(const String& id, const String& format, int srvId) {
    std::unique_ptr<AudioPluginFormat> fmt;
    if (!format.compare("VST")) {
#if JUCE_PLUGINHOST_VST
        fmt = std::make_unique<VSTPluginFormat>();
#else
        return false;
#endif
    } else if (!format.compare("VST3")) {
        fmt = std::make_unique<VST3PluginFormat>();
#if JUCE_MAC
    } else if (!format.compare("AudioUnit")) {
        fmt = std::make_unique<AudioUnitPluginFormat>();
#endif
    } else {
        return false;
    }

    setLogTagStatic("server");
    logln("scanning id=" << id << " fmt=" << format);

    KnownPluginList plist, newlist;
    loadKnownPluginList(plist, srvId);

    File crashFile(Defaults::getConfigFileName(Defaults::ConfigDeadMan, {{"id", String(srvId)}}));

    int retries = 4;
    bool success = false;
    String name;

    do {
        PluginDirectoryScanner scanner(newlist, *fmt, {}, true, crashFile);
        scanner.setFilesOrIdentifiersToScan({id});
        scanner.scanNextFile(true, name);

        if (scanner.getFailedFiles().isEmpty()) {
            success = true;
        } else {
            if (retries == 0) {
                for (auto& f : scanner.getFailedFiles()) {
                    plist.addToBlacklist(f);
                }
            }
            Thread::sleep(1000);
        }
    } while (!success && retries-- > 0);

    for (auto& t : newlist.getTypes()) {
        logln("adding plugin description:");
        logln("  name            = " << t.name << " (" << t.descriptiveName << ")");
        logln("  uniqueId        = " << String::toHexString(t.uniqueId));
        logln("  deprecatedUid   = " << String::toHexString(t.deprecatedUid));
        logln("  id string       = " << Processor::createPluginID(t));
        logln("  manufacturer    = " << t.manufacturerName);
        logln("  category        = " << t.category);
        logln("  shell           = " << (int)t.hasSharedContainer);
        logln("  instrument      = " << (int)t.isInstrument);
        logln("  input channels  = " << t.numInputChannels);
        logln("  output channels = " << t.numOutputChannels);
        plist.addType(t);
    }

    saveKnownPluginList(plist, srvId);

    return success;
}

void Server::scanNextPlugin(const String& id, const String& name, const String& fmt, int srvId) {
    traceScope();
    String fileFmt = id;
    fileFmt << "|" << fmt;
    ChildProcess proc;
    StringArray args;
    args.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
    args.addArray({"-scan", fileFmt});
    args.addArray({"-id", String(srvId)});
    if (proc.start(args)) {
        bool finished;
        do {
            proc.waitForProcessToFinish(30000);
            finished = true;
            if (proc.isRunning()) {
                if (!AlertWindow::showOkCancelBox(
                        AlertWindow::WarningIcon, "Timeout",
                        "The plugin scan for '" + name + "' did not finish yet. Do you want to continue to wait?",
                        "Wait", "Abort")) {
                    logln("error: scan timeout, killing scan process");
                    proc.kill();

                    // blacklist the plugin
                    KnownPluginList plist;
                    loadKnownPluginList(plist, srvId);
                    plist.addToBlacklist(id);
                    saveKnownPluginList(plist, srvId);
                } else {
                    finished = false;
                }
            } else {
                auto ec = proc.getExitCode();
                if (ec != 0) {
                    logln("error: scan failed with exit code " << (int)ec);
                }
            }
        } while (!finished);
    } else {
        logln("error: failed to start scan process");
    }
}

void Server::scanForPlugins() {
    traceScope();
    scanForPlugins({});
}

void Server::scanForPlugins(const std::vector<String>& include) {
    traceScope();
    logln("scanning for plugins...");
    std::vector<std::unique_ptr<AudioPluginFormat>> fmts;
#if JUCE_MAC
    if (m_enableAU) {
        fmts.push_back(std::make_unique<AudioUnitPluginFormat>());
    }
#endif
    if (m_enableVST3) {
        fmts.push_back(std::make_unique<VST3PluginFormat>());
    }
#if JUCE_PLUGINHOST_VST
    if (m_enableVST2) {
        fmts.push_back(std::make_unique<VSTPluginFormat>());
    }
#endif

    std::set<String> neverSeenList = m_pluginexclude;

    loadKnownPluginList();

    struct ScanThread : FnThread {
        int id;
    };

    int scanId = Defaults::SCAN_ID_START;
    std::vector<ScanThread> scanThreads(Defaults::SCAN_WORKERS);
    for (auto& t : scanThreads) {
        t.id = scanId++;
    }

    for (auto& fmt : fmts) {
        FileSearchPath searchPaths;
        if (fmt->getName().compare("AudioUnit") && !m_vstNoStandardFolders) {
            searchPaths = fmt->getDefaultLocationsToSearch();
        }
        if (!fmt->getName().compare("VST3")) {
            for (auto& f : m_vst3Folders) {
                searchPaths.addIfNotAlreadyThere(f);
            }
        } else if (!fmt->getName().compare("VST")) {
            for (auto& f : m_vst2Folders) {
                searchPaths.addIfNotAlreadyThere(f);
            }
        }
        searchPaths.removeRedundantPaths();
        searchPaths.removeNonExistentPaths();
        auto fileOrIds = fmt->searchPathsForPlugins(searchPaths, true);
        for (auto& fileOrId : fileOrIds) {
            auto name = fmt->getNameOfPluginFromIdentifier(fileOrId);
            auto plugindesc = m_pluginlist.getTypeForFile(fileOrId);
            if (File(name).exists()) {
                name = File(name).getFileName();
            }
            bool excluded = shouldExclude(name, include);
            if ((nullptr == plugindesc || fmt->pluginNeedsRescanning(*plugindesc)) &&
                !m_pluginlist.getBlacklistedFiles().contains(fileOrId) && !excluded) {
                ScanThread* scanThread = nullptr;
                do {
                    for (auto& t : scanThreads) {
                        if (!t.isThreadRunning()) {
                            scanThread = &t;
                            break;
                        }
                        sleep(5);
                    }
                } while (scanThread == nullptr);

                logln("  scanning: " << name);
                String splashName = name;
                if (fmt->getName().compare("AudioUnit")) {
                    File f(name);
                    if (f.exists()) {
                        splashName = f.getFileName();
                    }
                }

                getApp()->setSplashInfo(String("Scanning ") + fmt->getName() + ": " + splashName + " ...");

                scanThread->fn = [this, fileOrId, pluginName = name, fmtName = fmt->getName(), srvId = scanThread->id] {
                    scanNextPlugin(fileOrId, pluginName, fmtName, srvId);
                };
                scanThread->startThread();
            } else {
                logln("  (skipping: " << name << (excluded ? " excluded" : "") << ")");
            }
            neverSeenList.erase(name);
        }
    }

    std::set<String> newBlacklistedPlugins;

    for (auto& t : scanThreads) {
        while (t.isThreadRunning()) {
            sleep(50);
        }

        File file(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", String(t.id)}}));

        if (file.existsAsFile()) {
            KnownPluginList plist;
            loadKnownPluginList(plist, t.id);

            for (auto& p : plist.getBlacklistedFiles()) {
                if (!m_pluginlist.getBlacklistedFiles().contains(p)) {
                    m_pluginlist.addToBlacklist(p);
                    String name;
                    File f(p);
                    if (f.exists()) {
                        name = f.getFileNameWithoutExtension() + " (" +
                               f.getFileExtension().toUpperCase().substring(1) + ")";
#if JUCE_MAC
                    } else if (p.startsWith("AudioUnit")) {
                        AudioUnitPluginFormat fmt;
                        name = fmt.getNameOfPluginFromIdentifier(p) + " (AU)";
#endif
                    } else {
                        name = p;
                    }
                    newBlacklistedPlugins.insert(name);
                }
            }
            for (auto p : plist.getTypes()) {
                m_pluginlist.addType(p);
            }

            file.deleteFile();
        }
    }

    m_pluginlist.sort(KnownPluginList::sortAlphabetically, true);

    getApp()->setSplashInfo("Scanning finished.");

    for (auto& name : neverSeenList) {
        m_pluginexclude.erase(name);
    }
    logln("scan for plugins finished.");

    if (!newBlacklistedPlugins.empty()) {
        StringArray showList;
        for (auto& p : newBlacklistedPlugins) {
            showList.add(p);
            if (showList.size() == 10) {
                break;
            }
        }
        String msg = "The following plugins failed during the plaugin scan:";
        msg << newLine << newLine;
        msg << showList.joinIntoString(newLine);
        msg << newLine;
        if (newBlacklistedPlugins.size() > 10) {
            msg << "... " << (newBlacklistedPlugins.size() - 10) << " more plugin(s)." << newLine;
        }
        msg << newLine << newLine;
        msg << "You can force a rescan via Plugin Manager.";
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Failed Plugins", msg, "OK");
    }
}

void Server::run() {
    switch (m_sandboxModeRuntime) {
        case SANDBOX_CHAIN:
            runSandboxChain();
            break;
        case SANDBOX_PLUGIN:
            runSandboxPlugin();
            break;
        case SANDBOX_NONE:
            runServer();
            break;
    }
}

void Server::checkPort() {
    int port = m_port + getId();
    ChildProcess proc;
    StringArray args;
#ifdef JUCE_MAC
    // Waves is spawning a WavesLocalServer on macOS, that inherits the AG master socket and thus blocks the AG
    // server port. Trying to automatically kill any process, that binds to the AG server port before creating the
    // master socket.
    args.add("lsof");
    args.add("-nP");
    args.add("-iTCP:" + String(port));
    if (proc.start(args, ChildProcess::wantStdOut)) {
        std::regex re("^[^ ]+ +(\\d+) +.*\\(LISTEN\\)$");
        std::smatch match;
        auto out = proc.readAllProcessOutput();
        for (auto& line : StringArray::fromLines(out)) {
            auto lineStd = line.toStdString();
            if (std::regex_search(lineStd, match, re)) {
                auto pid = String(match[1]);
                logln("about to kill process " << pid << " that blocks server port " << port);
                ChildProcess kproc;
                kproc.start("kill " + pid);
            }
        }
        sleep(3000);
    }
#elif JUCE_WINDOWS
    args.add("powershell");
    args.add("-Command");
    args.add("& {(Get-NetTCPConnection -LocalPort " + String(port) + ").OwningProcess}");
    if (proc.start(args, ChildProcess::wantStdOut)) {
        auto out = proc.readAllProcessOutput().trim();
        if (out.isNotEmpty() && out.containsOnly("0123456789")) {
            int pid = out.getIntValue();
            logln("about to kill process " << pid << " that blocks server port " << port);
            ChildProcess kproc;
            kproc.start("taskkill /T /F /PID " + String(pid));
        }
        sleep(3000);
    }
#endif
}

void Server::runServer() {
    traceScope();
    if ((m_scanForPlugins || getOpt("ScanForPlugins", false)) && !getOpt("NoScanForPlugins", false)) {
        scanForPlugins();
    } else {
        loadKnownPluginList();
        m_pluginlist.sort(KnownPluginList::sortAlphabetically, true);
    }
    saveConfig();
    saveKnownPluginList();

    if (m_crashReporting) {
        Sentry::initialize();
    }

    for (auto& type : m_pluginlist.getTypes()) {
        if ((type.pluginFormatName == "AudioUnit" && !m_enableAU) ||
            (type.pluginFormatName == "VST" && !m_enableVST2) || (type.pluginFormatName == "VST3" && !m_enableVST3)) {
            m_pluginlist.removeType(type);
        }
    }

    getApp()->hideSplashWindow(1000);

#ifndef JUCE_WINDOWS
    setsockopt(m_masterSocket.getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

    setNonBlocking(m_masterSocket.getRawSocketHandle());

    ServiceResponder::initialize(m_port + getId(), getId(), m_name, getScreenLocalMode());

    if (m_name.isEmpty()) {
        m_name = ServiceResponder::getHostName();
        saveConfig();
    }

    logln("available plugins:");
    for (auto& desc : m_pluginlist.getTypes()) {
        logln("  " << desc.name << " [" << Processor::createPluginID(desc) << ", "
                   << Processor::createPluginIDDepricated(desc) << "]"
                   << " version=" << desc.version << " format=" << desc.pluginFormatName
                   << " ins=" << desc.numInputChannels << " outs=" << desc.numOutputChannels
                   << " instrument=" << (int)desc.isInstrument);
    }

    m_sandboxDeleter = std::make_unique<SandboxDeleter>();

    checkPort();

    if (getScreenLocalMode() && Defaults::unixDomainSocketsSupported()) {
        auto socketPath = Defaults::getSocketPath(Defaults::SERVER_SOCK, {{"id", String(getId())}}, true);
        logln("creating listener " << socketPath.getFullPathName());
        if (!m_masterSocketLocal.createListener(socketPath)) {
            logln("failed to create local master listener");
        }
    }

    logln("creating listener " << (m_host.length() == 0 ? "*" : m_host) << ":" << (m_port + getId()));
    if (m_masterSocket.createListener6(m_port + getId(), m_host)) {
        logln("server started: ID=" << getId() << ", PORT=" << m_port + getId() << ", NAME=" << m_name);
        while (!threadShouldExit()) {
            StreamingSocket* clnt = nullptr;
            bool isLocal = false;
            if (m_masterSocketLocal.isConnected()) {
                if ((clnt = accept(&m_masterSocketLocal, 100, [this] { return threadShouldExit(); }))) {
                    isLocal = true;
                }
            }
            if (nullptr == clnt && !threadShouldExit()) {
                clnt = accept(&m_masterSocket, 100, [this] { return threadShouldExit(); });
            }
            if (nullptr != clnt) {
                HandshakeRequest cfg;
                int len = clnt->read(&cfg, sizeof(cfg), true);
                bool handshakeOk = true;
                if (len > 0) {
                    if (cfg.version >= AG_PROTOCOL_VERSION) {
                        logln("new client " << clnt->getHostName());
                        logln("  version                   = " << cfg.version);
                        logln("  clientId                  = " << String::toHexString(cfg.clientId));
                        logln("  channelsIn                = " << cfg.channelsIn);
                        logln("  channelsOut               = " << cfg.channelsOut);
                        logln("  channelsSC                = " << cfg.channelsSC);

                        ChannelSet activeChannels(cfg.activeChannels, cfg.channelsIn > 0);
                        activeChannels.setNumChannels(cfg.channelsIn + cfg.channelsSC, cfg.channelsOut);
                        String active;
                        if (cfg.channelsIn > 0) {
                            active << "inputs: ";
                            if (activeChannels.isInputRangeActive()) {
                                active << "all";
                            } else {
                                bool first = true;
                                for (auto ch : activeChannels.getActiveChannels(true)) {
                                    active << (first ? "" : ",") << ch;
                                    first = false;
                                }
                            }
                            active << " ";
                        }
                        active << "outputs: ";
                        if (activeChannels.isOutputRangeActive()) {
                            active << "all";
                        } else {
                            bool first = true;
                            for (auto ch : activeChannels.getActiveChannels(false)) {
                                active << (first ? "" : ",") << ch;
                                first = false;
                            }
                        }
                        logln("  active channels           = " << active);

                        logln("  rate                      = " << cfg.sampleRate);
                        logln("  samplesPerBlock           = " << cfg.samplesPerBlock);
                        logln("  doublePrecission          = " << static_cast<int>(cfg.doublePrecission));
                        logln("  flags.NoPluginListFilter  = "
                              << (int)cfg.isFlag(HandshakeRequest::NO_PLUGINLIST_FILTER));
                    } else {
                        logln("client " << clnt->getHostName() << " with old protocol version");
                        handshakeOk = false;
                    }
                } else {
                    handshakeOk = false;
                }

                if (!handshakeOk) {
                    clnt->close();
                    delete clnt;
                    continue;
                }

                if (m_sandboxMode == SANDBOX_CHAIN) {
                    // Spawn a sandbox child process for a new client and tell the client the port to connect to
                    int num = 0;
                    String id = String::toHexString(cfg.clientId) + "-" + String(num);
                    while (m_sandboxes.contains(id)) {
                        num++;
                        id = String::toHexString(cfg.clientId) + "-" + String(num);
                    }
                    auto sandbox = std::make_shared<SandboxMaster>(*this, id);
                    logln("creating sandbox " << id);
                    if (sandbox->launchWorkerProcess(
                            File::getSpecialLocation(File::currentExecutableFile), Defaults::SANDBOX_CMD_PREFIX,
                            {"-id", String(getId()), "-islocal", String((int)isLocal), "-clientid", id}, 3000, 30000)) {
                        sandbox->onPortReceived = [this, id, clnt](int sandboxPort) {
                            traceScope();
                            if (!sendHandshakeResponse(clnt, true, sandboxPort)) {
                                logln("failed to send handshake response for sandbox " << id);
                                m_sandboxes.remove(id);
                            }
                            clnt->close();
                            delete clnt;
                        };
                        if (sandbox->send(SandboxMessage(SandboxMessage::CONFIG, cfg.toJson()), nullptr, true)) {
                            m_sandboxes.set(id, std::move(sandbox));
                        } else {
                            logln("failed to send message to sandbox");
                        }
                    } else {
                        logln("failed to launch sandbox");
                    }
                } else {
                    auto workerMasterSocket = std::make_shared<StreamingSocket>();

#ifndef JUCE_WINDOWS
                    setsockopt(workerMasterSocket->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

                    int workerPort = 0;

                    if (!createWorkerListener(workerMasterSocket, isLocal, workerPort)) {
                        logln("failed to create client listener");
                        clnt->close();
                        delete clnt;
                        clnt = nullptr;
                        continue;
                    }

                    // Create a new worker thread for a new client
                    logln("creating worker");
                    if (!sendHandshakeResponse(clnt, false, workerPort)) {
                        logln("failed to send handshake response");
                        clnt->close();
                        delete clnt;
                        continue;
                    }

                    clnt->close();
                    delete clnt;

                    auto w = std::make_shared<Worker>(workerMasterSocket, cfg);
                    w->startThread();
                    m_workers.add(w);
                    // lazy cleanup
                    std::shared_ptr<WorkerList> deadWorkers = std::make_shared<WorkerList>();
                    for (int i = 0; i < m_workers.size();) {
                        if (!m_workers.getReference(i)->isThreadRunning()) {
                            deadWorkers->add(m_workers.getReference(i));
                            m_workers.remove(i);
                        } else {
                            i++;
                        }
                    }
                    traceln("about to remove " << deadWorkers->size() << " dead workers");
                    deadWorkers->clear();
                }
            }
        }

        shutdownWorkers();

        if (m_sandboxes.size() > 0) {
            for (auto sandbox : m_sandboxes) {
                m_sandboxDeleter->add(sandbox);
            }
            m_sandboxes.clear();
        }
    } else {
        logln("failed to create master listener");
        runOnMsgThreadAsync([this] {
            traceScope();
            uint32 ec = App::EXIT_OK;
            if (AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "Error",
                                             "AudioGridder failed to bind to the server port " +
                                                 String(m_port + getId()) +
                                                 "!\n\nYou have to terminate the application that is blocking the port."
                                                 "\n\nTry again?",
                                             "Yes", "No")) {
                ec = App::EXIT_RESTART;
                logln("restarting server by user choice");
            }
            getApp()->prepareShutdown(ec);
        });
    }

    logln("waiting for sandboxes to terminate");
    m_sandboxDeleter->stopThread(-1);
}

void Server::runSandboxChain() {
    traceScope();

    if (m_crashReporting) {
        Sentry::initialize();
    }

    m_sandboxController = std::make_unique<SandboxSlave>(*this);

    if (!m_sandboxController->initialiseFromCommandLine(getOpt("commandLine", String()), Defaults::SANDBOX_CMD_PREFIX,
                                                        10000, 30000)) {
        logln("failed to initialize sandbox process");
        getApp()->prepareShutdown(App::EXIT_SANDBOX_INIT_ERROR);
        return;
    }

    auto workerMasterSocket = std::make_shared<StreamingSocket>();

#ifndef JUCE_WINDOWS
    setsockopt(workerMasterSocket->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

    if (!createWorkerListener(workerMasterSocket, getOpt("isLocal", false), m_port)) {
        logln("failed to create worker listener");
        getApp()->prepareShutdown(App::EXIT_SANDBOX_BIND_ERROR);
        return;
    }

    int waitForMasterSteps = 6000;
    while (!m_sandboxConnectedToMaster) {
        sleep(10);
        if (--waitForMasterSteps <= 0) {
            logln("giving up on waiting for master connection");
            getApp()->prepareShutdown(App::EXIT_SANDBOX_NO_MASTER);
            return;
        }
    }

    if (!m_sandboxController->send(SandboxMessage(SandboxMessage::SANDBOX_PORT, {{"port", m_port}}), nullptr, true)) {
        logln("failed to send sendbox port");
        getApp()->prepareShutdown();
        return;
    }

    loadKnownPluginList();
    m_pluginlist.sort(KnownPluginList::sortAlphabetically, true);

    logln("sandbox (chain isolation) started: PORT=" << m_port << ", NAME=" << m_name);

    while (!m_sandboxReady && !threadShouldExit()) {
        // wait for the master to send the config
        sleep(10);
    }

    if (!threadShouldExit()) {
        logln("creating worker");
        auto w = std::make_shared<Worker>(workerMasterSocket, m_sandboxConfig);
        w->startThread();
        if (w->isThreadRunning()) {
            m_workers.add(w);

            auto audioTime = Metrics::getStatistic<TimeStatistic>("audio");
            auto bytesOutMeter = Metrics::getStatistic<Meter>("NetBytesOut");
            auto bytesInMeter = Metrics::getStatistic<Meter>("NetBytesIn");

            while (!w->waitForThreadToExit(1000) && !threadShouldExit()) {
                json jmetrics;
                jmetrics["LoadedCount"] = Processor::loadedCount.load();
                jmetrics["NetBytesOut"] = bytesOutMeter->rate_1min();
                jmetrics["NetBytesIn"] = bytesInMeter->rate_1min();
                jmetrics["RPS"] = audioTime->getMeter().rate_1min();
                json jtimes = json::array();
                for (auto& hist : audioTime->get1minValues()) {
                    jtimes.push_back(hist.toJson());
                }
                jmetrics["audio"] = jtimes;
                m_sandboxController->send(SandboxMessage(SandboxMessage::METRICS, jmetrics), nullptr, true);
            }

            shutdownWorkers();
        } else {
            logln("failed to start worker thread");
        }
    }

    logln("terminating sandbox connection to master");
    if (nullptr != m_sandboxController) {
        auto* deleter = m_sandboxController.release();
        std::thread([deleter] { delete deleter; }).detach();
    }

    logln("run finished");

    if (!threadShouldExit()) {
        getApp()->prepareShutdown();
    }
}

void Server::runSandboxPlugin() {
    auto workerMasterSocket = std::make_shared<StreamingSocket>();

#ifndef JUCE_WINDOWS
    setsockopt(workerMasterSocket->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

    if (!jsonHasValue(m_opts, "config")) {
        logln("missing parameter config");
        getApp()->prepareShutdown(App::EXIT_SANDBOX_PARAM_ERROR);
        return;
    }

    m_port = getOpt("workerPort", 0);

    if (m_port == 0) {
        logln("missing parameter workerPort");
        getApp()->prepareShutdown(App::EXIT_SANDBOX_PARAM_ERROR);
        return;
    }

    bool hasUnixDomainSockets = Defaults::unixDomainSocketsSupported();
    File socketPath;

    if (hasUnixDomainSockets) {
        socketPath = Defaults::getSocketPath(Defaults::SANDBOX_PLUGIN_SOCK, {{"n", String(m_port)}}, true);
        if (!workerMasterSocket->createListener(socketPath)) {
            logln("failed to create worker listener");
            getApp()->prepareShutdown(App::EXIT_SANDBOX_BIND_ERROR);
            return;
        }
    } else {
        if (!workerMasterSocket->createListener6(m_port, m_host)) {
            logln("failed to create worker listener");
            getApp()->prepareShutdown(App::EXIT_SANDBOX_BIND_ERROR);
            return;
        }
    }

    loadKnownPluginList();
    m_pluginlist.sort(KnownPluginList::sortAlphabetically, true);

    logln("sandbox (plugin isolation) started: "
          << (hasUnixDomainSockets ? "PATH=" + socketPath.getFullPathName() : "PORT=" + String(m_port))
          << ", NAME=" << m_name);

    m_sandboxConfig.fromJson(m_opts["config"]);

    logln("creating worker");
    auto w = std::make_shared<Worker>(workerMasterSocket, m_sandboxConfig, m_sandboxModeRuntime);
    w->startThread();
    if (w->isThreadRunning()) {
        m_workers.add(w);

        while (w->isThreadRunning() && !threadShouldExit()) {
            sleepExitAware(100);
        }

        shutdownWorkers();
    } else {
        logln("failed to start worker thread");
    }

    logln("run finished");

    if (!threadShouldExit()) {
        getApp()->prepareShutdown();
    }
}

bool Server::sendHandshakeResponse(StreamingSocket* sock, bool sandboxEnabled, int port) {
    HandshakeResponse resp = {AG_PROTOCOL_VERSION, 0, 0};
    if (sandboxEnabled) {
        resp.setFlag(HandshakeResponse::SANDBOX_ENABLED);
    }
    if (m_screenLocalMode) {
        resp.setFlag(HandshakeResponse::LOCAL_MODE);
    }
    resp.port = port;
    return send(sock, reinterpret_cast<const char*>(&resp), sizeof(resp));
}

bool Server::createWorkerListener(std::shared_ptr<StreamingSocket> sock, bool isLocal, int& workerPort) {
    int workerPortMax = getOpt("workerPortMax", Defaults::CLIENT_PORT + 1000);
    workerPort = getOpt("workerPort", Defaults::CLIENT_PORT);
    if (isLocal) {
        File socketPath;
        do {
            socketPath =
                Defaults::getSocketPath(Defaults::WORKER_SOCK, {{"id", String(getId())}, {"n", String(workerPort)}});
        } while (socketPath.exists() && ++workerPort <= workerPortMax);
        if (!socketPath.exists()) {
            sock->createListener(socketPath);
        }
    } else {
        while (!sock->createListener6(workerPort, m_host)) {
            if (++workerPort > workerPortMax) {
                break;
            }
        }
    }
    return sock->isConnected();
}

void Server::handleMessageFromSandbox(SandboxMaster& sandbox, const SandboxMessage& msg) {
    if (msg.type == SandboxMessage::SHOW_EDITOR) {
        if (!m_screenLocalMode && m_sandboxHasScreen.isNotEmpty() && m_sandboxHasScreen != sandbox.id &&
            m_sandboxes.contains(m_sandboxHasScreen)) {
            m_sandboxes.getReference(m_sandboxHasScreen)->send(SandboxMessage(SandboxMessage::HIDE_EDITOR, {}));
        }
        m_sandboxHasScreen = sandbox.id;
    } else if (msg.type == SandboxMessage::HIDE_EDITOR && m_sandboxHasScreen == sandbox.id) {
        m_sandboxHasScreen.clear();
    } else if (msg.type == SandboxMessage::METRICS) {
        std::vector<TimeStatistic::Histogram> hists;

        if (msg.data.find("audio") != msg.data.end()) {
            for (auto& hist : msg.data["audio"]) {
                hists.emplace_back(hist);
            }
        }

        updateSandboxNetworkStats(sandbox.id, jsonGetValue(msg.data, "LoadedCount", (uint32)0),
                                  jsonGetValue(msg.data, "NetBytesIn", 0.0), jsonGetValue(msg.data, "NetBytesOut", 0.0),
                                  jsonGetValue(msg.data, "RPS", 0.0), hists);
    } else {
        logln("received unhandled message from sandbox " << sandbox.id);
    }
}

void Server::handleDisconnectFromSandbox(SandboxMaster& sandbox) {
    if (m_sandboxes.contains(sandbox.id)) {
        logln("disconnected from sandbox " << sandbox.id);
        m_sandboxLoadedCount.remove(sandbox.id);
        Metrics::getStatistic<TimeStatistic>("audio")->removeExt1minValues(sandbox.id);
        Metrics::getStatistic<TimeStatistic>("audio")->getMeter().removeExtRate1min(sandbox.id);
        Metrics::getStatistic<Meter>("NetBytesOut")->removeExtRate1min(sandbox.id);
        Metrics::getStatistic<Meter>("NetBytesIn")->removeExtRate1min(sandbox.id);
        auto deleter = m_sandboxes[sandbox.id];
        m_sandboxes.remove(sandbox.id);
        m_sandboxDeleter->add(std::move(deleter));
    }
}

void Server::handleConnectedToMaster() {
    logln("connected to sandbox master");
    m_sandboxConnectedToMaster = true;
}

void Server::handleDisconnectedFromMaster() {
    if (m_sandboxConnectedToMaster.exchange(false)) {
        logln("disconnected from sandbox master");
        signalThreadShouldExit();
        getApp()->prepareShutdown();
    }
}

void Server::handleMessageFromMaster(const SandboxMessage& msg) {
    if (msg.type == SandboxMessage::CONFIG) {
        logln("config message from sandbox master: " << msg.data.dump());
        m_sandboxConfig.fromJson(msg.data);
        m_sandboxReady = true;
    } else if (msg.type == SandboxMessage::HIDE_EDITOR) {
        if (m_workers.size() > 0) {
            auto m = std::make_shared<Message<HidePlugin>>();
            m_workers.getReference(0)->handleMessage(m, true);
        }
    } else {
        logln("received unhandled message from master");
    }
}

void Server::sandboxShowEditor() {
    if (m_sandboxModeRuntime == SANDBOX_CHAIN && nullptr != m_sandboxController) {
        m_sandboxController->send(SandboxMessage(SandboxMessage::SHOW_EDITOR, {}), nullptr, true);
    }
}

void Server::sandboxHideEditor() {
    if (m_sandboxModeRuntime == SANDBOX_CHAIN && nullptr != m_sandboxController) {
        m_sandboxController->send(SandboxMessage(SandboxMessage::HIDE_EDITOR, {}));
    }
}

void Server::updateSandboxNetworkStats(const String& key, uint32 loaded, double bytesIn, double bytesOut, double rps,
                                       const std::vector<TimeStatistic::Histogram>& audioHists) {
    m_sandboxLoadedCount.set(key, loaded);
    Metrics::getStatistic<Meter>("NetBytesIn")->updateExtRate1min(key, bytesIn);
    Metrics::getStatistic<Meter>("NetBytesOut")->updateExtRate1min(key, bytesOut);
    Metrics::getStatistic<TimeStatistic>("audio")->getMeter().updateExtRate1min(key, rps);
    Metrics::getStatistic<TimeStatistic>("audio")->updateExt1minValues(key, audioHists);
}

}  // namespace e47
