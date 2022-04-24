/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "Version.hpp"
#include "ServiceReceiver.hpp"
#include "Sentry.hpp"

namespace e47 {

void App::initialise(const String& /*commandLineParameters*/) {
#ifdef JUCE_MAC
    Process::setDockIconVisible(false);
#endif
    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPlugin));
    auto args = getCommandLineParameterArray();
    for (int i = 0; i < args.size(); i++) {
        if (args[i] == "-keeprunning") {
            m_keepRunning = true;
        }
    }
    if (Defaults::unixDomainSocketsSupported()) {
        if (!m_srv.beginWaitingForSocket(Defaults::getSocketPath(Defaults::PLUGIN_TRAY_SOCK, {}, true))) {
            quit();
            return;
        }
    } else {
        if (!m_srv.beginWaitingForSocket(Defaults::PLUGIN_TRAY_PORT, "127.0.0.1")) {
            quit();
            return;
        }
    }
    Logger::initialize("Tray", "AudioGridderTray_", "");
    ServiceReceiver::initialize(0);
    if (jsonGetValue(cfg, "CrashReporting", true)) {
        Sentry::initialize();
    }
}

void App::shutdown() {
    m_srv.stop();
    Logger::cleanup();
    ServiceReceiver::cleanup(0);
    Sentry::cleanup();
}

void App::loadConfig() {
    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPluginTray));
    m_mon.showChannelColor = jsonGetValue(cfg, "showChannelColor", m_mon.showChannelColor);
    m_mon.showChannelName = jsonGetValue(cfg, "showChannelName", m_mon.showChannelName);
    m_mon.windowAutoShow = jsonGetValue(cfg, "autoShow", m_mon.windowAutoShow);
}

void App::saveConfig() {
    json cfg;
    cfg["showChannelColor"] = m_mon.showChannelColor;
    cfg["showChannelName"] = m_mon.showChannelName;
    cfg["autoShow"] = m_mon.windowAutoShow;
    configWriteFile(Defaults::getConfigFileName(Defaults::ConfigPluginTray), cfg);
}

void App::getPopupMenu(PopupMenu& menu, bool withShowMonitorOption) {
    auto mdnsServers = ServiceReceiver::getServers();
    std::vector<ServerInfo> cfgServers;
    auto cfg = configParseFile(Defaults::getConfigFileName(Defaults::ConfigPlugin));
    if (jsonHasValue(cfg, "Servers")) {
        for (auto& srv : cfg["Servers"]) {
            cfgServers.emplace_back(srv.get<std::string>());
        }
    }
    menu.addSectionHeader("Connections");
    std::map<String, PopupMenu> serverMenus;
    for (auto& c : m_srv.getConnections()) {
        String srv = getServerString(c);
        String name = c->status.name;
        if (name.isEmpty()) {
            name = "Unnamed";
        }
        if (!c->status.connected) {
            name = "[X] " + name;
        }
        PopupMenu subRecon;
        for (auto& srvInfo : mdnsServers) {
            String mdnsName = getServerString(srvInfo, true);
            bool enabled = (srv != getServerString(srvInfo, false)) && (srvInfo.getVersion() == AUDIOGRIDDER_VERSION);
            subRecon.addItem(mdnsName, enabled, false, [c, srvInfo] {
                c->sendMessage(PluginTrayMessage(PluginTrayMessage::CHANGE_SERVER,
                                                 {{"serverInfo", srvInfo.serialize().toStdString()}}));
            });
        }
        subRecon.addSeparator();
        for (auto& srvInfo : cfgServers) {
            String cfgName = getServerString(srvInfo, false);
            bool enabled = srv != cfgName;
            subRecon.addItem(cfgName, enabled, false, [c, srvInfo] {
                c->sendMessage(PluginTrayMessage(PluginTrayMessage::CHANGE_SERVER,
                                                 {{"serverInfo", srvInfo.serialize().toStdString()}}));
            });
        }
        if (serverMenus.find(srv) == serverMenus.end()) {
            PopupMenu subReconAll;
            for (auto& srvInfo : mdnsServers) {
                String mdnsName = getServerString(srvInfo, true);
                bool enabled =
                    (srv != getServerString(srvInfo, false)) && (srvInfo.getVersion() == AUDIOGRIDDER_VERSION);
                subReconAll.addItem(mdnsName, enabled, false, [this, srvInfo] {
                    for (auto& c2 : m_srv.getConnections()) {
                        c2->sendMessage(PluginTrayMessage(PluginTrayMessage::CHANGE_SERVER,
                                                          {{"serverInfo", srvInfo.serialize().toStdString()}}));
                    }
                });
            }
            subReconAll.addSeparator();
            for (auto& srvInfo : cfgServers) {
                String cfgName = getServerString(srvInfo, false);
                bool enabled = srv != cfgName;
                subReconAll.addItem(cfgName, enabled, false, [this, srvInfo] {
                    for (auto& c2 : m_srv.getConnections()) {
                        c2->sendMessage(PluginTrayMessage(PluginTrayMessage::CHANGE_SERVER,
                                                          {{"serverInfo", srvInfo.serialize().toStdString()}}));
                    }
                });
            }
            serverMenus[srv].addSubMenu("Connect all...", subReconAll);
            serverMenus[srv].addSeparator();
        }
        serverMenus[srv].addSubMenu(name, subRecon);
    }
    for (auto& kv : serverMenus) {
        menu.addSubMenu(kv.first, kv.second);
    }

    menu.addSeparator();

    PopupMenu subMon;
    if (withShowMonitorOption) {
        subMon.addItem("Show...", [this] {
            m_mon.windowAlwaysShow = true;
            m_mon.refresh();
        });
    }
    subMon.addItem("Automatic", true, m_mon.windowAutoShow, [this] {
        m_mon.windowAutoShow = !m_mon.windowAutoShow;
        m_mon.refresh();
        saveConfig();
    });
    subMon.addItem("Show Channel Color", true, m_mon.showChannelColor, [this] {
        m_mon.showChannelColor = !m_mon.showChannelColor;
        m_mon.refresh();
        saveConfig();
    });
    subMon.addItem("Show Channel Name", true, m_mon.showChannelName, [this] {
        m_mon.showChannelName = !m_mon.showChannelName;
        m_mon.refresh();
        saveConfig();
    });
    menu.addSubMenu("Monitor", subMon);
}

PopupMenu App::Tray::getMenuForIndex(int, const String&) {
    PopupMenu menu;
    m_app->getPopupMenu(menu);
    if (m_app->getKeepRunning()) {
        menu.addSeparator();
        menu.addItem("Quit", [this] { m_app->quit(); });
    }
    menu.addSeparator();
    menu.addItem(AUDIOGRIDDER_VERSION, false, false, nullptr);
    return menu;
}

void App::Connection::messageReceived(const MemoryBlock& message) {
    PluginTrayMessage msg;
    msg.deserialize(message);

    if (msg.type == PluginTrayMessage::STATUS) {
        bool changed = false;
        auto updateValue = [&changed](auto& dst, const auto& src) {
            if (dst != src) {
                dst = src;
                changed = true;
            }
        };

        updateValue(status.name, jsonGetValue(msg.data, "name", status.name));
        updateValue(status.channelsIn, jsonGetValue(msg.data, "channelsIn", 0));
        updateValue(status.channelsOut, jsonGetValue(msg.data, "channelsOut", 0));
        updateValue(status.channelsSC, jsonGetValue(msg.data, "channelsSC", 0));
        updateValue(status.instrument, jsonGetValue(msg.data, "instrument", false));
        updateValue(status.colour, jsonGetValue(msg.data, "colour", 0u));
        updateValue(status.loadedPlugins, jsonGetValue(msg.data, "loadedPlugins", status.loadedPlugins));
        updateValue(status.perf95th, jsonGetValue(msg.data, "perf95th", 0.0));
        updateValue(status.blocks, jsonGetValue(msg.data, "blocks", 0));
        updateValue(status.serverNameId, jsonGetValue(msg.data, "serverNameId", status.serverNameId));
        updateValue(status.serverHost, jsonGetValue(msg.data, "serverHost", status.serverHost));
        updateValue(status.connected, jsonGetValue(msg.data, "connected", false));
        updateValue(status.loadedPluginsOk, jsonGetValue(msg.data, "loadedPluginsOk", false));
        updateValue(status.loadedPluginsErr, jsonGetValue(msg.data, "loadedPluginsErr", String()));

        status.lastUpdated = Time::currentTimeMillis();

        if (!initialized) {
            initialized = true;
            changed = true;
            logln("new connection " << String::toHexString((uint64)this) << " (name=" << status.name << ")");
            m_app->sendRecents(App::getServerString(this), this);
        }

        if (changed) {
            logln("state change by connection " << String::toHexString((uint64)this) << " (name=" << status.name
                                                << ")");
            m_app->getMonitor().refresh();
        }
    } else if (msg.type == PluginTrayMessage::STOP) {
        logln("received stop message");
        disconnect();
        connected = false;
    } else {
        m_app->handleMessage(msg, *this);
    }
}

void App::Connection::sendMessage(const PluginTrayMessage& msg) {
    MemoryBlock block;
    msg.serialize(block);
    InterprocessConnection::sendMessage(block);
}

void App::Server::checkConnections() {
    int i = 0;
    int removed = 0;
    while (i < m_connections.size()) {
        auto c = m_connections[i];
        bool timeout = Time::currentTimeMillis() - c->status.lastUpdated > 20000;
        bool dead = (c->initialized && !c->connected);
        if (timeout || dead) {
            logln("lost connection " << String::toHexString((uint64)c.get()) << " (name=" << c->status.name
                                     << " timeout=" << (int)timeout << " dead=" << (int)dead << ")");
            c->disconnect();
            m_connections.remove(i);
            removed++;
        } else {
            i++;
        }
    }
    if (m_connections.isEmpty()) {
        m_noConnectionCounter++;
    } else {
        m_noConnectionCounter = 0;
    }
    if (m_noConnectionCounter > 3 && !m_app->getKeepRunning()) {
        m_app->quit();
    } else {
        if (removed > 0) {
            m_app->getMonitor().refresh();
        }
    }
}

void App::handleMessage(const PluginTrayMessage& msg, Connection& sender) {
    if (msg.type == PluginTrayMessage::UPDATE_RECENTS) {
        auto srv = getServerString(&sender);
        auto plugin = ServerPlugin::fromString(msg.data["plugin"].get<std::string>());
        auto& recents = m_recents[srv];
        if (!recents.contains(plugin)) {
            recents.insert(0, plugin);
            if (recents.size() > 10) {
                recents.remove(10);
            }
            sendRecents(srv);
        }
    } else if (msg.type == PluginTrayMessage::SHOW_MONITOR) {
        m_mon.windowAlwaysShow = true;
        m_mon.refresh();
    }
}

void App::sendRecents(const String& srv, Connection* target) {
    auto jlist = json::array();
    for (auto& r : m_recents[srv]) {
        logln("  adding " << r.toString());
        jlist.push_back(r.toString().toStdString());
    }
    if (jlist.size() > 0) {
        PluginTrayMessage msgOut(PluginTrayMessage::GET_RECENTS, {{"recents", jlist}});
        if (nullptr != target) {
            if (srv == App::getServerString(target)) {
                target->sendMessage(msgOut);
            }
        } else {
            for (auto& c : m_srv.getConnections()) {
                if (srv == App::getServerString(c)) {
                    c->sendMessage(msgOut);
                }
            }
        }
    }
}

}  // namespace e47

START_JUCE_APPLICATION(e47::App)
