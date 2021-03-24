/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#ifndef ServiceReceiver_hpp
#define ServiceReceiver_hpp

#include <JuceHeader.h>
#include "mDNS.hpp"
#include "Utils.hpp"

namespace e47 {

class ServiceReceiver : public Thread, public LogTag {
  public:
    ServiceReceiver() : Thread("ServiceReceiver"), LogTag("mdns") { startThread(); }
    ~ServiceReceiver() override {
        logln("stopping receiver");
        stopThread(-1);
    }

    void run() override;

    int handleRecord(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry, uint16_t query_id,
                     uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data, size_t size, size_t name_offset,
                     size_t name_length, size_t record_offset, size_t record_length, void* user_data);

    static void initialize(uint64 id, std::function<void()> fn = nullptr);
    static std::shared_ptr<ServiceReceiver> getInstance();
    static void cleanup(uint64 id);

    static Array<ServerInfo> getServers();
    static String hostToName(const String& host);
    static ServerInfo hostToServerInfo(const String& host);

  private:
    static std::shared_ptr<ServiceReceiver> m_inst;
    static std::mutex m_instMtx;
    static size_t m_instRefCount;

    char m_entryBuffer[256];
    mdns_record_txt_t m_txtBuffer[128];
    int m_curId;
    int m_curPort;
    String m_curName;
    float m_curLoad;
    Array<ServerInfo> m_currentResult;

    Array<ServerInfo> m_servers;
    std::mutex m_serverMtx;

    HashMap<uint64, std::function<void()>> m_updateFn;

    Array<ServerInfo> getServersReal();
    bool updateServers();
};

}  // namespace e47
#endif /* ServiceReceiver_hpp */
