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

#ifdef JUCE_LINUX
#include <sys/socket.h>
#define SO_NOSIGPIPE MSG_NOSIGNAL
#endif

#include <regex>

namespace e47 {

struct ScanPipeHdr {
    enum Type : uint8 { LOAD_START, LOAD_FINISHED, SHELL };
    Type type;
    int len;
};

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

    bool loadUuid = true;

#ifndef AG_UNIT_TESTS
    // If a server with a new ID is started and has no config yet, fallback to the default config
    if (!File(file).exists()) {
        file = Defaults::getConfigFileName(Defaults::ConfigServer, {{"id", "0"}});
        // But do not inherit the UUID of the default server
        loadUuid = false;
    }
#endif

    logln("loading config from " << file);
    auto cfg = configParseFile(file);
    Tracer::setEnabled(jsonGetValue(cfg, "Tracer", Tracer::isEnabled()));
    Logger::setEnabled(jsonGetValue(cfg, "Logger", Logger::isEnabled()));
    m_id = jsonGetValue(cfg, "ID", m_id);
    m_name = jsonGetValue(cfg, "NAME", m_name);
    if (loadUuid) {
        m_uuid = jsonGetValue(cfg, "UUID", m_uuid.toDashedString());
    }
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
    m_screenMouseOffsetX = jsonGetValue(cfg, "ScreenMouseOffsetX", m_screenMouseOffsetX);
    m_screenMouseOffsetY = jsonGetValue(cfg, "ScreenMouseOffsetY", m_screenMouseOffsetY);
    m_pluginWindowsOnTop = jsonGetValue(cfg, "PluginWindowsOnTop", m_pluginWindowsOnTop);
    m_scanForPlugins = jsonGetValue(cfg, "ScanForPlugins", m_scanForPlugins);
    m_crashReporting = jsonGetValue(cfg, "CrashReporting", m_crashReporting);
    m_processingTraceTresholdMs = jsonGetValue(cfg, "ProcessingTraceTresholdMs", m_processingTraceTresholdMs);
    logln("crash reporting is " << (m_crashReporting ? "enabled" : "disabled"));
    m_sandboxMode = (SandboxMode)jsonGetValue(cfg, "SandboxMode", m_sandboxMode);
    logln("sandbox mode is " << (m_sandboxMode == SANDBOX_CHAIN    ? "chain isolation"
                                 : m_sandboxMode == SANDBOX_PLUGIN ? "plugin isolation"
                                                                   : "disabled"));
    m_sandboxLogAutoclean = jsonGetValue(cfg, "SandboxLogAutoclean", m_sandboxLogAutoclean);
    m_pluginExclude.clear();
    if (jsonHasValue(cfg, "ExcludePlugins")) {
        for (auto& s : cfg["ExcludePlugins"]) {
            m_pluginExclude.insert(s.get<std::string>());
        }
    }
}

void Server::saveConfig() {
    traceScope();
    json j;
    j["Tracer"] = Tracer::isEnabled();
    j["Logger"] = Logger::isEnabled();
    j["ID"] = getId();
    j["NAME"] = m_name.toStdString();
    j["UUID"] = m_uuid.toDashedString().toStdString();
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
    j["ScreenMouseOffsetX"] = m_screenMouseOffsetX;
    j["ScreenMouseOffsetY"] = m_screenMouseOffsetY;
    j["PluginWindowsOnTop"] = m_pluginWindowsOnTop;
    j["ExcludePlugins"] = json::array();
    for (auto& p : m_pluginExclude) {
        j["ExcludePlugins"].push_back(p.toStdString());
    }
    j["ScanForPlugins"] = m_scanForPlugins;
    j["CrashReporting"] = m_crashReporting;
    j["SandboxMode"] = m_sandboxMode;
    j["SandboxLogAutoclean"] = m_sandboxLogAutoclean;
    j["ProcessingTraceTresholdMs"] = m_processingTraceTresholdMs;

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

    loadKnownPluginList(m_pluginList, m_jpluginLayouts, getId());

    std::map<String, PluginDescription> dedupMap;

    for (auto& desc : m_pluginList.getTypes()) {
        std::unique_ptr<AudioPluginFormat> fmt;
        auto pluginId = Processor::createPluginID(desc);

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
            if (shouldExclude(name, desc.fileOrIdentifier)) {
                m_pluginList.removeType(desc);
                m_jpluginLayouts.erase(pluginId.toStdString());
            }
        }

        bool updateDedupMap = true;

        auto it = dedupMap.find(pluginId);
        if (it != dedupMap.end()) {
            auto& descExists = it->second;
            if (desc.version.compare(descExists.version) < 0) {
                // existing one is newer, remove current
                updateDedupMap = false;
                m_pluginList.removeType(desc);
                m_jpluginLayouts.erase(pluginId.toStdString());
                logln("  info: ignoring " << desc.descriptiveName << " (" << desc.version << ") due to newer version");
            } else {
                // existing one is older, keep current
                m_pluginList.removeType(descExists);
                m_jpluginLayouts.erase(Processor::createPluginID(descExists).toStdString());
                logln("  info: ignoring " << descExists.descriptiveName << " (" << descExists.version
                                          << ") due to newer version");
            }
        }

        if (updateDedupMap) {
            dedupMap[pluginId] = desc;
        }
    }
}

bool Server::parsePluginLayouts(const String& id) {
    if (m_jpluginLayouts.empty()) {
        if (m_sandboxModeRuntime == SANDBOX_NONE) {
            String msg =
                "No cached plugin layouts have been found. This can increase plugin loading times and not all "
                "I/O configurations might be available.";
            msg << newLine << newLine << "Do you want to rescan your plugins?";
            if (AlertWindow::showOkCancelBox(AlertWindow::QuestionIcon, "Missing Plugin I/O Layouts", msg, "Yes",
                                             "No")) {
                m_pluginList.clear();
                saveKnownPluginList();
                getApp()->restartServer(true);
                return false;
            }
        }
    } else {
        auto readLayouts = [this](const std::string& key, const auto& jlayouts) {
            if (jlayouts.is_array()) {
                for (auto& jlayout : jlayouts) {
                    auto layout = deserializeLayout(jsonGetValue(jlayout, "layout", String()));
                    m_pluginLayouts[key].add(layout);
                }
            }
        };

        try {
            logln("parsing " << m_jpluginLayouts.size() << " plugin layouts..."
                             << (id.isNotEmpty() ? " (id=" + id + ")" : String()));

            if (id.isNotEmpty()) {
                auto it = m_jpluginLayouts.find(id.toStdString());
                if (it != m_jpluginLayouts.end()) {
                    readLayouts(it.key(), it.value());
                }
            } else {
                for (auto it = m_jpluginLayouts.begin(); it != m_jpluginLayouts.end(); it++) {
                    readLayouts(it.key(), it.value());
                }
            }

            logln("...ok");
        } catch (const json::exception& e) {
            logln("parsing plugin layouts failed: " << e.what());
        }
    }
    return true;
}

const Array<AudioProcessor::BusesLayout>& Server::getPluginLayouts(const String& id) {
    auto it = m_pluginLayouts.find(id);
    if (it != m_pluginLayouts.end()) {
        return it->second;
    }

    static Array<AudioProcessor::BusesLayout> vempty;
    return vempty;
}

void Server::loadKnownPluginList(KnownPluginList& plist, json& playouts, int srvId) {
    setLogTagStatic("server");
    traceScope();

    File cacheFile(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", String(srvId)}}));
    File layoutsFile(Defaults::getConfigFileName(Defaults::PluginLayouts, {{"id", String(srvId)}}));

#ifndef AG_UNIT_TESTS
    if (!cacheFile.existsAsFile() && srvId < Defaults::SCAN_ID_START) {
        cacheFile = File(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", "0"}}));
    }
    if (!layoutsFile.existsAsFile() && srvId < Defaults::SCAN_ID_START) {
        layoutsFile = File(Defaults::getConfigFileName(Defaults::PluginLayouts, {{"id", "0"}}));
    }
#endif

    if (cacheFile.existsAsFile()) {
        logln("loading plugins cache from " << cacheFile.getFullPathName());
        if (auto xml = XmlDocument::parse(cacheFile)) {
            plist.recreateFromXml(*xml);
        } else {
            logln("failed to parse plugins cache " << cacheFile.getFullPathName());
        }
    } else {
        logln("no plugins cache found");
    }

    if (layoutsFile.existsAsFile()) {
        logln("loading plugin layouts from " << layoutsFile.getFullPathName());
        playouts = jsonReadFile(layoutsFile.getFullPathName(), true);
    } else {
        logln("no plugin layouts found");
    }
}

void Server::saveKnownPluginList(bool wipe) {
    traceScope();
    if (wipe) {
        m_pluginList.clear();
        m_jpluginLayouts.clear();
    }
    saveKnownPluginList(m_pluginList, m_jpluginLayouts, getId());
}

void Server::saveKnownPluginList(KnownPluginList& plist, json& playouts, int srvId) {
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

    File cacheFile(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", String(srvId)}}));
    logln("writing plugins cache to " << cacheFile.getFullPathName());
    auto xml = plist.createXml();
    if (!xml->writeTo(cacheFile)) {
        logln("failed to store plugin cache");
    }

    auto layoutsFile = Defaults::getConfigFileName(Defaults::PluginLayouts, {{"id", String(srvId)}});
    logln("writing plugin layouts to " << layoutsFile);
    jsonWriteFile(layoutsFile, playouts, true);
}

Server::~Server() {
    traceScope();

    stopAsyncFunctors();

    if (m_sandboxModeRuntime == SANDBOX_NONE) {
        m_masterSocket.close();
    }

    waitForThreadAndLog(this, this);

    m_pluginList.clear();
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

bool Server::shouldExclude(const String& name, const String& id) {
    traceScope();
    return shouldExclude(name, id, {});
}

bool Server::shouldExclude(const String& name, const String& id, const std::vector<String>& include) {
    traceScope();
    if (name.containsIgnoreCase("AGridder") || name.containsIgnoreCase("AudioGridder")) {
        return true;
    }
    if (include.size() > 0) {
        for (auto& incl : include) {
            if (name == incl || id == incl) {
                return false;
            }
        }
        return true;
    } else {
        for (auto& excl : m_pluginExclude) {
            if (id == excl) {
                return true;
            }
        }
    }
    return false;
}

void Server::addPlugins(const std::vector<String>& names, std::function<void(bool, const String&)> fn) {
    traceScope();
    std::thread([this, names, fn] {
        traceScope();
        scanForPlugins(names);
        saveConfig();
        saveKnownPluginList();
        if (fn) {
            for (auto& name : names) {
                bool found = false;
                for (auto& p : m_pluginList.getTypes()) {
                    if (!name.compare(p.fileOrIdentifier)) {
                        found = true;
                        break;
                    }
                }
                fn(found, name);
            }
        }
    }).detach();
}

bool Server::scanPlugin(const String& id, const String& format, int srvId, bool secondRun) {
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
    logln("scanning id=" << id << " fmt=" << format << " srvId=" << srvId);

    KnownPluginList plist, newlist;
    json playouts;
    loadKnownPluginList(plist, playouts, srvId);

    File crashFile(Defaults::getConfigFileName(Defaults::ConfigDeadMan, {{"id", String(srvId)}}));
    File errFile(Defaults::getConfigFileName(Defaults::ScanError, {{"id", String(srvId)}}));
    File errFileLayout(Defaults::getConfigFileName(Defaults::ScanLayoutError, {{"id", String(srvId)}}));

    if (crashFile.existsAsFile()) {
        crashFile.deleteFile();
    }

    if (errFileLayout.existsAsFile()) {
        errFileLayout.deleteFile();
    }

    errFile.create();

    logln("starting scan...");

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

    logln("...ok");

    String pipeName = "audiogridderscan." + String(srvId);
    NamedPipe pipe;
    if (!pipe.openExisting(pipeName)) {
        logln("warning: can't open pipe " << pipeName);
    }

    auto types = newlist.getTypes();
    for (auto& t : types) {
        auto pluginId = Processor::createPluginID(t);
        auto pluginIdDeprecated = Processor::createPluginIDDepricated(t);
        auto pluginIdWithName = Processor::createPluginIDWithName(t);

        // update the server process
        if (types.size() > 1 && pipe.isOpen()) {
            String out = t.descriptiveName;
            out << " (" << format.toLowerCase() << ")";
            ScanPipeHdr hdr;
            hdr.type = ScanPipeHdr::SHELL;
            hdr.len = out.length();
            pipe.write(&hdr, sizeof(hdr), 100);
            pipe.write(out.toRawUTF8(), hdr.len, 100);
        }

        logln("adding plugin description:");
        logln("  name            = " << t.name << " (" << t.descriptiveName << ")");
        logln("  id              = " << pluginId);
        logln("  id (deprecated) = " << pluginIdDeprecated);
        logln("  id (with name)  = " << pluginIdWithName);
        logln("  manufacturer    = " << t.manufacturerName);
        logln("  category        = " << t.category);
        logln("  shell           = " << (int)t.hasSharedContainer);
        logln("  instrument      = " << (int)t.isInstrument);
        logln("  input channels  = " << t.numInputChannels);
        logln("  output channels = " << t.numOutputChannels);
        plist.addType(t);

        if (!secondRun) {
            // Create an error file that we remove after testing, if we fail, the scanner will trigger a second
            // run, that does not test I/O layouts
            errFileLayout.create();

            logln("testing I/O layouts...");

            // Let the scan master know, that we are loading a plugin now. This might hang, so the master can kill us.
            if (pipe.isOpen()) {
                ScanPipeHdr hdr;
                hdr.type = ScanPipeHdr::LOAD_START;
                hdr.len = 0;
                pipe.write(&hdr, sizeof(hdr), 100);
            }

            String err;
            if (auto inst = Processor::loadPlugin(t, 48000, 512, err)) {
                logln("plugin loaded");

                if (pipe.isOpen()) {
                    ScanPipeHdr hdr;
                    hdr.type = ScanPipeHdr::LOAD_FINISHED;
                    hdr.len = 0;
                    pipe.write(&hdr, sizeof(hdr), 100);
                }

                auto layouts = Processor::findSupportedLayouts(inst);

                for (auto& l : layouts) {
                    json jlayout = {{"description", describeLayout(l).toStdString()},
                                    {"layout", serializeLayout(l).toStdString()}};
                    playouts[pluginId.toStdString()].push_back(jlayout);
                }

                errFileLayout.deleteFile();
            }
        } else {
            // testing I/O layouts failed during the first run, now we check for a mono fx plugin to enable multi-mono
            // support for it
            if (t.numInputChannels == 1 && t.numOutputChannels == 1) {
                AudioProcessor::BusesLayout l;
                l.inputBuses.add(AudioChannelSet::mono());
                l.outputBuses.add(AudioChannelSet::mono());
                json jlayout = {{"description", describeLayout(l).toStdString()},
                                {"layout", serializeLayout(l).toStdString()}};
                playouts[pluginId.toStdString()].push_back(jlayout);
            }
        }
    }

    saveKnownPluginList(plist, playouts, srvId);

    errFile.deleteFile();

    return success;
}

void Server::scanNextPlugin(const String& id, const String& name, const String& fmt, int srvId,
                            std::function<void(const String&)> onShellPlugin, bool secondRun) {
    traceScope();
    String fileFmt = id;
    fileFmt << "|" << fmt;
    ChildProcess proc;
    StringArray args;
    args.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
    args.addArray({"-scan", fileFmt});
    args.addArray({"-id", String(srvId)});
    if (secondRun) {
        args.add("-secondrun");
    }
    String pipeName = "audiogridderscan." + String(srvId);
    NamedPipe pipe;
    if (!pipe.createNewPipe(pipeName)) {
        logln("warning: can't open pipe " << pipeName);
    }
    bool blacklist = false;
    if (proc.start(args)) {
        const int secondsPerPlugin = 30;
        std::atomic_int secondsLeft{secondsPerPlugin};

        FnThread outputReader(
            [this, &pipe, &proc, &secondsLeft, name, secs = secondsPerPlugin, onShellPlugin] {
                std::vector<char> buf(256);
                std::unique_ptr<TimeStatistic::Timeout> loadTimeout;
                String lastShellName;
                while (proc.isRunning() && !currentThreadShouldExit()) {
                    ScanPipeHdr hdr;
                    if (pipe.read(&hdr, sizeof(hdr), 100) == sizeof(hdr)) {
                        switch (hdr.type) {
                            case ScanPipeHdr::LOAD_START:
                                loadTimeout = std::make_unique<TimeStatistic::Timeout>(5000);
                                break;
                            case ScanPipeHdr::LOAD_FINISHED:
                                loadTimeout.reset();
                                break;
                            case ScanPipeHdr::SHELL:
                                if (pipe.read(buf.data(), hdr.len, 1000) == hdr.len) {
                                    buf[(size_t)hdr.len] = 0;
                                    lastShellName = buf.data();
                                    // let the scanner know, that the current plugin is a shell and has plugins inside
                                    onShellPlugin(lastShellName);
                                    // increase the timeout as there might be mutliple plugins per shell
                                    secondsLeft = secs;
                                    logln("    -> shell plugin: " << lastShellName);
                                }
                                break;
                        }
                    }
                    if (nullptr != loadTimeout && loadTimeout->getMillisecondsLeft() <= 0) {
                        logln("error: load timeout for '" << (lastShellName.isNotEmpty() ? lastShellName : name)
                                                          << "', killing process");
                        proc.kill();
                        loadTimeout.reset();
                    }
                }
            },
            "OutputReader", true);

        bool finished;
        int numTimeouts = 0;
        do {
            secondsLeft = secondsPerPlugin;

            while (secondsLeft > 0) {
                proc.waitForProcessToFinish(1000);
                secondsLeft--;
            }

            finished = !proc.isRunning();

            if (!finished) {
                getApp()->enableCancelScan(srvId, [&proc] { proc.kill(); });

                numTimeouts++;

                if (!secondRun && numTimeouts > 9) {
                    // In case the timeout thread is not able to kill the process (I've seen some waves shells on
                    // Windows hanging the process) our last resort is killing it from here.
                    proc.kill();
                    finished = true;
                }
            }

            if (finished) {
                auto layoutErrFile =
                    File(Defaults::getConfigFileName(Defaults::ScanLayoutError, {{"id", String(srvId)}}));
                auto errFile = File(Defaults::getConfigFileName(Defaults::ScanError, {{"id", String(srvId)}}));

                if (!secondRun && layoutErrFile.existsAsFile()) {
                    logln("error: scan for '" << name << "' failed while testing layouts, starting second run");
                    layoutErrFile.deleteFile();
                    scanNextPlugin(id, name, fmt, srvId, onShellPlugin, true);
                } else {
                    if (errFile.existsAsFile()) {
                        logln("error: scan for '" << name << "' failed, as the plugin crashed the scanner probably");
                        errFile.deleteFile();
                        blacklist = true;
                    }
                }

                getApp()->disableCancelScan(srvId);
            }
        } while (!finished);
    } else {
        logln("error: failed to start scan process");
    }

    if (blacklist) {
        // blacklist the plugin
        KnownPluginList plist;
        json playouts;
        loadKnownPluginList(plist, playouts, srvId);
        plist.addToBlacklist(id);
        saveKnownPluginList(plist, playouts, srvId);
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
    size_t fmtAU = 0, fmtVST = 0, fmtVST3 = 0;

#if JUCE_MAC
    if (m_enableAU) {
        fmtAU = fmts.size();
        fmts.push_back(std::make_unique<AudioUnitPluginFormat>());
    }
#endif
    if (m_enableVST3) {
        fmtVST3 = fmts.size();
        fmts.push_back(std::make_unique<VST3PluginFormat>());
    }
#if JUCE_PLUGINHOST_VST
    if (m_enableVST2) {
        fmtVST = fmts.size();
        fmts.push_back(std::make_unique<VSTPluginFormat>());
    }
#endif

    std::set<String> neverSeenList = m_pluginExclude;
    std::set<String> newBlacklistedPlugins;

    loadKnownPluginList();

    // check for scan results after a crash
    for (int i = Defaults::SCAN_ID_START; i < Defaults::SCAN_ID_START + Defaults::SCAN_WORKERS; i++) {
        processScanResults(i, newBlacklistedPlugins);
    }

    struct ScanThread : FnThread {
        int id;
    };

    int scanId = Defaults::SCAN_ID_START;
    std::vector<ScanThread> scanThreads(Defaults::SCAN_WORKERS);
    StringArray inProgressNames;
    for (auto& t : scanThreads) {
        t.id = scanId++;
        inProgressNames.add("");
    }

    std::mutex inProgressMtx;
    std::atomic<float> progress{0};

    StringArray fileOrIds;

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
        fileOrIds.addArray(fmt->searchPathsForPlugins(searchPaths, true));
    }

    for (int idx = 0; idx < fileOrIds.size(); idx++) {
        auto& fileOrId = fileOrIds.getReference(idx);
        auto name = getPluginName(fileOrId, false);
        auto type = getPluginType(fileOrId);
        auto* fmt = type == "au"     ? fmts[fmtAU].get()
                    : type == "vst3" ? fmts[fmtVST3].get()
                    : type == "vst"  ? fmts[fmtVST].get()
                                     : nullptr;

        if (nullptr == fmt) {
            logln("error: can't detect plugin format for " << fileOrId);
            continue;
        }

        auto plugindesc = m_pluginList.getTypeForFile(fileOrId);
        bool excluded = shouldExclude(name, fileOrId, include);
        if ((nullptr == plugindesc || fmt->pluginNeedsRescanning(*plugindesc)) &&
            !m_pluginList.getBlacklistedFiles().contains(fileOrId) && newBlacklistedPlugins.count(fileOrId) == 0 &&
            !excluded) {
            ScanThread* scanThread = nullptr;
            String* inProgressName = nullptr;
            do {
                for (size_t i = 0; i < scanThreads.size(); i++) {
                    if (!scanThreads[i].isThreadRunning()) {
                        scanThread = &scanThreads[i];
                        inProgressName = &inProgressNames.getReference((int)i);
                        break;
                    }
                    sleep(5);
                }
            } while (scanThread == nullptr);

            String splashName = getPluginName(fileOrId);

            progress = (float)idx / fileOrIds.size() * 100.0f;

            logln("  scanning: " << splashName << " (" << String(progress, 1) << "%)");

            auto updateSplash = [&inProgressMtx, inProgressName, &inProgressNames, &progress](const String& newName) {
                std::lock_guard<std::mutex> lock(inProgressMtx);
                *inProgressName = newName;
                StringArray out = inProgressNames;
                out.removeEmptyStrings();
                if (out.isEmpty()) {
                    getApp()->setSplashInfo("Processing scan results...");
                } else {
                    getApp()->setSplashInfo(String("Scanning... (") + String(progress, 1) + String("%)") + newLine +
                                            newLine + out.joinIntoString(", "));
                }
            };

            updateSplash(splashName);

            scanThread->fn = [this, fileOrId, pluginName = name, fmtName = fmt->getName(), srvId = scanThread->id,
                              updateSplash] {
                scanNextPlugin(fileOrId, pluginName, fmtName, srvId, [&](const String& n) { updateSplash(n); });
                updateSplash("");
            };

            scanThread->startThread();
        } else {
            logln("  (skipping: " << name << (excluded ? " excluded" : "") << ")");
        }
        neverSeenList.erase(fileOrId);
    }

    for (auto& t : scanThreads) {
        while (t.isThreadRunning()) {
            sleep(50);
        }
        processScanResults(t.id, newBlacklistedPlugins);
    }

    m_pluginList.sort(KnownPluginList::sortAlphabetically, true);

    getApp()->setSplashInfo("Scanning finished.");

    for (auto& name : neverSeenList) {
        m_pluginExclude.erase(name);
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
        String msg = "The following plugins failed during the plugin scan:";
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

void Server::processScanResults(int id, std::set<String>& newBlacklistedPlugins) {
    File cacheFile(Defaults::getConfigFileName(Defaults::ConfigPluginCache, {{"id", String(id)}}));
    File layoutsFile(Defaults::getConfigFileName(Defaults::PluginLayouts, {{"id", String(id)}}));
    File deadmanFile(Defaults::getConfigFileName(Defaults::ConfigDeadMan, {{"id", String(id)}}));

    if (cacheFile.existsAsFile()) {
        KnownPluginList plist;
        json playouts;
        loadKnownPluginList(plist, playouts, id);

        for (auto& p : plist.getBlacklistedFiles()) {
            if (!m_pluginList.getBlacklistedFiles().contains(p)) {
                m_pluginList.addToBlacklist(p);
                newBlacklistedPlugins.insert(getPluginName(p));
            }
        }

        for (auto p : plist.getTypes()) {
            m_pluginList.addType(p);
        }

        for (auto it = playouts.begin(); it != playouts.end(); it++) {
            m_jpluginLayouts[it.key()] = it.value();
        }

        cacheFile.deleteFile();
        layoutsFile.deleteFile();
    }

    if (deadmanFile.existsAsFile()) {
        logln("reading scan crash file " << deadmanFile.getFullPathName());

        StringArray lines;
        deadmanFile.readLines(lines);

        for (auto& line : lines) {
            newBlacklistedPlugins.insert(getPluginName(line));
        }

        deadmanFile.deleteFile();
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

    getApp()->setSplashInfo("Loading plugins cache...");

    if ((m_scanForPlugins || getOpt("ScanForPlugins", false)) && !getOpt("NoScanForPlugins", false)) {
        scanForPlugins();
    } else {
        loadKnownPluginList();
        m_pluginList.sort(KnownPluginList::sortAlphabetically, true);
    }
    saveConfig();
    saveKnownPluginList();

    getApp()->setSplashInfo("Loading plugin layouts...");
    if (!parsePluginLayouts()) {
        return;
    }

    if (m_crashReporting) {
        Sentry::initialize();
    }

    for (auto& type : m_pluginList.getTypes()) {
        if ((type.pluginFormatName == "AudioUnit" && !m_enableAU) ||
            (type.pluginFormatName == "VST" && !m_enableVST2) || (type.pluginFormatName == "VST3" && !m_enableVST3)) {
            m_pluginList.removeType(type);
        }
    }

    getApp()->hideSplashWindow(1000);

#ifndef JUCE_WINDOWS
    setsockopt(m_masterSocket.getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

    setNonBlocking(m_masterSocket.getRawSocketHandle());

    ServiceResponder::initialize(m_port + getId(), getId(), m_name, m_uuid, getScreenLocalMode());

    if (m_name.isEmpty()) {
        m_name = ServiceResponder::getHostName();
        saveConfig();
    }

    logln("available plugins:");
    for (auto& desc : m_pluginList.getTypes()) {
        logln("  " << desc.name << " [" << Processor::createPluginID(desc) << ", "
                   << Processor::createPluginIDDepricated(desc) << "]"
                   << " version=" << desc.version << " format=" << desc.pluginFormatName
                   << " ins=" << desc.numInputChannels << " outs=" << desc.numOutputChannels
                   << " instrument=" << (int)desc.isInstrument);
    }

    // some time could have passed by until we reach that point, lets check if the user decided to quit
    if (threadShouldExit()) {
        return;
    }

    m_sandboxDeleter = std::make_unique<SandboxDeleter>();

    checkPort();

    // some time could have passed by until we reach that point, lets check if the user decided to quit
    if (threadShouldExit()) {
        return;
    }

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
                if ((clnt = accept(&m_masterSocketLocal, 100, [this] { return threadShouldExit(); })) != nullptr) {
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
                            active << ", ";
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
                                std::shared_ptr<SandboxMaster> deleter;
                                if (m_sandboxes.getAndRemove(id, deleter)) {
                                    deleter->terminate();
                                    m_sandboxDeleter->add(std::move(deleter));
                                }
                            }
                            clnt->close();
                            delete clnt;
                        };
                        if (sandbox->send(SandboxMessage(SandboxMessage::CONFIG, cfg.toJson()), nullptr, true)) {
                            m_sandboxes[id] = std::move(sandbox);
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
            for (auto pair : m_sandboxes) {
                pair.second->terminate();
                m_sandboxDeleter->add(pair.second);
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
    parsePluginLayouts();
    m_pluginList.sort(KnownPluginList::sortAlphabetically, true);

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
        deleter->terminate();
        std::thread([deleter] { delete deleter; }).detach();
    }

    logln("run finished");

    if (!threadShouldExit()) {
        getApp()->prepareShutdown();
    }
}

void Server::runSandboxPlugin() {
    traceScope();

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
    parsePluginLayouts(getOpt("pluginId", String()));
    m_pluginList.sort(KnownPluginList::sortAlphabetically, true);

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
    traceScope();
    HandshakeResponse resp = {AG_PROTOCOL_VERSION, 0, 0, 0, 0, 0, 0, 0, 0};
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
    traceScope();
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
    traceScope();
    if (msg.type == SandboxMessage::SHOW_EDITOR) {
        if (!m_screenLocalMode && m_sandboxHasScreen.isNotEmpty() && m_sandboxHasScreen != sandbox.id) {
            if (auto sb = m_sandboxes[m_sandboxHasScreen]) {
                sb->send(SandboxMessage(SandboxMessage::HIDE_EDITOR, {}));
            }
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
    traceScope();

    std::shared_ptr<SandboxMaster> deleter;
    if (m_sandboxes.getAndRemove(sandbox.id, deleter)) {
        logln("disconnected from sandbox " << sandbox.id);
        m_sandboxLoadedCount.remove(sandbox.id);
        Metrics::getStatistic<TimeStatistic>("audio")->removeExt1minValues(sandbox.id);
        Metrics::getStatistic<TimeStatistic>("audio")->getMeter().removeExtRate1min(sandbox.id);
        Metrics::getStatistic<Meter>("NetBytesOut")->removeExtRate1min(sandbox.id);
        Metrics::getStatistic<Meter>("NetBytesIn")->removeExtRate1min(sandbox.id);
        deleter->terminate();
        m_sandboxDeleter->add(std::move(deleter));
    }
}

void Server::handleConnectedToMaster() {
    logln("connected to sandbox master");
    m_sandboxConnectedToMaster = true;
}

void Server::handleDisconnectedFromMaster() {
    traceScope();
    if (m_sandboxConnectedToMaster.exchange(false)) {
        logln("disconnected from sandbox master");
        signalThreadShouldExit();
        getApp()->prepareShutdown();
    }
}

void Server::handleMessageFromMaster(const SandboxMessage& msg) {
    traceScope();
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
    traceScope();
    if (m_sandboxModeRuntime == SANDBOX_CHAIN && nullptr != m_sandboxController) {
        m_sandboxController->send(SandboxMessage(SandboxMessage::SHOW_EDITOR, {}), nullptr, true);
    }
}

void Server::sandboxHideEditor() {
    traceScope();
    if (m_sandboxModeRuntime == SANDBOX_CHAIN && nullptr != m_sandboxController) {
        m_sandboxController->send(SandboxMessage(SandboxMessage::HIDE_EDITOR, {}));
    }
}

void Server::updateSandboxNetworkStats(const String& key, uint32 loaded, double bytesIn, double bytesOut, double rps,
                                       const std::vector<TimeStatistic::Histogram>& audioHists) {
    traceScope();
    m_sandboxLoadedCount.set(key, loaded);
    Metrics::getStatistic<Meter>("NetBytesIn")->updateExtRate1min(key, bytesIn);
    Metrics::getStatistic<Meter>("NetBytesOut")->updateExtRate1min(key, bytesOut);
    Metrics::getStatistic<TimeStatistic>("audio")->getMeter().updateExtRate1min(key, rps);
    Metrics::getStatistic<TimeStatistic>("audio")->updateExt1minValues(key, audioHists);
}

}  // namespace e47
