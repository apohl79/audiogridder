/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifdef JUCE_MAC
#include <sys/socket.h>
#endif

#include "Server.hpp"
#include "Version.hpp"
#include "App.hpp"
#include "Metrics.hpp"
#include "ServiceResponder.hpp"
#include "ScreenRecorder.hpp"

namespace e47 {

Server::Server(json opts) : Thread("Server"), LogTag("server"), m_opts(opts) {
    logln("starting server (version: " << AUDIOGRIDDER_VERSION << ")...");
    File runFile(SERVER_RUN_FILE);
    runFile.create();
    loadConfig();
    TimeStatistics::initialize();
}

void Server::loadConfig() {
    logln("loading config");
    File cfg(SERVER_CONFIG_FILE);
    if (cfg.exists()) {
        FileInputStream fis(cfg);
        json j = json::parse(fis.readEntireStreamAsString().toStdString());
        if (j.find("ID") != j.end()) {
            m_id = j["ID"].get<int>();
        }
        if (j.find("NAME") != j.end()) {
            m_name = j["NAME"].get<std::string>();
        }
#ifdef JUCE_MAC
        if (j.find("AU") != j.end()) {
            m_enableAU = j["AU"].get<bool>();
            logln("AudioUnit support " << (m_enableAU ? "enabled" : "disabled"));
        }
#endif
        if (j.find("VST") != j.end()) {
            m_enableVST3 = j["VST"].get<bool>();
            logln("VST3 support " << (m_enableVST3 ? "enabled" : "disabled"));
        }
        m_vst3Folders.clear();
        if (j.find("VST3Folders") != j.end() && j["VST3Folders"].size() > 0) {
            logln("VST3 custom folders:");
            for (auto& s : j["VST3Folders"]) {
                if (s.get<std::string>().length() > 0) {
                    logln("  " << s.get<std::string>());
                    m_vst3Folders.add(s.get<std::string>());
                }
            }
        }
        if (j.find("VST2") != j.end()) {
            m_enableVST2 = j["VST2"].get<bool>();
            logln("VST2 support " << (m_enableVST2 ? "enabled" : "disabled"));
        }
        m_vst2Folders.clear();
        if (j.find("VST2Folders") != j.end() && j["VST2Folders"].size() > 0) {
            logln("VST2 custom folders:");
            for (auto& s : j["VST2Folders"]) {
                if (s.get<std::string>().length() > 0) {
                    logln("  " << s.get<std::string>());
                    m_vst2Folders.add(s.get<std::string>());
                }
            }
        }
        if (j.find("ScreenCapturingFFmpeg") != j.end()) {
            m_screenCapturingFFmpeg = j["ScreenCapturingFFmpeg"].get<bool>();
        }
        if (j.find("ScreenCapturingOff") != j.end()) {
            m_screenCapturingOff = j["ScreenCapturingOff"].get<bool>();
        }
        String scmode;
        if (m_screenCapturingOff) {
            scmode = "off";
        } else if (m_screenCapturingFFmpeg) {
            scmode = "ffmpeg";
            MessageManager::callAsync([] { ScreenRecorder::initialize(); });
        } else {
            scmode = "native";
        }
        logln("screen capturing mode: " << scmode);
        if (j.find("ScreenDiffDetection") != j.end()) {
            m_screenDiffDetection = j["ScreenDiffDetection"].get<bool>();
            if (!m_screenCapturingFFmpeg && !m_screenCapturingOff) {
                logln("screen capture difference detection " << (m_screenDiffDetection ? "enabled" : "disabled"));
            }
        }
        if (j.find("ScreenQuality") != j.end()) {
            m_screenJpgQuality = j["ScreenQuality"].get<float>();
        }
        m_pluginexclude.clear();
        if (j.find("ExcludePlugins") != j.end()) {
            for (auto& s : j["ExcludePlugins"]) {
                m_pluginexclude.insert(s.get<std::string>());
            }
        }
        if (j.find("ScanForPlugins") != j.end()) {
            m_scanForPlugins = j["ScanForPlugins"].get<bool>();
        }
    }
    File deadmanfile(DEAD_MANS_FILE);
    if (deadmanfile.exists()) {
        StringArray lines;
        deadmanfile.readLines(lines);
        for (auto& line : lines) {
            m_pluginlist.addToBlacklist(line);
        }
        deadmanfile.deleteFile();
        saveConfig();
    }
}

void Server::saveConfig() {
    logln("saving config");
    json j;
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
    j["ScreenCapturingFFmpeg"] = m_screenCapturingFFmpeg;
    j["ScreenCapturingOff"] = m_screenCapturingOff;
    j["ScreenQuality"] = m_screenJpgQuality;
    j["ScreenDiffDetection"] = m_screenDiffDetection;
    j["ExcludePlugins"] = json::array();
    for (auto& p : m_pluginexclude) {
        j["ExcludePlugins"].push_back(p.toStdString());
    }
    j["ScanForPlugins"] = m_scanForPlugins;

    File cfg(SERVER_CONFIG_FILE);
    cfg.deleteFile();
    FileOutputStream fos(cfg);
    fos.writeText(j.dump(4), false, false, "\n");
}

int Server::getId(bool ignoreOpts) const {
    if (ignoreOpts) {
        return m_id;
    }
    return getOpt("ID", m_id);
}

void Server::loadKnownPluginList() { loadKnownPluginList(m_pluginlist); }

void Server::loadKnownPluginList(KnownPluginList& plist) {
    File file(KNOWN_PLUGINS_FILE);
    if (file.exists()) {
        auto xml = XmlDocument::parse(file);
        plist.recreateFromXml(*xml);
    }
}

void Server::saveKnownPluginList() { saveKnownPluginList(m_pluginlist); }

void Server::saveKnownPluginList(KnownPluginList& plist) {
    File file(KNOWN_PLUGINS_FILE);
    auto xml = plist.createXml();
    xml->writeTo(file);
}

Server::~Server() {
    if (m_masterSocket.isConnected()) {
        m_masterSocket.close();
    }
    waitForThreadAndLog(this, this);
    m_pluginlist.clear();
    TimeStatistics::cleanup();
    ServiceResponder::cleanup();
    logln("server terminated");
    File runFile(SERVER_RUN_FILE);
    runFile.deleteFile();
}

void Server::shutdown() {
    logln("shutting down");
    m_masterSocket.close();
    for (auto& w : m_workers) {
        logln("shutting down worker, isRunning=" << (int)w->isThreadRunning());
        w->shutdown();
        w->waitForThreadToExit(-1);
    }
    signalThreadShouldExit();
}

void Server::setName(const String& name) {
    m_name = name;
    ServiceResponder::setHostName(name);
    logln("setting server name to " << name);
}

bool Server::shouldExclude(const String& name) {
    std::vector<String> emptylist;
    return shouldExclude(name, emptylist);
}

bool Server::shouldExclude(const String& name, const std::vector<String>& include) {
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
    std::thread([this, names, fn] {
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
    KnownPluginList plist;
    loadKnownPluginList(plist);
    auto getLogTag = [] { return "server"; };
    logln("scanning id=" << id << " fmt=" << format);
    bool success = true;
    PluginDirectoryScanner scanner(plist, *fmt, {}, true, File(DEAD_MANS_FILE));
    scanner.setFilesOrIdentifiersToScan({id});
    String name;
    scanner.scanNextFile(true, name);
    for (auto& f : scanner.getFailedFiles()) {
        plist.addToBlacklist(f);
        success = false;
    }
    saveKnownPluginList(plist);
    return success;
}

void Server::scanNextPlugin(const String& id, const String& fmt) {
    String fileFmt = id;
    fileFmt << "|" << fmt;
    ChildProcess proc;
    StringArray args;
    args.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
    args.add("-scan");
    args.add(fileFmt);
    if (proc.start(args)) {
        proc.waitForProcessToFinish(30000);
        if (proc.isRunning()) {
            logln("error: scan timeout, killing scan process");
            proc.kill();
        } else {
            auto ec = proc.getExitCode();
            if (ec != 0) {
                logln("error: scan failed with exit code " << as<int>(ec));
            }
        }
    } else {
        logln("error: failed to start scan process");
    }
}

void Server::scanForPlugins() {
    std::vector<String> emptylist;
    scanForPlugins(emptylist);
}

void Server::scanForPlugins(const std::vector<String>& include) {
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
        auto searchPaths = fmt->getDefaultLocationsToSearch();
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
            if ((nullptr == plugindesc || fmt->pluginNeedsRescanning(*plugindesc)) &&
                !m_pluginlist.getBlacklistedFiles().contains(fileOrId) && !shouldExclude(name, include)) {
                logln("  scanning: " << name);
                getApp()->setSplashInfo(String("Scanning plugin ") + name + "...");
                scanNextPlugin(fileOrId, fmt->getName());
            } else {
                logln("  (skipping: " << name << ")");
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
    if (m_scanForPlugins || getOpt("ScanForPlugins", false)) {
        scanForPlugins();
    } else {
        loadKnownPluginList();
        m_pluginlist.sort(KnownPluginList::sortAlphabetically, true);
    }
    saveConfig();
    saveKnownPluginList();

    getApp()->hideSplashWindow();

#ifdef JUCE_MAC
    setsockopt(m_masterSocket.getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

    ServiceResponder::initialize(m_port + getId(), getId(), m_name);

    if (m_name.isEmpty()) {
        m_name = ServiceResponder::getHostName();
        saveConfig();
    }

    logln("creating listener " << (m_host.length() == 0 ? "*" : m_host) << ":" << (m_port + getId()));
    if (m_masterSocket.createListener(m_port + getId(), m_host)) {
        logln("server started: ID=" << getId() << ", PORT=" << m_port + getId() << ", NAME=" << m_name);
        while (!currentThreadShouldExit()) {
            auto* clnt = m_masterSocket.waitForNextConnection();
            if (nullptr != clnt) {
                logln("new client " << clnt->getHostName());
                auto w = std::make_unique<Worker>(clnt);
                w->startThread();
                m_workers.add(std::move(w));
                // lazy cleanup
                std::shared_ptr<WorkerList> deadWorkers = std::make_shared<WorkerList>();
                for (int i = 0; i < m_workers.size();) {
                    if (!m_workers.getReference(i)->isThreadRunning()) {
                        deadWorkers->add(std::move(m_workers.getReference(i)));
                        m_workers.remove(i);
                    } else {
                        i++;
                    }
                }
                MessageManager::callAsync([deadWorkers] { deadWorkers->clear(); });
            }
        }
    } else {
        logln("failed to create listener");
    }
}

}  // namespace e47
