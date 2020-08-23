/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#ifndef ServiceResponder_hpp
#define ServiceResponder_hpp

#include <JuceHeader.h>

#include "Utils.hpp"
#include "mDNSConnector.hpp"

namespace e47 {

class ServiceResponder : public Thread, public LogTag {
  public:
    static std::unique_ptr<ServiceResponder> m_inst;

    ServiceResponder(int port, int id, const String& hostname);
    ~ServiceResponder() override;

    void run() override;

    int handleRecord(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry, uint16_t query_id,
                     uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data, size_t size, size_t name_offset,
                     size_t name_length, size_t record_offset, size_t record_length, void* user_data);

    static void initialize(int port, int id, const String& hostname);
    static void cleanup();
    static void setHostName(const String& hostname);
    static const String& getHostName();

  private:
    int m_port;
    int m_id;
    String m_hostname;
    mDNSConnector m_connector;

    char m_sendBuffer[256];
    char m_nameBuffer[256];
};

}  // namespace e47

#endif /* ServiceResponder_hpp */
