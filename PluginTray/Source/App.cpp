/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "App.hpp"
#include "Version.hpp"
#include "ServiceReceiver.hpp"

namespace e47 {

void App::initialise(const String& /*commandLineParameters*/) {
#ifdef JUCE_MAC
    Process::setDockIconVisible(false);
#endif
    if (!m_srv.beginWaitingForSocket(Defaults::PLUGIN_TRAY_PORT)) {
        quit();
    }
    AGLogger::initialize("Tray", "AudioGridderTray_", "");
    ServiceReceiver::initialize(0);
}

void App::shutdown() { AGLogger::cleanup(); }

PopupMenu App::Tray::getMenuForIndex(int, const String&) {
    auto mdnsServers = ServiceReceiver::getServers();
    PopupMenu menu;
    menu.addSectionHeader("Connections");
    std::map<String, PopupMenu> serverMenus;
    for (auto& c : m_app->getServer().getConnections()) {
        String srv = App::getServerString(c.get());
        String name = c->status.name;
        if (name.isEmpty()) {
            name = "Unnamed";
        }
        if (!c->status.ok) {
            name = "[X] " + name;
        }
        PopupMenu subRecon;
        for (auto& srvInfo : mdnsServers) {
            String mdnsName = App::getServerString(srvInfo);
            if (srv != mdnsName) {
                subRecon.addItem(mdnsName, [c, srvInfo] {
                    c->sendMessage(PluginTrayMessage(PluginTrayMessage::CHANGE_SERVER,
                                                     {{"serverInfo", srvInfo.serialize().toStdString()}}));
                });
            }
        }
        if (serverMenus.find(srv) == serverMenus.end()) {
            PopupMenu subReconAll;
            for (auto& srvInfo : mdnsServers) {
                String mdnsName = App::getServerString(srvInfo);
                if (srv != mdnsName) {
                    subReconAll.addItem(mdnsName, [this, srvInfo] {
                        for (auto& c2 : m_app->getServer().getConnections()) {
                            c2->sendMessage(PluginTrayMessage(PluginTrayMessage::CHANGE_SERVER,
                                                              {{"serverInfo", srvInfo.serialize().toStdString()}}));
                        }
                    });
                }
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
    menu.addItem(AUDIOGRIDDER_VERSION, false, false, nullptr);
    return menu;
}

void App::Connection::messageReceived(const MemoryBlock& message) {
    PluginTrayMessage msg;
    msg.deserialize(message);

    if (msg.type == PluginTrayMessage::STATUS) {
        status.name = jsonGetValue(msg.data, "name", status.name);
        status.channelsIn = jsonGetValue(msg.data, "channelsIn", 0);
        status.channelsOut = jsonGetValue(msg.data, "channelsOut", 0);
        status.instrument = jsonGetValue(msg.data, "instrument", false);
        status.colour = jsonGetValue(msg.data, "colour", 0u);
        status.loadedPlugins = jsonGetValue(msg.data, "loadedPlugins", status.loadedPlugins);
        status.perf95th = jsonGetValue(msg.data, "perf95th", 0.0);
        status.blocks = jsonGetValue(msg.data, "blocks", 0);
        status.serverNameId = jsonGetValue(msg.data, "serverNameId", status.serverNameId);
        status.serverHost = jsonGetValue(msg.data, "serverHost", status.serverHost);
        status.ok = jsonGetValue(msg.data, "ok", false);
        status.lastUpdated = Time::currentTimeMillis();
        if (!initialized) {
            initialized = true;
            logln("new connection: name=" << status.name);
            m_app->sendRecents(App::getServerString(this), this);
        }
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
    while (i < m_connections.size()) {
        auto c = m_connections[i];
        bool dead = Time::currentTimeMillis() - c->status.lastUpdated > 5000 || (c->initialized && !c->connected);
        if (dead) {
            logln("lost connection: name=" << c->status.name);
            m_connections.remove(i);
        } else {
            i++;
        }
    }
    if (m_connections.isEmpty()) {
        m_noConnectionCounter++;
    } else {
        m_noConnectionCounter = 0;
    }
    if (m_noConnectionCounter > 5) {
        m_app->quit();
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
                if (srv == App::getServerString(c.get())) {
                    c->sendMessage(msgOut);
                }
            }
        }
    }
}

}  // namespace e47

START_JUCE_APPLICATION(e47::App)
