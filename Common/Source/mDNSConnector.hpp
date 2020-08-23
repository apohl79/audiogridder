/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#ifndef mDNSConnector_hpp
#define mDNSConnector_hpp

#include <JuceHeader.h>
#include "Utils.hpp"
#include "mDNS.hpp"

#define MDNS_TO_JUCE_STRING(s) String(s.str, s.length)

namespace e47 {

class mDNSConnector : public LogTagDelegate {
  public:
    mDNSConnector(LogTag* tag) : LogTagDelegate(tag) { m_buffer = malloc(m_bufferSize); }
    ~mDNSConnector() { free(m_buffer); }

    int openClientSockets(int maxSockets, int port);
    int openServiceSockets(int maxSockets);

    // server side, wait for requests
    void readQueries(mdns_record_callback_fn callback, void* userData = nullptr, int timeoutSeconds = 1);

    // client side, send/read queries
    void sendQuery(const String& service);
    void readResponses(mdns_record_callback_fn callback, void* userData = nullptr, int timeoutSeconds = 1);

    void close();

    static String getHostName();
    static String ipv4ToString(const struct sockaddr_in* addr, size_t addrlen, bool noPort = false);
    static String ipv6ToString(const struct sockaddr_in6* addr, size_t addrlen, bool noPort = false);
    static String ipToString(const struct sockaddr* addr, size_t addrlen, bool noPort = false);

    // const Array<int> getSockets() const { return m_sockets; }
    uint32_t getAddr4() const { return m_addr4; }
    const uint8_t* getAddr6() const { return m_addr6; }

  private:
    Array<int> m_sockets;
    bool m_hasIPv4;
    bool m_hasIPv6;
    uint32_t m_addr4;
    uint8_t m_addr6[16];
    void* m_buffer;
    size_t m_bufferSize = 2048;

    enum ReadType { SERVICE, QUERY };
    void readRecords(ReadType type, mdns_record_callback_fn callback, void* userData = nullptr, int timeoutSeconds = 1);
};

}  // namespace e47

#endif /* mDNSConnector_hpp */
