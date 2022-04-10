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
#include "Message.hpp"

namespace e47 {

class Server;

class SandboxPeer : public LogTagDelegate {
  public:
    SandboxPeer(Server& server);
    ~SandboxPeer();

    using ResponseCallback = std::function<void(const SandboxMessage&)>;

    bool send(const SandboxMessage& msg, ResponseCallback callback = nullptr, bool shouldBlock = false);
    void read(const MemoryBlock& data);

  protected:
    Server& m_server;

    virtual bool sendMessage(const MemoryBlock&) = 0;
    virtual void handleMessage(const SandboxMessage&) = 0;

  private:
    HashMap<uint64, ResponseCallback, DefaultHashFunctions, CriticalSection> m_callbacks;

    ENABLE_ASYNC_FUNCTORS();
};

struct SandboxMaster : ChildProcessCoordinator, SandboxPeer {
    SandboxMaster(Server& server, const String& id);

    String id;
    std::function<void(int)> onPortReceived;

    // ChildProcessMaster
    void handleConnectionLost() override;
    void handleMessageFromSlave(const MemoryBlock& data) override { read(data); }

  protected:
    // SandboxPeer
    bool sendMessage(const MemoryBlock& data) override { return sendMessageToWorker(data); }
    void handleMessage(const SandboxMessage&) override;
};

struct SandboxSlave : ChildProcessWorker, SandboxPeer {
    SandboxSlave(Server& server);

    // ChildProcessSlave
    void handleConnectionMade() override;
    void handleConnectionLost() override;
    void handleMessageFromMaster(const MemoryBlock& data) override { read(data); }

  protected:
    // SandboxPeer
    bool sendMessage(const MemoryBlock& data) override { return sendMessageToCoordinator(data); }
    void handleMessage(const SandboxMessage&) override;
};

}  // namespace e47

#endif  // Sandbox_hpp
