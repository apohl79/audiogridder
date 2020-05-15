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
#include "Utils.hpp"
#include "json.hpp"

namespace e47 {

using json = nlohmann::json;

Server::Server() : Thread("Server") { loadConfig(); }

void Server::loadConfig() {
    logln("starting server...");
    loadKnownPluginList();
    File cfg(SERVER_CONFIG_FILE);
    if (cfg.exists()) {
        FileInputStream fis(cfg);
        json j = json::parse(fis.readEntireStreamAsString().toStdString());
        if (j.find("ID") != j.end()) {
            m_id = j["ID"].get<int>();
        }
        if (j.find("AU") != j.end()) {
            m_enableAU = j["AU"].get<bool>();
            logln("AudioUnit support " << (m_enableAU ? "enabled" : "disabled"));
        }
        if (j.find("VST") != j.end()) {
            m_enableVST = j["VST"].get<bool>();
            logln("VST3 support " << (m_enableVST ? "enabled" : "disabled"));
        }
        if (j.find("ScreenQuality") != j.end()) {
            m_screenJpgQuality = j["ScreenQuality"].get<float>();
        }
        if (j.find("ScreenDiffDetection") != j.end()) {
            m_screenDiffDetection = j["ScreenDiffDetection"].get<bool>();
            logln("Screen capture difference detection " << (m_screenDiffDetection ? "enabled" : "disabled"));
        }
        if (j.find("ExcludePlugins") != j.end()) {
            for (auto& s : j["ExcludePlugins"]) {
                m_pluginexclude.insert(s.get<std::string>());
            }
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
    json j;
    j["ID"] = m_id;
    j["AU"] = m_enableAU;
    j["VST"] = m_enableVST;
    j["ScreenQuality"] = m_screenJpgQuality;
    j["ScreenDiffDetection"] = m_screenDiffDetection;
    j["ExcludePlugins"] = json::array();
    for (auto& p : m_pluginexclude) {
        j["ExcludePlugins"].push_back(p.toStdString());
    }

    File cfg(SERVER_CONFIG_FILE);
    cfg.deleteFile();
    FileOutputStream fos(cfg);
    fos.writeText(j.dump(4), false, false, "\n");
}

void Server::loadKnownPluginList() {
    File file(KNOWN_PLUGINS_FILE);
    if (file.exists()) {
        auto xml = XmlDocument::parse(file);
        m_pluginlist.recreateFromXml(*xml);
    }
}

void Server::saveKnownPluginList() {
    File file(KNOWN_PLUGINS_FILE);
    auto xml = m_pluginlist.createXml();
    xml->writeTo(file);
}

Server::~Server() {
    if (m_masterSocket.isConnected()) {
        m_masterSocket.close();
    }
    stopThread(-1);
    m_pluginlist.clear();
    logln("server terminated");
}

void Server::shutdown() {
    m_masterSocket.close();
    for (auto& w : m_workers) {
        logln("shutting down worker, isRunning=" << (int)w->isThreadRunning());
        w->shutdown();
        w->waitForThreadToExit(-1);
    }
    signalThreadShouldExit();
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
        logln("scanning for plugins...");
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

bool Server::scanNextPlugin(PluginDirectoryScanner& scanner, String& name) {
    std::mutex mtx;
    std::condition_variable cv;
    bool hasNext = false;
    bool done = false;
    MessageManager::callAsync([this, &mtx, &cv, &scanner, &name, &hasNext, &done] {
        std::lock_guard<std::mutex> lock(mtx);
        hasNext = scanner.scanNextFile(true, name);
        done = true;
        cv.notify_one();
    });
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&done] { return done; });
    return hasNext;
}

void Server::scanForPlugins() {
    std::vector<String> emptylist;
    scanForPlugins(emptylist);
}

void Server::scanForPlugins(const std::vector<String>& include) {
    std::vector<std::unique_ptr<AudioPluginFormat>> fmts;
#ifdef JUCE_MAC
    if (m_enableAU) {
        fmts.push_back(std::make_unique<AudioUnitPluginFormat>());
    }
#endif
    if (m_enableVST) {
        fmts.push_back(std::make_unique<VST3PluginFormat>());
    }

    std::set<String> neverSeenList = m_pluginexclude;

    for (auto& fmt : fmts) {
        PluginDirectoryScanner scanner(m_pluginlist, *fmt, fmt->getDefaultLocationsToSearch(), true,
                                       File(DEAD_MANS_FILE));
        bool hasNext = true;
        while (hasNext) {
            auto name = scanner.getNextPluginFileThatWillBeScanned();
            if (shouldExclude(name, include)) {
                dbgln("  (skipping: " << name << ")");
                hasNext = scanner.skipNextFile();
            } else {
                logln("  scanning: " << name);
                getApp().setSplashInfo(String("Scanning plugin ") + name + "...");
                hasNext = scanNextPlugin(scanner, name);
                saveKnownPluginList();
            }
            neverSeenList.erase(name);
        }
        for (auto& f : scanner.getFailedFiles()) {
            m_pluginlist.addToBlacklist(f);
        }
    }

    StringArray instPlugs;
    for (auto& p : m_pluginlist.getTypes()) {
        if (p.isInstrument) {
            instPlugs.add(p.descriptiveName);
            m_pluginlist.removeType(p);
            if (!p.pluginFormatName.compare("AudioUnit")) {
                m_pluginexclude.insert(p.descriptiveName);
            } else {
                m_pluginexclude.insert(p.fileOrIdentifier);
            }
        }
    }
    if (!instPlugs.isEmpty()) {
        String info = "The following instrument plugins have been deactivated:\n\n";
        info << instPlugs.joinIntoString(", ");
        AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon, "Info", info, "OK");
    }

    m_pluginlist.sort(KnownPluginList::sortAlphabetically, true);

    for (auto& name : neverSeenList) {
        m_pluginexclude.erase(name);
    }
}

void Server::run() {
    logln("scanning for plugins...");
    scanForPlugins();
    saveConfig();
    saveKnownPluginList();

    getApp().hideSplashWindow();

#ifdef JUCE_MAC
    setsockopt(m_masterSocket.getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, nullptr, 0);
#endif

    if (m_masterSocket.createListener(m_port + m_id, m_host)) {
        dbgln("server started: ID=" << m_id << ", PORT=" << m_port + m_id);
        while (!currentThreadShouldExit()) {
            auto* clnt = m_masterSocket.waitForNextConnection();
            if (nullptr != clnt) {
                dbgln("new client " << clnt->getHostName());
                m_workers.emplace_back(std::make_unique<Worker>(clnt));
                m_workers.back()->startThread();
                // lazy cleanup
                std::shared_ptr<WorkerList> deadWorkers = std::make_shared<WorkerList>();
                for (auto it = m_workers.begin(); it < m_workers.end();) {
                    if (!(*it)->isThreadRunning()) {
                        deadWorkers->push_back(std::move(*it));
                        m_workers.erase(it);
                    } else {
                        it++;
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
