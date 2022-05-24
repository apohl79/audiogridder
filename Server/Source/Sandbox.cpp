/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#include "Sandbox.hpp"
#include "Server.hpp"

namespace e47 {

SandboxPeer::SandboxPeer(Server& server) : LogTagDelegate(&server), m_server(server) { initAsyncFunctors(); }
SandboxPeer::~SandboxPeer() { stopAsyncFunctors(); }

bool SandboxPeer::send(const SandboxMessage& msg, ResponseCallback callback, bool shouldBlock) {
    traceScope();
    MemoryBlock block;
    msg.serialize(block);
    auto hash = msg.id.hash();
    bool ret = true;
    bool* pret = shouldBlock ? &ret : nullptr;
    auto fn = [this, block, hash, callback, pret] {
        traceScope();
        bool result = sendMessage(block);
        if (result && callback) {
            m_callbacks.set(hash, callback);
        }
        if (nullptr != pret) {
            *pret = result;
        }
    };
    if (shouldBlock) {
        runOnMsgThreadSync(fn);
    } else {
        runOnMsgThreadAsync(fn);
    }
    return ret;
}

void SandboxPeer::read(const MemoryBlock& data) {
    traceScope();
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

void SandboxMaster::handleConnectionLost() {
    traceScope();
    m_server.handleDisconnectFromSandbox(*this);
}

void SandboxMaster::handleMessage(const SandboxMessage& msg) {
    traceScope();
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
