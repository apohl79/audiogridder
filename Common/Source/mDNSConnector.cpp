/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#include "mDNSConnector.hpp"

#ifdef JUCE_WINDOWS
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <iphlpapi.h>
// Link with iphlpapi.lib
#pragma comment(lib, "iphlpapi.lib")
#else
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif

namespace e47 {

int mDNSConnector::openClientSockets(int maxSockets, int port) {
    // When sending, each socket can only send to one network interface
    // Thus we need to open one socket for each interface and address family

    traceScope();

#ifdef JUCE_WINDOWS
    // Make sure windows sockets are initialized
    StreamingSocket dummy;
    ignoreUnused(dummy);

    PIP_ADAPTER_ADDRESSES adapter_address = nullptr;
    ULONG address_size = 8000;
    unsigned int ret;
    unsigned int num_retries = 4;
    do {
        adapter_address = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(address_size));
        ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, 0, adapter_address,
                                   &address_size);
        if (ret == ERROR_BUFFER_OVERFLOW) {
            free(adapter_address);
            adapter_address = 0;
        } else {
            break;
        }
    } while (num_retries-- > 0);

    if (!adapter_address || (ret != NO_ERROR)) {
        free(adapter_address);
        logln("failed to get network adapter addresses");
        return 0;
    }

    int first_ipv4 = 1;
    int first_ipv6 = 1;
    for (IP_ADAPTER_ADDRESSES* adapter = adapter_address; adapter; adapter = adapter->Next) {
        if (adapter->TunnelType == TUNNEL_TYPE_TEREDO || adapter->OperStatus != IfOperStatusUp) {
            continue;
        }

        for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in* saddr = (struct sockaddr_in*)unicast->Address.lpSockaddr;
                if ((saddr->sin_addr.S_un.S_un_b.s_b1 != 127) || (saddr->sin_addr.S_un.S_un_b.s_b2 != 0) ||
                    (saddr->sin_addr.S_un.S_un_b.s_b3 != 0) || (saddr->sin_addr.S_un.S_un_b.s_b4 != 1)) {
                    if (first_ipv4) {
                        m_addr4 = saddr->sin_addr.S_un.S_addr;
                        first_ipv4 = 0;
                    }
                    m_hasIPv4 = true;
                    if (m_sockets.size() < maxSockets) {
                        saddr->sin_port = htons((unsigned short)port);
                        int sock = mdns_socket_open_ipv4(saddr);
                        if (sock >= 0) {
                            m_sockets.add(sock);
                        }
                    }
                }
            } else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
                struct sockaddr_in6* saddr = (struct sockaddr_in6*)unicast->Address.lpSockaddr;
                static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
                static const unsigned char localhost_mapped[] = {0, 0, 0,    0,    0,    0, 0, 0,
                                                                 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
                if ((unicast->DadState == NldsPreferred) && memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
                    memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
                    if (first_ipv6) {
                        memcpy(m_addr6, &saddr->sin6_addr, 16);
                        first_ipv6 = 0;
                    }
                    m_hasIPv6 = true;
                    if (m_sockets.size() < maxSockets) {
                        saddr->sin6_port = htons((unsigned short)port);
                        int sock = mdns_socket_open_ipv6(saddr);
                        if (sock >= 0) {
                            m_sockets.add(sock);
                        }
                    }
                }
            }
        }
    }

    free(adapter_address);
#else
    struct ifaddrs* ifaddr = nullptr;
    struct ifaddrs* ifa = nullptr;

    if (getifaddrs(&ifaddr) < 0) {
        logln("unable to get interface addresses");
    }

    int first_ipv4 = 1;
    int first_ipv6 = 1;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* saddr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            if (saddr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                if (first_ipv4) {
                    m_addr4 = saddr->sin_addr.s_addr;
                    first_ipv4 = 0;
                }
                m_hasIPv4 = true;
                if (m_sockets.size() < maxSockets) {
                    saddr->sin_port = htons(port);
                    int sock = mdns_socket_open_ipv4(saddr);
                    if (sock >= 0) {
                        m_sockets.add(sock);
                        logln("opened socket " << sock << " for " << ipv4ToString(saddr, sizeof(sockaddr_in)) << " ("
                                               << ifa->ifa_name << ")");
                    }
                }
            }
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6* saddr = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
            static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
            static const unsigned char localhost_mapped[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
            if (memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
                memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
                if (first_ipv6) {
                    memcpy(m_addr6, &saddr->sin6_addr, 16);
                    first_ipv6 = 0;
                }
                m_hasIPv6 = true;
                if (m_sockets.size() < maxSockets) {
                    saddr->sin6_port = htons(port);
                    int sock = mdns_socket_open_ipv6(saddr);
                    if (sock >= 0) {
                        m_sockets.add(sock);
                        logln("opened socket for " << ipv6ToString(saddr, sizeof(sockaddr_in6)) << " (" << ifa->ifa_name
                                                   << ")");
                    }
                }
            }
        }
    }

    freeifaddrs(ifaddr);
#endif
    return m_sockets.size();
}

int mDNSConnector::openServiceSockets(int maxSockets) {
    // When recieving, each socket can recieve data from all network interfaces
    // Thus we only need to open one socket for each address family

    traceScope();

    // Call the client socket function to enumerate and get local addresses,
    // but not open the actual sockets
    openClientSockets(0, 0);

    if (m_hasIPv4 && m_sockets.size() < maxSockets) {
        struct sockaddr_in sock_addr;
        memset(&sock_addr, 0, sizeof(struct sockaddr_in));
        sock_addr.sin_family = AF_INET;
#if defined(JUCE_WINDOWS)
        sock_addr.sin_addr = in4addr_any;
#else
        sock_addr.sin_addr.s_addr = INADDR_ANY;
#endif
        sock_addr.sin_port = htons(MDNS_PORT);
#if defined(JUCE_MAC)
        sock_addr.sin_len = sizeof(struct sockaddr_in);
#endif
        int sock = mdns_socket_open_ipv4(&sock_addr);
        if (sock > 0) {
            m_sockets.add(sock);
            logln("opened socket for " << ipv4ToString(&sock_addr, sizeof(sockaddr_in)));
        }
    }

    if (m_hasIPv6 && m_sockets.size() < maxSockets) {
        struct sockaddr_in6 sock_addr;
        memset(&sock_addr, 0, sizeof(struct sockaddr_in6));
        sock_addr.sin6_family = AF_INET6;
        sock_addr.sin6_addr = in6addr_any;
        sock_addr.sin6_port = htons(MDNS_PORT);
#if defined(JUCE_MAC)
        sock_addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
        int sock = mdns_socket_open_ipv6(&sock_addr);
        if (sock > 0) {
            m_sockets.add(sock);
            logln("opened socket for " << ipv6ToString(&sock_addr, sizeof(sockaddr_in6)));
        }
    }

    return m_sockets.size();
}

void mDNSConnector::readQueries(mdns_record_callback_fn callback, void* userData, int timeoutSeconds) {
    readRecords(SERVICE, callback, userData, timeoutSeconds);
}

void mDNSConnector::readResponses(mdns_record_callback_fn callback, void* userData, int timeoutSeconds) {
    readRecords(QUERY, callback, userData, timeoutSeconds);
}

void mDNSConnector::sendQuery(const String& service) {
    traceScope();
    for (int i = 0; i < m_sockets.size();) {
        int sock = m_sockets[i];
        if (mdns_query_send(sock, MDNS_RECORDTYPE_PTR, service.getCharPointer(), (size_t)service.length(), m_buffer,
                            m_bufferSize, 0) < 0) {
            int e = errno;
#ifdef JUCE_WINDOWS
            logln("failed to send query: " << strerror(e));
#else
            char errstr[64];
#ifdef JUCE_MAC
            const char* estrp = errstr;
            int foundErrStr = strerror_r(e, errstr, sizeof(errstr));
#elif JUCE_LINUX
            const char* estrp = strerror_r(e, errstr, sizeof(errstr));
            bool foundErrStr = nullptr != estrp;
#endif
            if (foundErrStr) {
                logln("failed to send query: " << estrp);
            } else {
                logln("failed to send query: errno=" << e);
            }
#endif
            m_sockets.remove(i);
            logln("remaining sockets: " << m_sockets.size());
        } else {
            i++;
        }
    }
}

void mDNSConnector::readRecords(ReadType type, mdns_record_callback_fn callback, void* userData, int timeoutSeconds) {
    traceScope();
    int nfds = 0;
    fd_set readfs;
    FD_ZERO(&readfs);
    for (int sock : m_sockets) {
        if (sock >= nfds) {
            nfds = sock + 1;
        }
        FD_SET(sock, &readfs);
    }

    struct timeval tv = {timeoutSeconds, 0};
    int ret = select(nfds, &readfs, nullptr, nullptr, &tv);
    if (ret > 0) {
        for (int sock : m_sockets) {
            if (FD_ISSET(sock, &readfs)) {
                switch (type) {
                    case SERVICE:
                        mdns_socket_listen(sock, m_buffer, m_bufferSize, callback, userData);
                        break;
                    case QUERY:
                        mdns_query_recv(sock, m_buffer, m_bufferSize, callback, userData, 0);
                        break;
                }
            }
            FD_SET(sock, &readfs);
        }
    }
}

void mDNSConnector::close() {
    traceScope();
    for (int sock : m_sockets) {
        mdns_socket_close(sock);
    }
}

String mDNSConnector::getHostName() {
    String s = "localhost";
    char hostname_buffer[256];
#ifdef JUCE_WINDOWS
    DWORD hostname_size = (DWORD)sizeof(hostname_buffer);
    if (GetComputerNameA(hostname_buffer, &hostname_size)) {
        s = hostname_buffer;
    }
#else
    size_t hostname_size = sizeof(hostname_buffer);
    if (gethostname(hostname_buffer, hostname_size) == 0) {
        s = hostname_buffer;
    }
#endif
    return s;
}

String mDNSConnector::ipv4ToString(const struct sockaddr_in* addr, size_t addrlen, bool noPort) {
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                          NI_NUMERICSERV | NI_NUMERICHOST);
    String str;
    if (ret == 0) {
        str << host;
        if (addr->sin_port != 0 && !noPort) {
            str << ":" << service;
        }
    }

    return str;
}

String mDNSConnector::ipv6ToString(const struct sockaddr_in6* addr, size_t addrlen, bool noPort) {
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                          NI_NUMERICSERV | NI_NUMERICHOST);
    String str;
    if (ret == 0) {
        if (addr->sin6_port != 0 && !noPort) {
            str << "[" << host << "]:" << service;
        } else {
            str << host;
        }
    }

    return str;
}

String mDNSConnector::ipToString(const struct sockaddr* addr, size_t addrlen, bool noPort) {
    if (addr->sa_family == AF_INET6) {
        return ipv6ToString(reinterpret_cast<const struct sockaddr_in6*>(addr), addrlen, noPort);
    }
    return ipv4ToString(reinterpret_cast<const struct sockaddr_in*>(addr), addrlen, noPort);
}

}  // namespace e47
