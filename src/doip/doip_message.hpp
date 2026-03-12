#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <arpa/inet.h>

// DoIP protocol version
constexpr uint8_t DOIP_PROTO_VERSION     = 0x02;
constexpr uint8_t DOIP_PROTO_VERSION_INV = 0xFD; // bitwise inverse

// DoIP payload types
constexpr uint16_t DOIP_ROUTING_ACTIVATION_REQ  = 0x0005;
constexpr uint16_t DOIP_ROUTING_ACTIVATION_RES  = 0x0006;
constexpr uint16_t DOIP_DIAGNOSTIC_MESSAGE      = 0x8001;
constexpr uint16_t DOIP_DIAGNOSTIC_ACK          = 0x8002;
constexpr uint16_t DOIP_DIAGNOSTIC_NACK         = 0x8003;
constexpr uint16_t DOIP_ALIVE_CHECK_REQ         = 0x0007;
constexpr uint16_t DOIP_ALIVE_CHECK_RES         = 0x0008;

// DoIP routing activation response codes
constexpr uint8_t DOIP_ROUTING_OK               = 0x10;
constexpr uint8_t DOIP_ROUTING_DENIED           = 0x00;

// DoIP diagnostic ACK codes
constexpr uint8_t DOIP_ACK_OK                   = 0x00;
constexpr uint8_t DOIP_NACK_UNKNOWN_SOURCE      = 0x02;

// Fixed DoIP header size = 8 bytes
constexpr size_t DOIP_HEADER_SIZE = 8;

// Our ECU logical address
constexpr uint16_t DOIP_ECU_ADDRESS = 0x0001;

struct DoIpHeader {
    uint8_t  proto_version     = DOIP_PROTO_VERSION;
    uint8_t  proto_version_inv = DOIP_PROTO_VERSION_INV;
    uint16_t payload_type      = 0;
    uint32_t payload_length    = 0;

    // Parse 8 bytes into header
    static bool parse(const uint8_t* buf, size_t len, DoIpHeader& out) {
        if (len < DOIP_HEADER_SIZE) return false;
        out.proto_version     = buf[0];
        out.proto_version_inv = buf[1];
        out.payload_type      = (buf[2] << 8) | buf[3];
        out.payload_length    = (buf[4] << 24) | (buf[5] << 16)
                              | (buf[6] <<  8) |  buf[7];
        return true;
    }

    // Serialize 8 bytes
    void serialize(uint8_t* buf) const {
        buf[0] = proto_version;
        buf[1] = proto_version_inv;
        buf[2] = (payload_type >> 8) & 0xFF;
        buf[3] =  payload_type       & 0xFF;
        uint32_t pl = htonl(payload_length);
        memcpy(buf + 4, &pl, 4);
    }
};

// Build a complete DoIP message
std::vector<uint8_t> make_doip_message(uint16_t payload_type,
                                        const uint8_t* payload,
                                        size_t payload_len);
