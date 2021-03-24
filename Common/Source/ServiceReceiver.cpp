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
#include "json.hpp"

#ifdef JUCE_WINDOWS
#include <Winsock2.h>
#else
#include <netdb.h>
#endif

using json = nlohmann::json;

namespace e47 {

std::shared_ptr<ServiceReceiver> ServiceReceiver::m_inst;
std::mutex ServiceReceiver::m_instMtx;
size_t ServiceReceiver::m_instRefCount = 0;

int queryCallback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry, uint16_t query_id,
                  uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data, size_t size, size_t name_offset,
                  size_t name_length, size_t record_offset, size_t record_length, void* user_data) {
    setLogTagStatic("mdns_querycallback");
    traceScope();
    auto inst = ServiceReceiver::getInstance();
    if (nullptr != inst) {
        return inst->handleRecord(sock, from, addrlen, entry, query_id, rtype, rclass, ttl, data, size, name_offset,
                                  name_length, record_offset, record_length, user_data);
    }
    return 0;
}

void ServiceReceiver::run() {
    traceScope();
    mDNSConnector connector(this);
    int num = connector.openClientSockets(32, 0);
    if (num < 1) {
        logln("failed to open client socket(s)");
        return;
    }

    logln("receiver ready");

    while (!currentThreadShouldExit()) {
        m_currentResult.clear();

        connector.sendQuery(Defaults::MDNS_SERVICE_NAME);
        // read/store result
        auto timeout = Time::currentTimeMillis() + 3000;
        do {
            connector.readResponses(queryCallback);
        } while (timeout > Time::currentTimeMillis());

        struct SortSrvByName {
            static int compareElements(const ServerInfo& lhs, const ServerInfo& rhs) {
                return lhs.getNameAndID().compare(rhs.getNameAndID());
            }
        };

        SortSrvByName comp;
        m_currentResult.sort(comp);

        bool changed = updateServers();
        if (changed) {
            auto newList = getServersReal();
            logln("updated server list:");
            for (auto& s : newList) {
                logln("  " << s.toString());
            }

            bool locked = false;
            while (!currentThreadShouldExit() && (locked = m_instMtx.try_lock()) == false) {
                sleep(5);
            }
            if (locked) {
                for (auto fn : m_updateFn) {
                    fn();
                }
                m_instMtx.unlock();
            } else {
                logln("can't lock, not executing callbacks");
            }
        }
    }
    connector.close();

    logln("receiver terminated");
}

bool ServiceReceiver::updateServers() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_serverMtx);
    bool changed = false;
    for (auto& s1 : m_currentResult) {
        bool exists = false;
        for (auto& s2 : m_servers) {
            if (s1 == s2) {
                s2.refresh(s1.getLoad());
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_servers.add(s1);
            changed = true;
        }
    }
    auto now = Time::getCurrentTime().toMilliseconds();
    for (int i = 0; i < m_servers.size();) {
        if (m_servers.getReference(i).getUpdated().toMilliseconds() + 30000 < now) {
            m_servers.remove(i);
            changed = true;
        } else {
            i++;
        }
    }
    return changed;
}

int ServiceReceiver::handleRecord(int /*sock*/, const struct sockaddr* from, size_t addrlen,
                                  mdns_entry_type_t /*entry*/, uint16_t /*query_id*/, uint16_t rtype,
                                  uint16_t /*rclass*/, uint32_t /*ttl*/, const void* data, size_t size,
                                  size_t /*name_offset*/, size_t /*name_length*/, size_t record_offset,
                                  size_t record_length, void* /*user_data*/) {
    traceScope();
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
                if (m_txtBuffer[itxt].value.length) {
                    if (key == "ID") {
                        m_curId = MDNS_TO_JUCE_STRING(m_txtBuffer[itxt].value).getIntValue();
                        complete = true;
                    } else if (key == "INFO") {
                        json j = json::parse(MDNS_TO_JUCE_STRING(m_txtBuffer[itxt].value).toStdString());
                        m_curId = 0;
                        if (j.find("ID") != j.end()) {
                            m_curId = j["ID"].get<int>();
                        }
                        m_curLoad = 0.0f;
                        if (j.find("LOAD") != j.end()) {
                            m_curLoad = j["LOAD"].get<float>();
                        }
                        complete = true;
                    }
                }
            }
            break;
        }
    }
    if (complete) {
        auto host = mDNSConnector::ipToString(from, addrlen, true);
        m_currentResult.add(ServerInfo(host, m_curName, m_curId, m_curLoad));
    }
    return 0;
}

void ServiceReceiver::initialize(uint64 id, std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr == m_inst) {
        m_inst = std::make_shared<ServiceReceiver>();
    }
    m_inst->m_updateFn.set(id, fn);
    m_instRefCount++;
}

std::shared_ptr<ServiceReceiver> ServiceReceiver::getInstance() {
    std::lock_guard<std::mutex> lock(m_instMtx);
    return m_inst;
}

void ServiceReceiver::cleanup(uint64 id) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    m_inst->m_updateFn.remove(id);
    m_instRefCount--;
    if (m_instRefCount == 0) {
        m_inst->signalThreadShouldExit();
        m_inst.reset();
    }
}

Array<ServerInfo> ServiceReceiver::getServers() {
    auto inst = getInstance();
    if (nullptr != inst) {
        return inst->getServersReal();
    }
    return {};
}

Array<ServerInfo> ServiceReceiver::getServersReal() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_serverMtx);
    return m_servers;
}

String ServiceReceiver::hostToName(const String& host) {
    for (auto& s : getServers()) {
        if (s.getHost() == host) {
            return s.getName();
        }
    }
    return host;
}

ServerInfo ServiceReceiver::hostToServerInfo(const String& host) {
    for (auto& s : getServers()) {
        if (s.getHost() == host) {
            return s;
        }
    }
    return {};
}

}  // namespace e47
