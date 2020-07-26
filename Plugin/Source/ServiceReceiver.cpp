/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#include "ServiceReceiver.hpp"
#include "Defaults.hpp"
#include "mDNSConnector.hpp"

#ifdef JUCE_WINDOWS
#include <Winsock2.h>
#else
#include <netdb.h>
#endif

namespace e47 {

std::shared_ptr<ServiceReceiver> ServiceReceiver::m_inst;
std::mutex ServiceReceiver::m_instMtx;
size_t ServiceReceiver::m_instRefCount;

int queryCallback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry, uint16_t query_id,
                  uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data, size_t size, size_t name_offset,
                  size_t name_length, size_t record_offset, size_t record_length, void* user_data) {
    auto inst = ServiceReceiver::getInstance();
    if (nullptr != inst) {
        return inst->handleRecord(sock, from, addrlen, entry, query_id, rtype, rclass, ttl, data, size, name_offset,
                                  name_length, record_offset, record_length, user_data);
    }
    return 0;
}

void ServiceReceiver::run() {
    mDNSConnector connector(this);
    int num = connector.openClientSockets(32, 33445);
    if (num < 1) {
        logln("failed to open client socket(s)");
        return;
    }
    while (!currentThreadShouldExit()) {
        m_currentResult.clear();

        connector.sendQuery(MDNS_SERVICE_NAME);
        // read/store result
        auto timeout = Time::currentTimeMillis() + 3000;
        do {
            connector.readResponses(queryCallback);
        } while (timeout > Time::currentTimeMillis());

        struct SortSrvByName {
            static int compareElements(const ServerString& lhs, const ServerString& rhs) {
                return lhs.getNameAndID().compare(rhs.getNameAndID());
            }
        };

        SortSrvByName comp;
        m_currentResult.sort(comp);

        std::lock_guard<std::mutex> lock(m_serverMtx);
        if (m_servers != m_currentResult) {
            m_servers = m_currentResult;
            logln("updated server list:");
            for (auto& s : m_servers) {
                logln("  " << s.toString());
            }
            if (m_updateFn != nullptr) {
                m_updateFn();
            }
        }
    }
    connector.close();
}

int ServiceReceiver::handleRecord(int /*sock*/, const struct sockaddr* from, size_t addrlen,
                                  mdns_entry_type_t /*entry*/, uint16_t /*query_id*/, uint16_t rtype,
                                  uint16_t /*rclass*/, uint32_t /*ttl*/, const void* data, size_t size,
                                  size_t /*name_offset*/, size_t /*name_length*/, size_t record_offset,
                                  size_t record_length, void* /*user_data*/) {
    bool complete = false;
    switch (rtype) {
        case MDNS_RECORDTYPE_SRV: {
            mdns_record_srv_t srv =
                mdns_record_parse_srv(data, size, record_offset, record_length, m_entryBuffer, sizeof(m_entryBuffer));
            m_curPort = srv.port;
            m_curName = MDNS_TO_JUCE_STRING(srv.name).dropLastCharacters(7);  // remove ".local."
            break;
        }
        case MDNS_RECORDTYPE_A: {
            // struct sockaddr_in addr;
            // mdns_record_parse_a(data, size, record_offset, record_length, &addr);
            // auto addrstr = mDNSConnector::ipv4ToString(&addr, sizeof(addr));
            break;
        }
        case MDNS_RECORDTYPE_AAAA: {
            // struct sockaddr_in6 addr;
            // mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr);
            // auto addrstr = mDNSConnector::ipv6ToString(&addr, sizeof(addr));
            break;
        }
        case MDNS_RECORDTYPE_TXT: {
            size_t parsed = mdns_record_parse_txt(data, size, record_offset, record_length, m_txtBuffer,
                                                  sizeof(m_txtBuffer) / sizeof(mdns_record_txt_t));
            for (size_t itxt = 0; itxt < parsed; ++itxt) {
                auto key = MDNS_TO_JUCE_STRING(m_txtBuffer[itxt].key);
                if (m_txtBuffer[itxt].value.length && key == "ID") {
                    m_curId = MDNS_TO_JUCE_STRING(m_txtBuffer[itxt].value);
                    complete = true;
                }
            }
            break;
        }
    }
    if (complete) {
        auto host = mDNSConnector::ipToString(from, addrlen, true);
        m_currentResult.add(ServerString(host, m_curName, m_curId.getIntValue()));
    }
    return 0;
}

void ServiceReceiver::initialize(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr == m_inst) {
        m_inst = std::make_shared<ServiceReceiver>();
        m_inst->m_updateFn = fn;
    }
    m_instRefCount++;
}

std::shared_ptr<ServiceReceiver> ServiceReceiver::getInstance() { return m_inst; }

void ServiceReceiver::cleanup() {
    std::lock_guard<std::mutex> lock(m_instMtx);
    m_instRefCount--;
    if (m_instRefCount == 0) {
        m_inst->signalThreadShouldExit();
        m_inst.reset();
    }
}

Array<ServerString> ServiceReceiver::getServers() {
    auto inst = getInstance();
    if (nullptr != inst) {
        return inst->getServersReal();
    }
    return {};
}

Array<ServerString> ServiceReceiver::getServersReal() {
    std::lock_guard<std::mutex> lock(m_serverMtx);
    return m_servers;
}

String ServiceReceiver::hostToName(const String& host) {
    auto inst = getInstance();
    if (nullptr != inst) {
        std::lock_guard<std::mutex> lock(inst->m_serverMtx);
        for (auto& s : inst->m_servers) {
            if (s.getHost() == host) {
                return s.getName();
            }
        }
    }
    return host;
}

}  // namespace e47
