/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Sandbox.hpp"
#include "Server.hpp"

namespace e47 {

SandboxPeer::SandboxPeer(Server& server) : LogTagDelegate(&server), m_server(server) {}

bool SandboxPeer::send(const SandboxMessage& msg, ResponseCallback callback) {
    auto data = msg.serialize();
    MemoryBlock block(data.data(), data.length());
    bool ret = sendMessage(block);
    if (ret && callback) {
        m_callbacks.set(msg.id.hash(), callback);
    }
    return ret;
}

void SandboxPeer::read(const MemoryBlock& data) {
    try {
        auto j = json::parse(data.begin(), data.end());
        SandboxMessage msg(j["type"].get<SandboxMessage::Type>(), j["data"], j["uuid"].get<std::string>());
        auto idhash = msg.id.hash();
        if (m_callbacks.contains(idhash)) {
            m_callbacks[idhash](msg);
            m_callbacks.remove(idhash);
        } else {
            handleMessage(msg);
        }
    } catch (json::parse_error& e) {
        logln("failed to parse json message from sandbox: " << e.what());
    }
}

SandboxMaster::SandboxMaster(Server& server, const String& i) : SandboxPeer(server), id(i) {}

void SandboxMaster::handleConnectionLost() { m_server.handleDisconnectFromSandbox(*this); }

void SandboxMaster::handleMessage(const SandboxMessage& msg) {
    if (msg.type == SandboxMessage::SANDBOX_PORT) {
        int port = msg.data["port"].get<int>();
        logln("received port " << port << " from sandbox " << id);
        if (onPortReceived) {
            onPortReceived(port);
        }
    } else {
        m_server.handleMessageFromSandbox(*this, msg);
    }
}

SandboxSlave::SandboxSlave(Server& server) : SandboxPeer(server) {}

void SandboxSlave::handleConnectionLost() { m_server.handleDisconnectedFromMaster(); }

void SandboxSlave::handleConnectionMade() { m_server.handleConnectedToMaster(); }

void SandboxSlave::handleMessage(const SandboxMessage& msg) { m_server.handleMessageFromMaster(msg); }

}  // namespace e47
