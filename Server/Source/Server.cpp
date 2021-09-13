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

#ifdef JUCE_MAC
#include <sys/socket.h>
#endif

namespace e47 {

Server::Server(json opts) : Thread("Server"), LogTag("server"), m_opts(opts) { initAsyncFunctors(); }

void Server::initialize() {
    traceScope();
    String mode;
    if (getOpt("sandboxMode", false)) {
        mode = "sandbox";
    } else {
        mode = "server";
        File runFile(Defaults::getConfigFileName(Defaults::ConfigServerRun));
        runFile.create();
    }
    setLogTagName(mode);
    logln("starting " << mode << " (version: " << AUDIOGRIDDER_VERSION << ", build date: " << AUDIOGRIDDER_BUILD_DATE
                      << ")...");
    loadConfig();
    Metrics::initialize();
    CPUInfo::initialize();
    WindowPositions::initialize();

    if (!getOpt("sandboxMode", false)) {
        Metrics::getStatistic<TimeStatistic>("audio")->enableExtData(true);
        Metrics::getStatistic<TimeStatistic>("audio")->getMeter().enableExtData(true);
        Metrics::getStatistic<Meter>("NetBytesOut")->enableExtData(true);
        Metrics::getStatistic<Meter>("NetBytesIn")->enableExtData(true);
    }
}

void Server::loadConfig() {
    traceScope();
    logln("loading config");
    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigServer));
    Tracer::setEnabled(jsonGetValue(cfg, "Tracer", Tracer::isEnabled()));
    AGLogger::setEnabled(jsonGetValue(cfg, "Logger", AGLogger::isEnabled()));
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
    String encoder;
    if (jsonHasValue(cfg, "ScreenCapturingFFmpegEncoder")) {
        encoder = jsonGetValue(cfg, "ScreenCapturingFFmpegEncoder", encoder);
        if (encoder == "webp") {
            m_screenCapturingFFmpegEncMode = ScreenRecorder::WEBP;
        } else if (encoder == "mjpeg") {
            m_screenCapturingFFmpegEncMode = ScreenRecorder::MJPEG;
        } else {
            logln("unknown ffmpeg encoder mode " << encoder << "! falling back to webp.");
            m_screenCapturingFFmpegEncMode = ScreenRecorder::WEBP;
            encoder = "webp";
        }
    }
    m_screenCapturingOff = jsonGetValue(cfg, "ScreenCapturingOff", m_screenCapturingOff);
    m_screenCapturingFFmpegQuality = jsonGetValue(cfg, "ScreenCapturingFFmpegQual", m_screenCapturingFFmpegQuality);
    String scmode;
    if (m_screenCapturingOff) {
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
    m_parallelPluginLoad = jsonGetValue(cfg, "ParallelPluginLoad", m_parallelPluginLoad);
    m_crashReporting = jsonGetValue(cfg, "CrashReporting", m_crashReporting);
    m_sandboxing = jsonGetValue(cfg, "Sandboxing", m_sandboxing);
    m_sandboxLogAutoclean = jsonGetValue(cfg, "SandboxLogAutoclean", m_sandboxLogAutoclean);
}

void Server::saveConfig() {
    traceScope();
    logln("saving config");
    json j;
    j["Tracer"] = Tracer::isEnabled();
    j["Logger"] = AGLogger::isEnabled();
    j["ID"] = m_id;
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
    j["ParallelPluginLoad"] = m_parallelPluginLoad;
    j["CrashReporting"] = m_crashReporting;
    j["Sandboxing"] = m_sandboxing;
    j["SandboxLogAutoclean"] = m_sandboxLogAutoclean;

    File cfg(Defaults::getConfigFileName(Defaults::ConfigServer));
    if (cfg.exists()) {
        cfg.deleteFile();
    } else {
        cfg.create();
    }
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
    loadKnownPluginList(m_pluginlist);
    std::map<String, PluginDescription> dedupMap;
    for (auto& desc : m_pluginlist.getTypes()) {
        std::unique_ptr<AudioPluginFormat> fmt;
        if (desc.pluginFormatName == "AudioUnit") {
#ifdef JUCE_MAC
            fmt = std::make_unique<AudioUnitPluginFormat>();
#endif
        } else if (desc.pluginFormatName == "VST") {
            fmt = std::make_unique<VSTPluginFormat>();
        } else if (desc.pluginFormatName == "VST3") {
            fmt = std::make_unique<VST3PluginFormat>();
        }
        if (nullptr != fmt) {
            auto name = fmt->getNameOfPluginFromIdentifier(desc.fileOrIdentifier);
            if (shouldExclude(name)) {
                m_pluginlist.removeType(desc);
            }
        }
        String id = AGProcessor::createPluginID(desc);
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
    File deadmanfile(Defaults::getConfigFileName(Defaults::ConfigDeadMan));
    if (deadmanfile.exists()) {
        StringArray lines;
        deadmanfile.readLines(lines);
        for (auto& line : lines) {
            logln("  adding " << line << " to blacklist");
            m_pluginlist.addToBlacklist(line);
        }
        deadmanfile.deleteFile();
        saveConfig();
        saveKnownPluginList();
    }
}

void Server::loadKnownPluginList(KnownPluginList& plist) {
    setLogTagStatic("server");
    traceScope();
    File file(Defaults::getConfigFileName(Defaults::ConfigPluginCache));
    if (file.exists()) {
        auto xml = XmlDocument::parse(file);
        plist.recreateFromXml(*xml);
    }
}

void Server::saveKnownPluginList() {
    traceScope();
    saveKnownPluginList(m_pluginlist);
}

void Server::saveKnownPluginList(KnownPluginList& plist) {
    setLogTagStatic("server");
    traceScope();
    File file(Defaults::getConfigFileName(Defaults::ConfigPluginCache));
    auto xml = plist.createXml();
    if (!xml->writeTo(file)) {
        logln("failed to store plugin cache");
    }
}

Server::~Server() {
    traceScope();
    stopAsyncFunctors();
    if (!getOpt("sandboxMode", false)) {
        m_masterSocket.close();
    }
    waitForThreadAndLog(this, this);
    m_pluginlist.clear();
    Metrics::cleanup();
    ServiceResponder::cleanup();
    CPUInfo::cleanup();
    WindowPositions::cleanup();
    logln("server terminated");
    if (!getOpt("sandboxMode", false)) {
        logln("removing run file");
        File runFile(Defaults::getConfigFileName(Defaults::ConfigServerRun));
        runFile.deleteFile();
    }
}

void Server::shutdown() {
    traceScope();
    logln("shutting down server");
    if (!getOpt("sandboxMode", false)) {
        m_masterSocket.close();
    }
    logln("shutting down " << m_workers.size() << " workers");
    for (auto& w : m_workers) {
        logln("shutting down worker, isRunning=" << (int)w->isThreadRunning());
        w->shutdown();
    }
    logln("waiting for " << m_workers.size() << " workers");
    for (auto& w : m_workers) {
        w->waitForThreadToExit(-1);
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

bool Server::shouldExclude(const String& name) {
    traceScope();
    std::vector<String> emptylist;
    return shouldExclude(name, emptylist);
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

bool Server::scanPlugin(const String& id, const String& format) {
    std::unique_ptr<AudioPluginFormat> fmt;
    if (!format.compare("VST")) {
        fmt = std::make_unique<VSTPluginFormat>();
    } else if (!format.compare("VST3")) {
        fmt = std::make_unique<VST3PluginFormat>();
#ifdef JUCE_MAC
    } else if (!format.compare("AudioUnit")) {
        fmt = std::make_unique<AudioUnitPluginFormat>();
#endif
    } else {
        return false;
    }
    setLogTagStatic("server");
    logln("scanning id=" << id << " fmt=" << format);
    bool success = true;
    KnownPluginList plist, newlist;
    loadKnownPluginList(plist);
    PluginDirectoryScanner scanner(newlist, *fmt, {}, true, File(Defaults::getConfigFileName(Defaults::ConfigDeadMan)));
    scanner.setFilesOrIdentifiersToScan({id});
    String name;
    scanner.scanNextFile(true, name);
    for (auto& f : scanner.getFailedFiles()) {
        plist.addToBlacklist(f);
        success = false;
    }
    for (auto& t : newlist.getTypes()) {
        logln("adding plugin description:");
        logln("  name            = " << t.name << " (" << t.descriptiveName << ")");
        logln("  uid             = " << t.uid);
        logln("  id string       = " << AGProcessor::createPluginID(t));
        logln("  manufacturer    = " << t.manufacturerName);
        logln("  category        = " << t.category);
        logln("  shell           = " << (int)t.hasSharedContainer);
        logln("  instrument      = " << (int)t.isInstrument);
        logln("  input channels  = " << t.numInputChannels);
        logln("  output channels = " << t.numOutputChannels);
        plist.addType(t);
    }
    saveKnownPluginList(plist);
    return success;
}

void Server::scanNextPlugin(const String& id, const String& fmt) {
    traceScope();
    String fileFmt = id;
    fileFmt << "|" << fmt;
    ChildProcess proc;
    StringArray args;
    args.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
    args.add("-scan");
    args.add(fileFmt);
    if (proc.start(args)) {
        bool finished;
        do {
            proc.waitForProcessToFinish(30000);
            finished = true;
            if (proc.isRunning()) {
                if (!AlertWindow::showOkCancelBox(
                        AlertWindow::WarningIcon, "Timeout",
                        "The plugin scan did not finish yet. Do you want to continue to wait?", "Wait", "Abort")) {
                    logln("error: scan timeout, killing scan process");
                    proc.kill();
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
    std::vector<String> emptylist;
    scanForPlugins(emptylist);
}

void Server::scanForPlugins(const std::vector<String>& include) {
    traceScope();
    logln("scanning for plugins...");
    std::vector<std::unique_ptr<AudioPluginFormat>> fmts;
#ifdef JUCE_MAC
    if (m_enableAU) {
        fmts.push_back(std::make_unique<AudioUnitPluginFormat>());
    }
#endif
    if (m_enableVST3) {
        fmts.push_back(std::make_unique<VST3PluginFormat>());
    }
    if (m_enableVST2) {
        fmts.push_back(std::make_unique<VSTPluginFormat>());
    }

    std::set<String> neverSeenList = m_pluginexclude;

    loadKnownPluginList();

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
            bool excluded = shouldExclude(name, include);
            if ((nullptr == plugindesc || fmt->pluginNeedsRescanning(*plugindesc)) &&
                !m_pluginlist.getBlacklistedFiles().contains(fileOrId) && !excluded) {
                logln("  scanning: " << name);
                String splashName = name;
                if (fmt->getName().compare("AudioUnit")) {
                    File f(name);
                    if (f.exists()) {
                        splashName = f.getFileName();
                    }
                }
                getApp()->setSplashInfo(String("Scanning ") + fmt->getName() + ": " + splashName + " ...");
                scanNextPlugin(fileOrId, fmt->getName());
            } else {
                logln("  (skipping: " << name << (excluded ? " excluded" : "") << ")");
            }
            neverSeenList.erase(name);
        }
    }

    loadKnownPluginList();
    m_pluginlist.sort(KnownPluginList::sortAlphabetically, true);

    for (auto& name : neverSeenList) {
        m_pluginexclude.erase(name);
    }
    logln("scan for plugins finished.");
}

void Server::run() {
    if (getOpt("sandboxMode", false)) {
        runSandbox();
    } else {
        runServer();
    }
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

    ServiceResponder::initialize(m_port + getId(), getId(), m_name);

    if (m_name.isEmpty()) {
        m_name = ServiceResponder::getHostName();
        saveConfig();
    }

    logln("available plugins:");
    for (auto& desc : m_pluginlist.getTypes()) {
        logln("  " << desc.name << " [" << AGProcessor::createPluginID(desc) << "]"
                   << " version=" << desc.version << " format=" << desc.pluginFormatName
                   << " ins=" << desc.numInputChannels << " outs=" << desc.numOutputChannels
                   << " instrument=" << (int)desc.isInstrument);
    }

#ifdef JUCE_MAC
    // Waves is spawning a WavesLocalServer on macOS, that inherits the AG master socket and thus blocks the AG
    // server port. Trying to automatically kill any process, that binds to the AG server port before creating the
    // master socket.
    int port = m_port + getId();
    ChildProcess proc;
    StringArray args;
    args.add("lsof");
    args.add("-nP");
    args.add("-iTCP:" + String(port));
    if (proc.start(args, ChildProcess::wantStdOut)) {
        auto out = proc.readAllProcessOutput();
        for (auto& line : StringArray::fromLines(out)) {
            if (line.endsWith("(LISTEN)")) {
                auto parts = StringArray::fromTokens(line, " ", "");
                if (parts.size() > 1) {
                    auto pid = parts[1];
                    logln("about to kill process " << pid << " that blocks server port " << Defaults::SERVER_PORT);
                    ChildProcess kproc;
                    kproc.start("kill " + pid);
                }
            }
        }
        sleep(3000);
    }
#endif

    logln("creating listener " << (m_host.length() == 0 ? "*" : m_host) << ":" << (m_port + getId()));
    if (m_masterSocket.createListener(m_port + getId(), m_host)) {
        logln("server started: ID=" << getId() << ", PORT=" << m_port + getId() << ", NAME=" << m_name);
        //raise(SIGSEGV);
        while (!currentThreadShouldExit()) {
            auto* clnt = accept(&m_masterSocket, 1000, [] { return currentThreadShouldExit(); });
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

                        logln("  rate                      = " << cfg.rate);
                        logln("  samplesPerBlock           = " << cfg.samplesPerBlock);
                        logln("  doublePrecission          = " << static_cast<int>(cfg.doublePrecission));
                        logln("  flags.NoPluginListFilter  = "
                              << (int)cfg.isFlag(HandshakeRequest::NO_PLUGINLIST_FILTER));
                    } else {
                        logln("client " << clnt->getHostName() << " with old protocol version");
                        handshakeOk = false;
                    }
                } else {
                    logln("client " << clnt->getHostName() << " with protocol error");
                    handshakeOk = false;
                }

                if (!handshakeOk) {
                    clnt->close();
                    delete clnt;
                    continue;
                }

                if (m_sandboxing) {
                    // Spawn a sandbox child process for a new client and tell the client the port to connect to
                    int num = 0;
                    String id = String::toHexString(cfg.clientId) + "-" + String(num);
                    while (m_sandboxes.contains(id)) {
                        num++;
                        id = String::toHexString(cfg.clientId) + "-" + String(num);
                    }
                    auto sandbox = std::make_shared<SandboxMaster>(*this, id);
                    logln("creating sandbox " << id);
                    if (sandbox->launchSlaveProcess(File::getSpecialLocation(File::currentExecutableFile),
                                                    Defaults::SANDBOX_CMD_PREFIX, 30000)) {
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
                            m_sandboxes.set(id, sandbox);
                        } else {
                            logln("failed to send message to sandbox");
                        }
                    } else {
                        logln("failed to launch sandbox");
                    }
                } else {
                    auto masterSocket = std::make_shared<StreamingSocket>();

#ifndef JUCE_WINDOWS
                    setsockopt(masterSocket->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

                    int workerPort = Defaults::CLIENT_PORT;
                    while (!masterSocket->createListener(workerPort, m_host)) {
                        workerPort++;
                        if (workerPort > Defaults::CLIENT_PORT + 1000) {
                            logln("failed to create listener");
                            clnt->close();
                            delete clnt;
                            clnt = nullptr;
                            break;
                        }
                    }

                    if (nullptr == clnt) {
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

                    auto w = std::make_shared<Worker>(masterSocket, cfg);
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
            {
                std::lock_guard<std::mutex> lock(m_sandboxesForDeletionMtx);
                if (!m_sandboxesForDeletion.isEmpty()) {
                    std::thread([deleters = std::move(m_sandboxesForDeletion)] {}).detach();
                }
            }
        }
        if (m_sandboxes.size() > 0) {
            for (auto sandbox : m_sandboxes) {
                m_sandboxesForDeletion.add(sandbox);
            }
            m_sandboxes.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_sandboxesForDeletionMtx);
            if (!m_sandboxesForDeletion.isEmpty()) {
                std::thread([deleters = std::move(m_sandboxesForDeletion)] {}).detach();
                sleep(3000);
            }
        }
    } else {
        logln("failed to create listener");
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
}

void Server::runSandbox() {
    traceScope();

    m_sandboxController = std::make_unique<SandboxSlave>(*this);

    if (!m_sandboxController->initialiseFromCommandLine(getOpt("commandLine", String()), Defaults::SANDBOX_CMD_PREFIX,
                                                        30000)) {
        logln("failed to initialize sandbox process");
        getApp()->prepareShutdown(App::EXIT_SANDBOX_INIT_ERROR);
        return;
    }

    auto masterSocket = std::make_shared<StreamingSocket>();

#ifndef JUCE_WINDOWS
    setsockopt(masterSocket->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

    m_port = Defaults::CLIENT_PORT;
    while (!masterSocket->createListener(m_port, m_host)) {
        m_port++;
        if (m_port > Defaults::CLIENT_PORT + 1000) {
            logln("failed to create listener");
            getApp()->prepareShutdown(App::EXIT_SANDBOX_BIND_ERROR);
            return;
        }
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

    logln("sandbox started: PORT=" << m_port << ", NAME=" << m_name);

    while (!m_sandboxReady && !currentThreadShouldExit()) {
        // wait for the master to send the config
        sleep(10);
    }

    if (!currentThreadShouldExit()) {
        logln("creating worker");
        auto w = std::make_shared<Worker>(masterSocket, m_sandboxConfig);
        w->startThread();
        if (w->isThreadRunning()) {
            m_workers.add(w);

            auto audioTime = Metrics::getStatistic<TimeStatistic>("audio");
            auto bytesOutMeter = Metrics::getStatistic<Meter>("NetBytesOut");
            auto bytesInMeter = Metrics::getStatistic<Meter>("NetBytesIn");

            while (!w->waitForThreadToExit(1000)) {
                json jmetrics;
                jmetrics["LoadedCount"] = AGProcessor::loadedCount.load();
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

    if (!currentThreadShouldExit()) {
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
        m_sandboxLoadedCount.set(sandbox.id, jsonGetValue(msg.data, "LoadedCount", (uint32)0));

        auto bytesOutMeter = Metrics::getStatistic<Meter>("NetBytesOut");
        bytesOutMeter->updateExtRate1min(sandbox.id, jsonGetValue(msg.data, "NetBytesOut", 0.0));

        auto bytesInMeter = Metrics::getStatistic<Meter>("NetBytesIn");
        bytesInMeter->updateExtRate1min(sandbox.id, jsonGetValue(msg.data, "NetBytesIn", 0.0));

        auto audioTime = Metrics::getStatistic<TimeStatistic>("audio");
        audioTime->getMeter().updateExtRate1min(sandbox.id, jsonGetValue(msg.data, "RPS", 0.0));

        if (msg.data.find("audio") != msg.data.end()) {
            std::vector<TimeStatistic::Histogram> hists;
            for (auto& hist : msg.data["audio"]) {
                hists.emplace_back(hist);
            }
            audioTime->updateExt1minValues(sandbox.id, hists);
        }

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
        m_sandboxesForDeletion.add(std::move(deleter));
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
    if (getOpt("sandboxMode", false) && nullptr != m_sandboxController) {
        m_sandboxController->send(SandboxMessage(SandboxMessage::SHOW_EDITOR, {}), nullptr, true);
    }
}

void Server::sandboxHideEditor() {
    if (getOpt("sandboxMode", false) && nullptr != m_sandboxController) {
        m_sandboxController->send(SandboxMessage(SandboxMessage::HIDE_EDITOR, {}));
    }
}

}  // namespace e47
