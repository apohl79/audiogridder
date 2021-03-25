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
        String srv = c->status.serverNameId + " (" + c->status.serverHost + ")";
        String name = c->status.name;
        if (name.isEmpty()) {
            name = "Unnamed";
        }
        if (!c->status.ok) {
            name = "[X] " + name;
        }
        PopupMenu subRecon;
        for (auto& srvInfo : mdnsServers) {
            String mdnsName = srvInfo.getNameAndID() + " (" + srvInfo.getHost() + ")";
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
                String mdnsName = srvInfo.getNameAndID() + " (" + srvInfo.getHost() + ")";
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

}  // namespace e47

START_JUCE_APPLICATION(e47::App)
