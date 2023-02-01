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
#include "Metrics.hpp"

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
    if (auto inst = ServiceReceiver::getInstance()) {
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

    while (!threadShouldExit()) {
        m_currentResult.clear();

        connector.sendQuery(Defaults::MDNS_SERVICE_NAME);

        // read/store result
        TimeStatistic::Timeout timeout(3000);
        do {
            connector.readResponses(queryCallback);
        } while (timeout.getMillisecondsLeft() > 0 && !threadShouldExit());

        struct SortSrvByName {
            static int compareElements(const ServerInfo& lhs, const ServerInfo& rhs) {
                return lhs.getNameAndID().compare(rhs.getNameAndID());
            }
        };

        SortSrvByName comp;
        m_currentResult.sort(comp);

        bool changed = updateServers();
        if (changed) {
            auto newList = getServersInternal();
            logln("updated server list:");
            for (auto& s : newList) {
                logln("  " << s.toString());
            }

            bool locked = false;
            while (!threadShouldExit() && (locked = m_instMtx.try_lock()) == false) {
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

    bool changed = false;
    Array<ServerInfo> newServers;
    auto now = Time::currentTimeMillis();

    {
        std::lock_guard<std::mutex> lock(m_serversMtx);
        for (auto& s1 : m_currentResult) {
            bool exists = false;
            for (auto& s2 : m_servers) {
                if (s1 == s2) {
                    s2.refresh(s1.getLoad());
                    exists = true;
                    break;
                }
            }
            if (!exists && !newServers.contains(s1)) {
                newServers.add(s1);
            }
        }
        for (int i = 0; i < m_servers.size();) {
            if (m_servers.getReference(i).getUpdated().toMilliseconds() + 30000 < now) {
                m_servers.remove(i);
                changed = true;
            } else {
                i++;
            }
        }
    }

    // reachable checks of existing servers
    int idx = 0;
    for (auto& srv : getServersInternal()) {
        if (threadShouldExit()) {
            return false;
        }
        if (!isReachable(srv)) {
            std::lock_guard<std::mutex> lock(m_serversMtx);
            m_servers.remove(idx);
            changed = true;
        } else {
            idx++;
        }
    }
    // reachable checks of new servers
    for (auto& srv : newServers) {
        if (threadShouldExit()) {
            return false;
        }
        if (isReachable(srv)) {
            std::lock_guard<std::mutex> lock(m_serversMtx);
            m_servers.add(srv);
            changed = true;
        }
    }

    for (auto it = m_lastReachableChecks.begin(); it != m_lastReachableChecks.end();) {
        if (it->second + 30000 > now) {
            it = m_lastReachableChecks.erase(it);
        } else {
            it++;
        }
    }

    return changed;
}

bool ServiceReceiver::isReachable(const ServerInfo& srv) {
    auto now = Time::currentTimeMillis();
    String host = srv.getHost();
    int port = Defaults::SERVER_PORT + srv.getID();
    String key = host + String(port);
    if (m_lastReachableChecks.count(key) == 0 || m_lastReachableChecks[key] + 30000 < now) {
        StreamingSocket sock;
        if (!sock.connect(host, port, 500) || (srv.getLocalMode() && !sock.isLocal())) {
            return false;
        }
        sock.close();
        m_lastReachableChecks[key] = now;
    }
    return true;
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
                        m_curId = jsonGetValue(j, "ID", 0);
                        m_curUuid = jsonGetValue(j, "UUID", String());
                        m_curLoad = jsonGetValue(j, "LOAD", 0.0f);
                        m_curLocalMode = jsonGetValue(j, "LM", false);
                        m_curVersion = jsonGetValue(j, "V", String("unknown"));
                        complete = true;
                    }
                }
            }
            break;
        }
    }
    if (complete) {
        auto host = mDNSConnector::ipToString(from, addrlen, true);
        m_currentResult.add(ServerInfo(host, m_curName, from->sa_family == AF_INET6, m_curId,
                                       m_curUuid.isNotEmpty() ? m_curUuid : Uuid::null(), m_curLoad, m_curLocalMode,
                                       m_curVersion));
    }
    return 0;
}

void ServiceReceiver::initialize(uint64 id, std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr == m_inst) {
        m_inst = std::make_shared<ServiceReceiver>();
    }
    if (fn) {
        m_inst->m_updateFn.set(id, fn);
    }
    m_instRefCount++;
}

std::shared_ptr<ServiceReceiver> ServiceReceiver::getInstance() {
    std::lock_guard<std::mutex> lock(m_instMtx);
    return m_inst;
}

void ServiceReceiver::cleanup(uint64 id) {
    std::lock_guard<std::mutex> lock(m_instMtx);
    if (nullptr != m_inst) {
        m_inst->m_updateFn.remove(id);
        m_instRefCount--;
        if (m_instRefCount == 0) {
            m_inst->signalThreadShouldExit();
            m_inst.reset();
        }
    }
}

Array<ServerInfo> ServiceReceiver::getServers() {
    auto inst = getInstance();
    if (nullptr != inst) {
        return inst->getServersInternal();
    }
    return {};
}

Array<ServerInfo> ServiceReceiver::getServersInternal() {
    traceScope();
    std::lock_guard<std::mutex> lock(m_serversMtx);
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

ServerInfo ServiceReceiver::lookupServerInfo(const String& host) {
    for (auto& s : getServers()) {
        if (s.getHost() == host) {
            return s;
        } else if (s.getHostAndID() == host) {
            return s;
        } else if (s.getName() == host) {
            return s;
        } else if (s.getNameAndID() == host) {
            return s;
        }
    }
    return {};
}

ServerInfo ServiceReceiver::lookupServerInfo(const Uuid& uuid) {
    for (auto& s : getServers()) {
        if (s.getUUID() == uuid) {
            return s;
        }
    }
    return {};
}

}  // namespace e47
