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

#ifndef MDNS_H
#define MDNS_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MDNS_INVALID_POS ((size_t)-1)

#define MDNS_STRING_CONST(s) (s), (sizeof((s)) - 1)
#define MDNS_STRING_FORMAT(s) (int)((s).length), s.str

#define MDNS_POINTER_OFFSET(p, ofs) ((void*)((char*)(p) + (ptrdiff_t)(ofs)))
#define MDNS_POINTER_OFFSET_CONST(p, ofs) ((const void*)((const char*)(p) + (ptrdiff_t)(ofs)))
#define MDNS_POINTER_DIFF(a, b) ((size_t)((const char*)(a) - (const char*)(b)))

#define MDNS_PORT 5353
#define MDNS_UNICAST_RESPONSE 0x8000U
#define MDNS_CACHE_FLUSH 0x8000U

enum mdns_record_type : uint16_t {
    MDNS_RECORDTYPE_IGNORE = 0,
    // Address
    MDNS_RECORDTYPE_A = 1,
    // Domain Name pointer
    MDNS_RECORDTYPE_PTR = 12,
    // Arbitrary text string
    MDNS_RECORDTYPE_TXT = 16,
    // IP6 Address [Thomson]
    MDNS_RECORDTYPE_AAAA = 28,
    // Server Selection [RFC2782]
    MDNS_RECORDTYPE_SRV = 33
};

enum mdns_entry_type : uint16_t {
    MDNS_ENTRYTYPE_QUESTION = 0,
    MDNS_ENTRYTYPE_ANSWER = 1,
    MDNS_ENTRYTYPE_AUTHORITY = 2,
    MDNS_ENTRYTYPE_ADDITIONAL = 3
};

enum mdns_class : uint16_t { MDNS_CLASS_IN = 1 };

typedef enum mdns_record_type mdns_record_type_t;
typedef enum mdns_entry_type mdns_entry_type_t;
typedef enum mdns_class mdns_class_t;

typedef int (*mdns_record_callback_fn)(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
                                       uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl,
                                       const void* data, size_t size, size_t name_offset, size_t name_length,
                                       size_t record_offset, size_t record_length, void* user_data);

typedef struct mdns_string_t mdns_string_t;
typedef struct mdns_string_pair_t mdns_string_pair_t;
typedef struct mdns_record_srv_t mdns_record_srv_t;
typedef struct mdns_record_txt_t mdns_record_txt_t;

#ifdef JUCE_WINDOWS
typedef int mdns_size_t;
#else
typedef size_t mdns_size_t;
#endif

struct mdns_string_t {
    const char* str;
    size_t length;
};

struct mdns_string_pair_t {
    size_t offset;
    size_t length;
    int ref;
};

struct mdns_record_srv_t {
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    mdns_string_t name;
};

struct mdns_record_txt_t {
    mdns_string_t key;
    mdns_string_t value;
};

struct mdns_header_t {
    uint16_t query_id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answer_rrs;
    uint16_t authority_rrs;
    uint16_t additional_rrs;
};

// mDNS/DNS-SD public API

//! Open and setup a IPv4 socket for mDNS/DNS-SD. To bind the socket to a specific interface,
//  pass in the appropriate socket address in saddr, otherwise pass a null pointer for INADDR_ANY.
//  To send one-shot discovery requests and queries pass a null pointer or set 0 as port to assign
//  a random user level ephemeral port. To run discovery service listening for incoming
//  discoveries and queries, you must set MDNS_PORT as port.
int mdns_socket_open_ipv4(struct sockaddr_in* saddr);

//! Setup an already opened IPv4 socket for mDNS/DNS-SD. To bind the socket to a specific interface,
//  pass in the appropriate socket address in saddr, otherwise pass a null pointer for INADDR_ANY.
//  To send one-shot discovery requests and queries pass a null pointer or set 0 as port to assign
//  a random user level ephemeral port. To run discovery service listening for incoming
//  discoveries and queries, you must set MDNS_PORT as port.
int mdns_socket_setup_ipv4(int sock, struct sockaddr_in* saddr);

//! Open and setup a IPv6 socket for mDNS/DNS-SD. To bind the socket to a specific interface,
//  pass in the appropriate socket address in saddr, otherwise pass a null pointer for in6addr_any.
//  To send one-shot discovery requests and queries pass a null pointer or set 0 as port to assign
//  a random user level ephemeral port. To run discovery service listening for incoming
//  discoveries and queries, you must set MDNS_PORT as port.
int mdns_socket_open_ipv6(struct sockaddr_in6* saddr);

//! Setup an already opened IPv6 socket for mDNS/DNS-SD. To bind the socket to a specific interface,
//  pass in the appropriate socket address in saddr, otherwise pass a null pointer for in6addr_any.
//  To send one-shot discovery requests and queries pass a null pointer or set 0 as port to assign
//  a random user level ephemeral port. To run discovery service listening for incoming
//  discoveries and queries, you must set MDNS_PORT as port.
int mdns_socket_setup_ipv6(int sock, struct sockaddr_in6* saddr);

//! Close a socket opened with mdns_socket_open_ipv4 and mdns_socket_open_ipv6.
void mdns_socket_close(int sock);

//! Listen for incoming multicast DNS-SD and mDNS query requests. The socket should have been
//  opened on port MDNS_PORT using one of the mdns open or setup socket functions. Returns the
//  number of queries  parsed.
size_t mdns_socket_listen(int sock, void* buffer, size_t capacity, mdns_record_callback_fn callback, void* user_data);

//! Send a multicast DNS-SD reqeuest on the given socket to discover available services. Returns
//  0 on success, or <0 if error.
int mdns_discovery_send(int sock);

//! Recieve unicast responses to a DNS-SD sent with mdns_discovery_send. Any data will be piped to
//  the given callback for parsing. Returns the number of responses parsed.
size_t mdns_discovery_recv(int sock, void* buffer, size_t capacity, mdns_record_callback_fn callback, void* user_data);

//! Send a unicast DNS-SD answer with a single record to the given address. Returns 0 if success,
//  or <0 if error.
int mdns_discovery_answer(int sock, const void* address, size_t address_size, void* buffer, size_t capacity,
                          const char* record, size_t length);

//! Send a multicast mDNS query on the given socket for the given service name. The supplied buffer
//  will be used to build the query packet. The query ID can be set to non-zero to filter responses,
//  however the RFC states that the query ID SHOULD be set to 0 for multicast queries. The query
//  will request a unicast response if the socket is bound to an ephemeral port, or a multicast
//  response if the socket is bound to mDNS port 5353.
//  Returns the used query ID, or <0 if error.
int mdns_query_send(int sock, mdns_record_type_t type, const char* name, size_t length, void* buffer, size_t capacity,
                    uint16_t query_id);

//! Receive unicast responses to a mDNS query sent with mdns_discovery_recv, optionally filtering
//  out any responses not matching the given query ID. Set the query ID to 0 to parse
//  all responses, even if it is not matching the query ID set in a specific query. Any data will
//  be piped to the given callback for parsing. Returns the number of responses parsed.
size_t mdns_query_recv(int sock, void* buffer, size_t capacity, mdns_record_callback_fn callback, void* user_data,
                       int query_id);

//! Send a unicast or multicast mDNS query answer with a single record to the given address. The
//  answer will be sent multicast if address size is 0, otherwise it will be sent unicast to the
//  given address. Use the top bit of the query class field (MDNS_UNICAST_RESPONSE) to determine
//  if the answer should be sent unicast (bit set) or multicast (bit not set).
//  Returns 0 if success, or <0 if error.
int mdns_query_answer(int sock, const void* address, size_t address_size, void* buffer, size_t capacity,
                      uint16_t query_id, const char* service, size_t service_length, const char* hostname,
                      size_t hostname_length, uint32_t ipv4, const uint8_t* ipv6, uint16_t port, const char* txt,
                      size_t txt_length);

// Internal functions

mdns_string_t mdns_string_extract(const void* buffer, size_t size, size_t* offset, char* str, size_t capacity);

int mdns_string_skip(const void* buffer, size_t size, size_t* offset);

int mdns_string_equal(const void* buffer_lhs, size_t size_lhs, size_t* ofs_lhs, const void* buffer_rhs, size_t size_rhs,
                      size_t* ofs_rhs);

void* mdns_string_make(void* data, size_t capacity, const char* name, size_t length);

void* mdns_string_make_ref(void* data, size_t capacity, size_t ref_offset);

void* mdns_string_make_with_ref(void* data, size_t capacity, const char* name, size_t length, size_t ref_offset);

mdns_string_t mdns_record_parse_ptr(const void* buffer, size_t size, size_t offset, size_t length, char* strbuffer,
                                    size_t capacity);

mdns_record_srv_t mdns_record_parse_srv(const void* buffer, size_t size, size_t offset, size_t length, char* strbuffer,
                                        size_t capacity);

struct sockaddr_in* mdns_record_parse_a(const void* buffer, size_t size, size_t offset, size_t length,
                                        struct sockaddr_in* addr);

struct sockaddr_in6* mdns_record_parse_aaaa(const void* buffer, size_t size, size_t offset, size_t length,
                                            struct sockaddr_in6* addr);

size_t mdns_record_parse_txt(const void* buffer, size_t size, size_t offset, size_t length, mdns_record_txt_t* records,
                             size_t capacity);

#endif
