/* mDNS/DNS-SD library  -  Public Domain  -  2017 Mattias Jansson
 *
 * Modefied version for AudioGridder.
 *
 * This library provides a cross-platform mDNS and DNS-SD library in C.
 * The implementation is based on RFC 6762 and RFC 6763.
 *
 * The latest source code is always available at
 *
 * https://github.com/mjansson/mdns
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any
 * restrictions.
 */

#include "mDNS.hpp"

#include <JuceHeader.h>

#include <fcntl.h>
#ifdef JUCE_WINDOWS
#include <Winsock2.h>
#include <Ws2tcpip.h>
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

int mdns_socket_open_ipv4(struct sockaddr_in* saddr) {
    int sock = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return -1;
    if (mdns_socket_setup_ipv4(sock, saddr)) {
        mdns_socket_close(sock);
        return -1;
    }
    return sock;
}

int mdns_socket_setup_ipv4(int sock, struct sockaddr_in* saddr) {
    unsigned char ttl = 1;
    unsigned char loopback = 1;
    unsigned int reuseaddr = 1;
    struct ip_mreq req;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseaddr, sizeof(reuseaddr));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuseaddr, sizeof(reuseaddr));
#endif
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loopback, sizeof(loopback));

    memset(&req, 0, sizeof(req));
    req.imr_multiaddr.s_addr = htonl((((uint32_t)224U) << 24U) | ((uint32_t)251U));
    if (saddr) req.imr_interface = saddr->sin_addr;
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&req, sizeof(req))) return -1;

    struct sockaddr_in sock_addr;
    if (!saddr) {
        saddr = &sock_addr;
        memset(saddr, 0, sizeof(struct sockaddr_in));
        saddr->sin_family = AF_INET;
        saddr->sin_addr.s_addr = INADDR_ANY;
#ifdef __APPLE__
        saddr->sin_len = sizeof(struct sockaddr_in);
#endif
    } else {
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&saddr->sin_addr, sizeof(saddr->sin_addr));
#ifndef _WIN32
        saddr->sin_addr.s_addr = INADDR_ANY;
#endif
    }

    if (bind(sock, (struct sockaddr*)saddr, sizeof(struct sockaddr_in))) return -1;

#ifdef _WIN32
    unsigned long param = 1;
    ioctlsocket(sock, FIONBIO, &param);
#else
    const int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    return 0;
}

int mdns_socket_open_ipv6(struct sockaddr_in6* saddr) {
    int sock = (int)socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return -1;
    if (mdns_socket_setup_ipv6(sock, saddr)) {
        mdns_socket_close(sock);
        return -1;
    }
    return sock;
}

int mdns_socket_setup_ipv6(int sock, struct sockaddr_in6* saddr) {
    int hops = 1;
    unsigned int loopback = 1;
    unsigned int reuseaddr = 1;
    struct ipv6_mreq req;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseaddr, sizeof(reuseaddr));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuseaddr, sizeof(reuseaddr));
#endif
    setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (const char*)&hops, sizeof(hops));
    setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (const char*)&loopback, sizeof(loopback));

    memset(&req, 0, sizeof(req));
    req.ipv6mr_multiaddr.s6_addr[0] = 0xFF;
    req.ipv6mr_multiaddr.s6_addr[1] = 0x02;
    req.ipv6mr_multiaddr.s6_addr[15] = 0xFB;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&req, sizeof(req))) return -1;

    struct sockaddr_in6 sock_addr;
    if (!saddr) {
        saddr = &sock_addr;
        memset(saddr, 0, sizeof(struct sockaddr_in6));
        saddr->sin6_family = AF_INET6;
        saddr->sin6_addr = in6addr_any;
#ifdef __APPLE__
        saddr->sin6_len = sizeof(struct sockaddr_in6);
#endif
    } else {
        unsigned int ifindex = 0;
        setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char*)&ifindex, sizeof(ifindex));
#ifndef _WIN32
        saddr->sin6_addr = in6addr_any;
#endif
    }

    if (bind(sock, (struct sockaddr*)saddr, sizeof(struct sockaddr_in6))) return -1;

#ifdef _WIN32
    unsigned long param = 1;
    ioctlsocket(sock, FIONBIO, &param);
#else
    const int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    return 0;
}

void mdns_socket_close(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

int mdns_is_string_ref(uint8_t val) { return (0xC0 == (val & 0xC0)); }

mdns_string_pair_t mdns_get_next_substring(const void* rawdata, size_t size, size_t offset) {
    const uint8_t* buffer = (const uint8_t*)rawdata;
    mdns_string_pair_t pair = {MDNS_INVALID_POS, 0, 0};
    if (!buffer[offset]) {
        pair.offset = offset;
        return pair;
    }
    if (mdns_is_string_ref(buffer[offset])) {
        if (size < offset + 2) return pair;

        offset = 0x3fff & ntohs(*(uint16_t*)MDNS_POINTER_OFFSET(buffer, offset));
        if (offset >= size) return pair;

        pair.ref = 1;
    }

    size_t length = (size_t)buffer[offset++];
    if (size < offset + length) return pair;

    pair.offset = offset;
    pair.length = length;

    return pair;
}

int mdns_string_skip(const void* buffer, size_t size, size_t* offset) {
    size_t cur = *offset;
    mdns_string_pair_t substr;
    do {
        substr = mdns_get_next_substring(buffer, size, cur);
        if (substr.offset == MDNS_INVALID_POS) return 0;
        if (substr.ref) {
            *offset = cur + 2;
            return 1;
        }
        cur = substr.offset + substr.length;
    } while (substr.length);

    *offset = cur + 1;
    return 1;
}

int mdns_string_equal(const void* buffer_lhs, size_t size_lhs, size_t* ofs_lhs, const void* buffer_rhs, size_t size_rhs,
                      size_t* ofs_rhs) {
    size_t lhs_cur = *ofs_lhs;
    size_t rhs_cur = *ofs_rhs;
    size_t lhs_end = MDNS_INVALID_POS;
    size_t rhs_end = MDNS_INVALID_POS;
    mdns_string_pair_t lhs_substr;
    mdns_string_pair_t rhs_substr;
    do {
        lhs_substr = mdns_get_next_substring(buffer_lhs, size_lhs, lhs_cur);
        rhs_substr = mdns_get_next_substring(buffer_rhs, size_rhs, rhs_cur);
        if ((lhs_substr.offset == MDNS_INVALID_POS) || (rhs_substr.offset == MDNS_INVALID_POS)) return 0;
        if (lhs_substr.length != rhs_substr.length) return 0;
        if (strncasecmp((const char*)buffer_rhs + rhs_substr.offset, (const char*)buffer_lhs + lhs_substr.offset,
                        rhs_substr.length))
            return 0;
        if (lhs_substr.ref && (lhs_end == MDNS_INVALID_POS)) lhs_end = lhs_cur + 2;
        if (rhs_substr.ref && (rhs_end == MDNS_INVALID_POS)) rhs_end = rhs_cur + 2;
        lhs_cur = lhs_substr.offset + lhs_substr.length;
        rhs_cur = rhs_substr.offset + rhs_substr.length;
    } while (lhs_substr.length);

    if (lhs_end == MDNS_INVALID_POS) lhs_end = lhs_cur + 1;
    *ofs_lhs = lhs_end;

    if (rhs_end == MDNS_INVALID_POS) rhs_end = rhs_cur + 1;
    *ofs_rhs = rhs_end;

    return 1;
}

mdns_string_t mdns_string_extract(const void* buffer, size_t size, size_t* offset, char* str, size_t capacity) {
    size_t cur = *offset;
    size_t end = MDNS_INVALID_POS;
    mdns_string_pair_t substr;
    mdns_string_t result;
    result.str = str;
    result.length = 0;
    char* dst = str;
    size_t remain = capacity;
    do {
        substr = mdns_get_next_substring(buffer, size, cur);
        if (substr.offset == MDNS_INVALID_POS) return result;
        if (substr.ref && (end == MDNS_INVALID_POS)) end = cur + 2;
        if (substr.length) {
            size_t to_copy = (substr.length < remain) ? substr.length : remain;
            memcpy(dst, (const char*)buffer + substr.offset, to_copy);
            dst += to_copy;
            remain -= to_copy;
            if (remain) {
                *dst++ = '.';
                --remain;
            }
        }
        cur = substr.offset + substr.length;
    } while (substr.length);

    if (end == MDNS_INVALID_POS) end = cur + 1;
    *offset = end;

    result.length = capacity - remain;
    return result;
}

size_t mdns_string_find(const char* str, size_t length, char c, size_t offset) {
    const void* found;
    if (offset >= length) return MDNS_INVALID_POS;
    found = memchr(str + offset, c, length - offset);
    if (found) return (size_t)((const char*)found - str);
    return MDNS_INVALID_POS;
}

void* mdns_string_make(void* data, size_t capacity, const char* name, size_t length) {
    size_t pos = 0;
    size_t last_pos = 0;
    size_t remain = capacity;
    unsigned char* dest = (unsigned char*)data;
    while ((last_pos < length) && ((pos = mdns_string_find(name, length, '.', last_pos)) != MDNS_INVALID_POS)) {
        size_t sublength = pos - last_pos;
        if (sublength < remain) {
            *dest = (unsigned char)sublength;
            memcpy(dest + 1, name + last_pos, sublength);
            dest += sublength + 1;
            remain -= sublength + 1;
        } else {
            return nullptr;
        }
        last_pos = pos + 1;
    }
    if (last_pos < length) {
        size_t sublength = length - last_pos;
        if (sublength < remain) {
            *dest = (unsigned char)sublength;
            memcpy(dest + 1, name + last_pos, sublength);
            dest += sublength + 1;
            remain -= sublength + 1;
        } else {
            return nullptr;
        }
    }
    if (!remain) return nullptr;
    *dest++ = 0;
    return dest;
}

void* mdns_string_make_ref(void* data, size_t capacity, size_t ref_offset) {
    if (capacity < 2) return nullptr;
    uint16_t* udata = (uint16_t*)data;
    *udata++ = htons(0xC000 | (uint16_t)ref_offset);
    return udata;
}

void* mdns_string_make_with_ref(void* data, size_t capacity, const char* name, size_t length, size_t ref_offset) {
    void* remaindata = mdns_string_make(data, capacity, name, length);
    capacity -= MDNS_POINTER_DIFF(remaindata, data);
    if (!data || !capacity) return nullptr;
    return mdns_string_make_ref(MDNS_POINTER_OFFSET(remaindata, -1), capacity + 1, ref_offset);
}

size_t mdns_records_parse(int sock, const struct sockaddr* from, size_t addrlen, const void* buffer, size_t size,
                          size_t* offset, mdns_entry_type_t type, uint16_t query_id, size_t records,
                          mdns_record_callback_fn callback, void* user_data) {
    size_t parsed = 0;
    int do_callback = (callback ? 1 : 0);
    for (size_t i = 0; i < records; ++i) {
        size_t name_offset = *offset;
        mdns_string_skip(buffer, size, offset);
        size_t name_length = (*offset) - name_offset;
        const uint16_t* data = reinterpret_cast<const uint16_t*>((const char*)buffer + (*offset));

        uint16_t rtype = ntohs(*data++);
        uint16_t rclass = ntohs(*data++);
        uint32_t ttl = ntohl(*(const uint32_t*)(const void*)data);
        data += 2;
        uint16_t length = ntohs(*data++);

        *offset += 10;

        if (do_callback) {
            ++parsed;
            if (callback(sock, from, addrlen, type, query_id, rtype, rclass, ttl, buffer, size, name_offset,
                         name_length, *offset, length, user_data))
                do_callback = 0;
        }

        *offset += length;
    }
    return parsed;
}

int mdns_unicast_send(int sock, const void* address, size_t address_size, const void* buffer, size_t size) {
    if (sendto(sock, (const char*)buffer, (mdns_size_t)size, 0, (const struct sockaddr*)address,
               (socklen_t)address_size) < 0)
        return -1;
    return 0;
}

int mdns_multicast_send(int sock, const void* buffer, size_t size) {
    struct sockaddr_storage addr_storage;
    struct sockaddr_in addr;
    struct sockaddr_in6 addr6;
    struct sockaddr* saddr = (struct sockaddr*)&addr_storage;
    socklen_t saddrlen = sizeof(struct sockaddr_storage);
    if (getsockname(sock, saddr, &saddrlen)) return -1;
    if (saddr->sa_family == AF_INET6) {
        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
#ifdef __APPLE__
        addr6.sin6_len = sizeof(addr6);
#endif
        addr6.sin6_addr.s6_addr[0] = 0xFF;
        addr6.sin6_addr.s6_addr[1] = 0x02;
        addr6.sin6_addr.s6_addr[15] = 0xFB;
        addr6.sin6_port = htons((unsigned short)MDNS_PORT);
        saddr = (struct sockaddr*)&addr6;
        saddrlen = sizeof(addr6);
    } else {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
#ifdef __APPLE__
        addr.sin_len = sizeof(addr);
#endif
        addr.sin_addr.s_addr = htonl((((uint32_t)224U) << 24U) | ((uint32_t)251U));
        addr.sin_port = htons((unsigned short)MDNS_PORT);
        saddr = (struct sockaddr*)&addr;
        saddrlen = sizeof(addr);
    }

    if (sendto(sock, (const char*)buffer, (mdns_size_t)size, 0, saddr, saddrlen) < 0) return -1;
    return 0;
}

const uint8_t mdns_services_query[] = {
    // Query ID
    0x00, 0x00,
    // Flags
    0x00, 0x00,
    // 1 question
    0x00, 0x01,
    // No answer, authority or additional RRs
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // _services._dns-sd._udp.local.
    0x09, '_', 's', 'e', 'r', 'v', 'i', 'c', 'e', 's', 0x07, '_', 'd', 'n', 's', '-', 's', 'd', 0x04, '_', 'u', 'd',
    'p', 0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
    // PTR record
    0x00, MDNS_RECORDTYPE_PTR,
    // QU (unicast response) and class IN
    0x80, MDNS_CLASS_IN};

int mdns_discovery_send(int sock) {
    return mdns_multicast_send(sock, mdns_services_query, sizeof(mdns_services_query));
}

size_t mdns_discovery_recv(int sock, void* buffer, size_t capacity, mdns_record_callback_fn callback, void* user_data) {
    struct sockaddr_in6 addr;
    struct sockaddr* saddr = (struct sockaddr*)&addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
#ifdef __APPLE__
    saddr->sa_len = sizeof(addr);
#endif
    int ret = (int)recvfrom(sock, (char*)buffer, (mdns_size_t)capacity, 0, saddr, &addrlen);
    if (ret <= 0) return 0;

    size_t data_size = (size_t)ret;
    size_t records = 0;
    uint16_t* data = (uint16_t*)buffer;

    uint16_t query_id = ntohs(*data++);
    uint16_t flags = ntohs(*data++);
    uint16_t questions = ntohs(*data++);
    uint16_t answer_rrs = ntohs(*data++);
    uint16_t authority_rrs = ntohs(*data++);
    uint16_t additional_rrs = ntohs(*data++);

    // According to RFC 6762 the query ID MUST match the sent query ID (which is 0 in our case)
    if (query_id || (flags != 0x8400)) return 0;  // Not a reply to our question

    // It seems some implementations do not fill the correct questions field,
    // so ignore this check for now and only validate answer string
    /*
     if (questions != 1)
     return 0;
     */

    int i;
    for (i = 0; i < questions; ++i) {
        size_t ofs = (size_t)((char*)data - (char*)buffer);
        size_t verify_ofs = 12;
        // Verify it's our question, _services._dns-sd._udp.local.
        if (!mdns_string_equal(buffer, data_size, &ofs, mdns_services_query, sizeof(mdns_services_query), &verify_ofs))
            return 0;
        data = reinterpret_cast<uint16_t*>((char*)buffer + ofs);

        uint16_t rtype = ntohs(*data++);
        uint16_t rclass = ntohs(*data++);

        // Make sure we get a reply based on our PTR question for class IN
        if ((rtype != MDNS_RECORDTYPE_PTR) || ((rclass & 0x7FFF) != MDNS_CLASS_IN)) return 0;
    }

    int do_callback = 1;
    for (i = 0; i < answer_rrs; ++i) {
        size_t ofs = (size_t)((char*)data - (char*)buffer);
        size_t verify_ofs = 12;
        // Verify it's an answer to our question, _services._dns-sd._udp.local.
        size_t name_offset = ofs;
        int is_answer =
            mdns_string_equal(buffer, data_size, &ofs, mdns_services_query, sizeof(mdns_services_query), &verify_ofs);
        size_t name_length = ofs - name_offset;
        data = reinterpret_cast<uint16_t*>((char*)buffer + ofs);

        uint16_t rtype = ntohs(*data++);
        uint16_t rclass = ntohs(*data++);
        uint32_t ttl = ntohl(*(uint32_t*)(void*)data);
        data += 2;
        uint16_t length = ntohs(*data++);
        if (length >= (data_size - ofs)) return 0;

        if (is_answer && do_callback) {
            ++records;
            ofs = (size_t)((char*)data - (char*)buffer);
            if (callback(sock, saddr, addrlen, MDNS_ENTRYTYPE_ANSWER, query_id, rtype, rclass, ttl, buffer, data_size,
                         name_offset, name_length, ofs, length, user_data))
                do_callback = 0;
        }
        data = reinterpret_cast<uint16_t*>((char*)data + length);
        ;
    }

    size_t offset = (size_t)((char*)data - (char*)buffer);
    records += mdns_records_parse(sock, saddr, addrlen, buffer, data_size, &offset, MDNS_ENTRYTYPE_AUTHORITY, query_id,
                                  authority_rrs, callback, user_data);
    records += mdns_records_parse(sock, saddr, addrlen, buffer, data_size, &offset, MDNS_ENTRYTYPE_ADDITIONAL, query_id,
                                  additional_rrs, callback, user_data);

    return records;
}

size_t mdns_socket_listen(int sock, void* buffer, size_t capacity, mdns_record_callback_fn callback, void* user_data) {
    struct sockaddr_in6 addr;
    struct sockaddr* saddr = (struct sockaddr*)&addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
#ifdef __APPLE__
    saddr->sa_len = sizeof(addr);
#endif
    int ret = (int)recvfrom(sock, (char*)buffer, (mdns_size_t)capacity, 0, saddr, &addrlen);
    if (ret <= 0) return 0;

    size_t data_size = (size_t)ret;
    uint16_t* data = (uint16_t*)buffer;

    uint16_t query_id = ntohs(*data++);
    uint16_t flags = ntohs(*data++);
    uint16_t questions = ntohs(*data++);
    /*
     This data is unused at the moment, skip
     uint16_t answer_rrs = ntohs(*data++);
     uint16_t authority_rrs = ntohs(*data++);
     uint16_t additional_rrs = ntohs(*data++);
     */
    data += 3;

    size_t parsed = 0;
    for (int iquestion = 0; iquestion < questions; ++iquestion) {
        size_t question_offset = (size_t)((char*)data - (char*)buffer);
        size_t offset = question_offset;
        size_t verify_ofs = 12;
        if (mdns_string_equal(buffer, data_size, &offset, mdns_services_query, sizeof(mdns_services_query),
                              &verify_ofs)) {
            if (flags || (questions != 1)) return 0;
        } else {
            offset = question_offset;
            if (!mdns_string_skip(buffer, data_size, &offset)) break;
        }
        size_t length = offset - question_offset;
        data = reinterpret_cast<uint16_t*>((char*)buffer + offset);

        uint16_t rtype = ntohs(*data++);
        uint16_t rclass = ntohs(*data++);

        // Make sure we get a question of class IN
        if ((rclass & 0x7FFF) != MDNS_CLASS_IN) return 0;

        if (callback)
            callback(sock, saddr, addrlen, MDNS_ENTRYTYPE_QUESTION, query_id, rtype, rclass, 0, buffer, data_size,
                     question_offset, length, question_offset, length, user_data);

        ++parsed;
    }

    return parsed;
}

int mdns_discovery_answer(int sock, const void* address, size_t address_size, void* buffer, size_t capacity,
                          const char* record, size_t length) {
    if (capacity < (sizeof(mdns_services_query) + 32 + length)) return -1;

    uint16_t* data = (uint16_t*)buffer;
    // Basic reply structure
    memcpy(data, mdns_services_query, sizeof(mdns_services_query));
    // Flags
    uint16_t* flags = data + 1;
    *flags = htons(0x8400U);
    // One answer
    uint16_t* answers = data + 3;
    *answers = htons(1);

    // Fill in answer PTR record
    data = reinterpret_cast<uint16_t*>((char*)buffer + sizeof(mdns_services_query));
    // Reference _services._dns-sd._udp.local. string in question
    *data++ = htons(0xC000U | 12U);
    // Type
    *data++ = htons(MDNS_RECORDTYPE_PTR);
    // Rclass
    *data++ = htons(MDNS_CLASS_IN);
    // TTL
    *reinterpret_cast<uint32_t*>(data) = htonl(10);
    data += 2;
    // Record string length
    uint16_t* record_length = data++;
    uint8_t* record_data = (uint8_t*)data;
    size_t remain = capacity - (sizeof(mdns_services_query) + 10);
    record_data = (uint8_t*)mdns_string_make(record_data, remain, record, length);
    *record_length = htons((uint16_t)(record_data - (uint8_t*)data));
    *record_data++ = 0;

    ptrdiff_t tosend = (char*)record_data - (char*)buffer;
    return mdns_unicast_send(sock, address, address_size, buffer, (size_t)tosend);
}

int mdns_query_send(int sock, mdns_record_type_t type, const char* name, size_t length, void* buffer, size_t capacity,
                    uint16_t query_id) {
    if (capacity < (17 + length)) return -1;

    uint16_t rclass = MDNS_CLASS_IN | MDNS_UNICAST_RESPONSE;

    struct sockaddr_storage addr_storage;
    struct sockaddr* saddr = (struct sockaddr*)&addr_storage;
    socklen_t saddrlen = sizeof(addr_storage);
    if (getsockname(sock, saddr, &saddrlen) == 0) {
        if ((saddr->sa_family == AF_INET) &&
            (ntohs((reinterpret_cast<struct sockaddr_in*>(saddr))->sin_port) == MDNS_PORT))
            rclass &= ~MDNS_UNICAST_RESPONSE;
        else if ((saddr->sa_family == AF_INET6) &&
                 (ntohs((reinterpret_cast<struct sockaddr_in6*>(saddr))->sin6_port) == MDNS_PORT))
            rclass &= ~MDNS_UNICAST_RESPONSE;
    }

    uint16_t* data = (uint16_t*)buffer;
    // Query ID
    *data++ = htons(query_id);
    // Flags
    *data++ = 0;
    // Questions
    *data++ = htons(1);
    // No answer, authority or additional RRs
    *data++ = 0;
    *data++ = 0;
    *data++ = 0;
    // Fill in question
    // Name string
    data = (uint16_t*)mdns_string_make(data, capacity - 17, name, length);
    if (!data) return -1;
    // Record type
    *data++ = htons(type);
    //! Optional unicast response based on local port, class IN
    *data++ = htons(rclass);

    ptrdiff_t tosend = (char*)data - (char*)buffer;
    if (mdns_multicast_send(sock, buffer, (size_t)tosend)) return -1;
    return query_id;
}

size_t mdns_query_recv(int sock, void* buffer, size_t capacity, mdns_record_callback_fn callback, void* user_data,
                       int only_query_id) {
    struct sockaddr_in6 addr;
    struct sockaddr* saddr = (struct sockaddr*)&addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
#ifdef __APPLE__
    saddr->sa_len = sizeof(addr);
#endif
    int ret = (int)recvfrom(sock, (char*)buffer, (mdns_size_t)capacity, 0, saddr, &addrlen);
    if (ret <= 0) return 0;

    size_t data_size = (size_t)ret;
    uint16_t* data = (uint16_t*)buffer;

    uint16_t query_id = ntohs(*data++);
    uint16_t flags = ntohs(*data++);
    uint16_t questions = ntohs(*data++);
    uint16_t answer_rrs = ntohs(*data++);
    uint16_t authority_rrs = ntohs(*data++);
    uint16_t additional_rrs = ntohs(*data++);
    (void)sizeof(flags);

    if ((only_query_id > 0) && (query_id != only_query_id)) return 0;  // Not a reply to the wanted one-shot query

    if (questions > 1) return 0;

    // Skip questions part
    int i;
    for (i = 0; i < questions; ++i) {
        size_t ofs = (size_t)((char*)data - (char*)buffer);
        if (!mdns_string_skip(buffer, data_size, &ofs)) return 0;
        data = reinterpret_cast<uint16_t*>((char*)buffer + ofs);
        uint16_t rtype = ntohs(*data++);
        uint16_t rclass = ntohs(*data++);
        (void)sizeof(rtype);
        (void)sizeof(rclass);
    }

    size_t records = 0;
    size_t offset = MDNS_POINTER_DIFF(data, buffer);
    records += mdns_records_parse(sock, saddr, addrlen, buffer, data_size, &offset, MDNS_ENTRYTYPE_ANSWER, query_id,
                                  answer_rrs, callback, user_data);
    records += mdns_records_parse(sock, saddr, addrlen, buffer, data_size, &offset, MDNS_ENTRYTYPE_AUTHORITY, query_id,
                                  authority_rrs, callback, user_data);
    records += mdns_records_parse(sock, saddr, addrlen, buffer, data_size, &offset, MDNS_ENTRYTYPE_ADDITIONAL, query_id,
                                  additional_rrs, callback, user_data);
    return records;
}

int mdns_query_answer(int sock, const void* address, size_t address_size, void* buffer, size_t capacity,
                      uint16_t query_id, const char* service, size_t service_length, const char* hostname,
                      size_t hostname_length, uint32_t ipv4, const uint8_t* ipv6, uint16_t port, const char* txt,
                      size_t txt_length) {
    if (capacity < (sizeof(struct mdns_header_t) + 32 + service_length + hostname_length)) return -1;

    int unicast = (address_size ? 1 : 0);
    int use_ipv4 = (ipv4 != 0);
    int use_ipv6 = (ipv6 != nullptr);
    int use_txt = (txt && txt_length && (txt_length <= 255));

    uint16_t question_rclass = (unicast ? MDNS_UNICAST_RESPONSE : 0) | MDNS_CLASS_IN;
    uint16_t rclass = (unicast ? MDNS_CACHE_FLUSH : 0) | MDNS_CLASS_IN;
    uint32_t ttl = (unicast ? 10 : 60);
    uint32_t a_ttl = ttl;

    // Basic answer structure
    struct mdns_header_t* header = (struct mdns_header_t*)buffer;
    header->query_id = (address_size ? htons(query_id) : 0);
    header->flags = htons(0x8400);
    header->questions = htons(unicast ? 1 : 0);
    header->answer_rrs = htons(1);
    header->authority_rrs = 0;
    header->additional_rrs = htons((unsigned short)(1 + use_ipv4 + use_ipv6 + use_txt));

    void* data = MDNS_POINTER_OFFSET(buffer, sizeof(struct mdns_header_t));
    uint16_t* udata;
    size_t remain, service_offset = 0, local_offset = 0, full_offset, host_offset;

    // Fill in question if unicast
    if (unicast) {
        service_offset = MDNS_POINTER_DIFF(data, buffer);
        remain = capacity - service_offset;
        data = mdns_string_make(data, remain, service, service_length);
        local_offset = MDNS_POINTER_DIFF(data, buffer) - 7;
        remain = capacity - MDNS_POINTER_DIFF(data, buffer);
        if (!data || (remain <= 4)) return -1;

        udata = (uint16_t*)data;
        *udata++ = htons(MDNS_RECORDTYPE_PTR);
        *udata++ = htons(question_rclass);
        data = udata;
    }
    remain = capacity - MDNS_POINTER_DIFF(data, buffer);

    // Fill in answers
    // PTR record for service
    if (unicast) {
        data = mdns_string_make_ref(data, remain, service_offset);
    } else {
        service_offset = MDNS_POINTER_DIFF(data, buffer);
        remain = capacity - service_offset;
        data = mdns_string_make(data, remain, service, service_length);
        local_offset = MDNS_POINTER_DIFF(data, buffer) - 7;
    }
    remain = capacity - MDNS_POINTER_DIFF(data, buffer);
    if (!data || (remain <= 10)) return -1;
    udata = (uint16_t*)data;
    *udata++ = htons(MDNS_RECORDTYPE_PTR);
    *udata++ = htons(rclass);
    *reinterpret_cast<uint32_t*>(udata) = htonl(ttl);
    udata += 2;
    uint16_t* record_length = udata++;  // length
    data = udata;
    // Make a string <hostname>.<service>.local.
    full_offset = MDNS_POINTER_DIFF(data, buffer);
    remain = capacity - full_offset;
    data = mdns_string_make_with_ref(data, remain, hostname, hostname_length, service_offset);
    remain = capacity - MDNS_POINTER_DIFF(data, buffer);
    if (!data || (remain <= 10)) return -1;
    *record_length = htons((uint16_t)MDNS_POINTER_DIFF(data, record_length + 1));

    // Fill in additional records
    // SRV record for <hostname>.<service>.local.
    data = mdns_string_make_ref(data, remain, full_offset);
    remain = capacity - MDNS_POINTER_DIFF(data, buffer);
    if (!data || (remain <= 10)) return -1;
    udata = (uint16_t*)data;
    *udata++ = htons(MDNS_RECORDTYPE_SRV);
    *udata++ = htons(rclass);
    *reinterpret_cast<uint32_t*>(udata) = htonl(ttl);
    udata += 2;
    record_length = udata++;  // length
    *udata++ = htons(0);      // priority
    *udata++ = htons(0);      // weight
    *udata++ = htons(port);   // port
    // Make a string <hostname>.local.
    data = udata;
    host_offset = MDNS_POINTER_DIFF(data, buffer);
    remain = capacity - host_offset;
    data = mdns_string_make_with_ref(data, remain, hostname, hostname_length, local_offset);
    remain = capacity - MDNS_POINTER_DIFF(data, buffer);
    if (!data || (remain <= 10)) return -1;
    *record_length = htons((uint16_t)MDNS_POINTER_DIFF(data, record_length + 1));

    // A record for <hostname>.local.
    if (use_ipv4) {
        data = mdns_string_make_ref(data, remain, host_offset);
        remain = capacity - MDNS_POINTER_DIFF(data, buffer);
        if (!data || (remain <= 14)) return -1;
        udata = (uint16_t*)data;
        *udata++ = htons(MDNS_RECORDTYPE_A);
        *udata++ = htons(rclass);
        *reinterpret_cast<uint32_t*>(udata) = htonl(a_ttl);
        udata += 2;
        *udata++ = htons(4);                         // length
        *reinterpret_cast<uint32_t*>(udata) = ipv4;  // ipv4 address
        udata += 2;
        data = udata;
        remain = capacity - MDNS_POINTER_DIFF(data, buffer);
    }

    // AAAA record for <hostname>.local.
    if (use_ipv6) {
        data = mdns_string_make_ref(data, remain, host_offset);
        remain = capacity - MDNS_POINTER_DIFF(data, buffer);
        if (!data || (remain <= 26)) return -1;
        udata = (uint16_t*)data;
        *udata++ = htons(MDNS_RECORDTYPE_AAAA);
        *udata++ = htons(rclass);
        *reinterpret_cast<uint32_t*>(udata) = htonl(a_ttl);
        udata += 2;
        *udata++ = htons(16);     // length
        memcpy(udata, ipv6, 16);  // ipv6 address
        data = MDNS_POINTER_OFFSET(udata, 16);
        remain = capacity - MDNS_POINTER_DIFF(data, buffer);
    }

    // TXT record for <hostname>.<service>.local.
    if (use_txt) {
        data = mdns_string_make_ref(data, remain, full_offset);
        remain = capacity - MDNS_POINTER_DIFF(data, buffer);
        if (!data || (remain <= (11 + txt_length))) return -1;
        udata = (uint16_t*)data;
        *udata++ = htons(MDNS_RECORDTYPE_TXT);
        *udata++ = htons(rclass);
        *reinterpret_cast<uint32_t*>(udata) = htonl(ttl);
        udata += 2;
        *udata++ = htons((unsigned short)(txt_length + 1));  // length
        char* txt_record = (char*)udata;
        *txt_record++ = (char)txt_length;
        memcpy(txt_record, txt, txt_length);  // txt record
        data = MDNS_POINTER_OFFSET(txt_record, txt_length);
        // Unused until multiple txt records are supported
        // remain = capacity - MDNS_POINTER_DIFF(data, buffer);
    }

    size_t tosend = MDNS_POINTER_DIFF(data, buffer);
    if (address_size) return mdns_unicast_send(sock, address, address_size, buffer, tosend);
    return mdns_multicast_send(sock, buffer, tosend);
}

mdns_string_t mdns_record_parse_ptr(const void* buffer, size_t size, size_t offset, size_t length, char* strbuffer,
                                    size_t capacity) {
    // PTR record is just a string
    if ((size >= offset + length) && (length >= 2))
        return mdns_string_extract(buffer, size, &offset, strbuffer, capacity);
    mdns_string_t empty = {nullptr, 0};
    return empty;
}

mdns_record_srv_t mdns_record_parse_srv(const void* buffer, size_t size, size_t offset, size_t length, char* strbuffer,
                                        size_t capacity) {
    mdns_record_srv_t srv;
    memset(&srv, 0, sizeof(mdns_record_srv_t));
    // Read the priority, weight, port number and the discovery name
    // SRV record format (http://www.ietf.org/rfc/rfc2782.txt):
    // 2 bytes network-order unsigned priority
    // 2 bytes network-order unsigned weight
    // 2 bytes network-order unsigned port
    // string: discovery (domain) name, minimum 2 bytes when compressed
    if ((size >= offset + length) && (length >= 8)) {
        const uint16_t* recorddata = reinterpret_cast<uint16_t*>((char*)buffer + offset);
        srv.priority = ntohs(*recorddata++);
        srv.weight = ntohs(*recorddata++);
        srv.port = ntohs(*recorddata++);
        offset += 6;
        srv.name = mdns_string_extract(buffer, size, &offset, strbuffer, capacity);
    }
    return srv;
}

struct sockaddr_in* mdns_record_parse_a(const void* buffer, size_t size, size_t offset, size_t length,
                                        struct sockaddr_in* addr) {
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
#ifdef __APPLE__
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    if ((size >= offset + length) && (length == 4))
        addr->sin_addr.s_addr = *reinterpret_cast<uint16_t*>((char*)buffer + offset);
    return addr;
}

struct sockaddr_in6* mdns_record_parse_aaaa(const void* buffer, size_t size, size_t offset, size_t length,
                                            struct sockaddr_in6* addr) {
    memset(addr, 0, sizeof(struct sockaddr_in6));
    addr->sin6_family = AF_INET6;
#ifdef __APPLE__
    addr->sin6_len = sizeof(struct sockaddr_in6);
#endif
    if ((size >= offset + length) && (length == 16))
        addr->sin6_addr = *reinterpret_cast<const struct in6_addr*>((char*)buffer + offset);
    return addr;
}

size_t mdns_record_parse_txt(const void* buffer, size_t size, size_t offset, size_t length, mdns_record_txt_t* records,
                             size_t capacity) {
    size_t parsed = 0;
    const char* strdata;
    size_t separator, sublength;
    size_t end = offset + length;

    if (size < end) end = size;

    while ((offset < end) && (parsed < capacity)) {
        strdata = (const char*)buffer + offset;
        sublength = *(const unsigned char*)strdata;

        ++strdata;
        offset += sublength + 1;

        separator = 0;
        for (size_t c = 0; c < sublength; ++c) {
            // DNS-SD TXT record keys MUST be printable US-ASCII, [0x20, 0x7E]
            if ((strdata[c] < 0x20) || (strdata[c] > 0x7E)) break;
            if (strdata[c] == '=') {
                separator = c;
                break;
            }
        }

        if (!separator) continue;

        if (separator < sublength) {
            records[parsed].key.str = strdata;
            records[parsed].key.length = separator;
            records[parsed].value.str = strdata + separator + 1;
            records[parsed].value.length = sublength - (separator + 1);
        } else {
            records[parsed].key.str = strdata;
            records[parsed].key.length = sublength;
        }

        ++parsed;
    }

    return parsed;
}
