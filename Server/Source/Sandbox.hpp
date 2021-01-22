/*
 * Copyright (c) 2021 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 */

#ifndef Sandbox_hpp
#define Sandbox_hpp

#include <JuceHeader.h>

#include "Utils.hpp"

namespace e47 {

class Server;

struct SandboxMessage {
    enum Type : uint16 { CONFIG, SANDBOX_PORT, GET_RECENTS, UPDATE_RECENTS, RECENTS };
    Type type;
    Uuid id;
    json data;

    SandboxMessage(Type t, const json& d) : type(t), data(d) {}
    SandboxMessage(Type t, const json& d, const String& i) : SandboxMessage(t, d) { id = i; }

    auto serialize() const {
        json dataJson;
        dataJson["type"] = type;
        dataJson["uuid"] = id.toString().toStdString();
        dataJson["data"] = data;
        return dataJson.dump();
    }
};

class SandboxPeer : public LogTagDelegate {
  public:
    SandboxPeer(Server& server);

    using ResponseCallback = std::function<void(const SandboxMessage&)>;

    bool send(const SandboxMessage& msg, ResponseCallback callback = nullptr);
    void read(const MemoryBlock& data);

  protected:
    Server& m_server;

    virtual bool sendMessage(const MemoryBlock&) = 0;
    virtual void handleMessage(const SandboxMessage&) = 0;

  private:
    HashMap<uint64, ResponseCallback, DefaultHashFunctions, CriticalSection> m_callbacks;
};

struct SandboxMaster : ChildProcessMaster, SandboxPeer {
    SandboxMaster(Server& server, const String& id);

    String id;
    std::function<void(int)> onPortReceived;

    // ChildProcessMaster
    void handleConnectionLost() override;
    void handleMessageFromSlave(const MemoryBlock& data) override { read(data); }

  protected:
    // SandboxPeer
    bool sendMessage(const MemoryBlock& data) override { return sendMessageToSlave(data); }
    void handleMessage(const SandboxMessage&) override;
};

struct SandboxSlave : ChildProcessSlave, SandboxPeer {
    SandboxSlave(Server& server);

    // ChildProcessSlave
    void handleConnectionMade() override;
    void handleConnectionLost() override;
    void handleMessageFromMaster(const MemoryBlock& data) override { read(data); }

  protected:
    // SandboxPeer
    bool sendMessage(const MemoryBlock& data) override { return sendMessageToMaster(data); }
    void handleMessage(const SandboxMessage&) override;
};

}  // namespace e47

#endif  // Sandbox_hpp
