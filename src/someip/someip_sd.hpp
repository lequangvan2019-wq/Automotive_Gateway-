#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <arpa/inet.h>

constexpr auto     SOMEIP_SD_MULTICAST = "224.224.224.245";
constexpr uint16_t SOMEIP_SD_PORT      = 30490;

constexpr uint8_t SD_ENTRY_FIND_SERVICE       = 0x00;
constexpr uint8_t SD_ENTRY_OFFER_SERVICE      = 0x01;
constexpr uint8_t SD_ENTRY_SUBSCRIBE_EVENTGRP = 0x06;
constexpr uint8_t SD_ENTRY_SUBSCRIBE_ACK      = 0x07;

constexpr uint8_t SD_OPT_IPV4_ENDPOINT  = 0x04;
constexpr uint8_t SD_OPT_IPV4_MULTICAST = 0x14;

// One SD entry — 16 bytes
struct SdEntry {
    uint8_t  type          = 0;
    uint8_t  index_first   = 0;
    uint8_t  num_opts      = 0;
    uint16_t service_id    = 0;
    uint16_t instance_id   = 0xFFFF;
    uint8_t  major_version = 0x01;
    uint32_t ttl           = 3;
    uint16_t minor_version = 0;
    uint16_t eventgroup_id = 0;

    void serialize(uint8_t* buf) const {
        buf[0] = type;
        buf[1] = index_first;
        buf[2] = (num_opts << 4);
        buf[3] = 0;
        uint16_t svc = htons(service_id);
        memcpy(buf + 4, &svc, 2);
        uint16_t ins = htons(instance_id);
        memcpy(buf + 6, &ins, 2);
        buf[8] = major_version;
        buf[9]  = (ttl >> 16) & 0xFF;
        buf[10] = (ttl >>  8) & 0xFF;
        buf[11] =  ttl        & 0xFF;
        uint16_t minor = htons(minor_version);
        memcpy(buf + 12, &minor, 2);
        uint16_t evg = htons(eventgroup_id);
        memcpy(buf + 14, &evg, 2);
    }
};

// IPv4 endpoint option — 12 bytes
struct SdOptionIpv4 {
    uint8_t  type      = SD_OPT_IPV4_ENDPOINT;
    uint32_t ipv4_addr = 0;
    uint8_t  protocol  = 0x11;  // 0x11=UDP
    uint16_t port      = 0;

    size_t serialize(uint8_t* buf) const {
        uint16_t len = htons(9);
        memcpy(buf, &len, 2);
        buf[2] = type;
        buf[3] = 0;
        memcpy(buf + 4, &ipv4_addr, 4);
        buf[8] = 0;
        buf[9] = protocol;
        uint16_t p = htons(port);
        memcpy(buf + 10, &p, 2);
        return 12;
    }
};

class SomeIpSdMessage {
public:
    void add_offer_service(uint16_t service_id,
                           uint32_t ipv4_addr,
                           uint16_t port,
                           uint32_t ttl = 3);

    void add_subscribe_ack(uint16_t service_id,
                           uint16_t eventgroup_id);

    std::vector<uint8_t> serialize(uint16_t session_id) const;

private:
    std::vector<SdEntry>      entries_;
    std::vector<SdOptionIpv4> options_;
};
