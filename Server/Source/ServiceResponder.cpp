/*
 * Copyright (c) 2020 Andreas Pohl
 * Licensed under MIT (https://github.com/apohl79/audiogridder/blob/master/COPYING)
 *
 * Author: Andreas Pohl
 *
 * Based on https://github.com/mjansson/mdns
 */

#include "ServiceResponder.hpp"
#include "Defaults.hpp"
#include "CPUInfo.hpp"
#include "json.hpp"

using json = nlohmann::json;

namespace e47 {
std::unique_ptr<ServiceResponder> ServiceResponder::m_inst;

int serviceCallback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry, uint16_t query_id,
                    uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data, size_t size, size_t name_offset,
                    size_t name_length, size_t record_offset, size_t record_length, void* user_data) {
    setLogTagStatic("mdns");
    traceScope();
    if (nullptr != ServiceResponder::m_inst) {
        return ServiceResponder::m_inst->handleRecord(sock, from, addrlen, entry, query_id, rtype, rclass, ttl, data,
                                                      size, name_offset, name_length, record_offset, record_length,
                                                      user_data);
    }
    return 0;
}

ServiceResponder::ServiceResponder(int port, int id, const String& hostname)
    : Thread("ServiceResponder"), LogTag("mdns"), m_port(port), m_id(id), m_hostname(hostname), m_connector(this) {
    traceScope();

    if (m_hostname.isEmpty()) {
        m_hostname = m_connector.getHostName();
    }
}

ServiceResponder::~ServiceResponder() {
    traceScope();
    stopThread(-1);
}

void ServiceResponder::setHostName(const String& hostname) { m_inst->m_hostname = hostname; }

const String& ServiceResponder::getHostName() { return m_inst->m_hostname; }

void ServiceResponder::initialize(int port, int id, const String& hostname) {
    m_inst = std::make_unique<ServiceResponder>(port, id, hostname);
    m_inst->startThread();
}

void ServiceResponder::cleanup() {
    m_inst->signalThreadShouldExit();
    m_inst.reset();
}

void ServiceResponder::run() {
    traceScope();
    int num = m_connector.openServiceSockets(32);
    if (num <= 0) {
        logln("failed to open client socket(s)");
        return;
    }

    logln("opened " << num << " socket(s)");
    logln("service: " << Defaults::MDNS_SERVICE_NAME);
    logln("hostname: " << m_hostname);

    while (!currentThreadShouldExit()) {
        m_connector.readQueries(serviceCallback);
    }

    m_connector.close();
    logln("closed socket(s)");
}

int ServiceResponder::handleRecord(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
                                   uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t /*ttl*/,
                                   const void* data, size_t size, size_t /*name_offset*/, size_t /*name_length*/,
                                   size_t record_offset, size_t record_length, void* /*user_data*/) {
    traceScope();
    if (entry != MDNS_ENTRYTYPE_QUESTION) {
        return 0;
    }
    auto fromaddrstr = mDNSConnector::ipToString(from, addrlen);
    if (rtype == MDNS_RECORDTYPE_PTR) {
        auto service = MDNS_TO_JUCE_STRING(
            mdns_record_parse_ptr(data, size, record_offset, record_length, m_nameBuffer, sizeof(m_nameBuffer)));
        if (service == Defaults::MDNS_SERVICE_NAME) {
            uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
            // logln(fromaddrstr << " : question PTR " << service);
            // logln("  answer " << m_hostname << "." << MDNS_SERVICE_NAME << " port " << m_port << " ("
            //                  << (unicast ? "unicast" : "multicast") << ")");
            if (!unicast) {
                addrlen = 0;
            }
            json j;
            j["ID"] = m_id;
            j["LOAD"] = CPUInfo::getUsage();
            String txtRecord;
            txtRecord << "INFO=" << j.dump();
            mdns_query_answer(sock, from, addrlen, m_sendBuffer, sizeof(m_sendBuffer), query_id,
                              service.getCharPointer(), (size_t)service.length(), m_hostname.getCharPointer(),
                              (size_t)m_hostname.length(), m_connector.getAddr4(), m_connector.getAddr6(),
                              (uint16_t)m_port, txtRecord.getCharPointer(), (size_t)txtRecord.length());
        }
    }
    return 0;
}

}  // namespace e47
