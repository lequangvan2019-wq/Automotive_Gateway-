#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <arpa/inet.h>

constexpr uint8_t SOMEIP_MSG_NOTIFICATION = 0x02;
constexpr uint8_t SOMEIP_RET_OK           = 0x00;
constexpr size_t  SOMEIP_HEADER_SIZE      = 16;
constexpr size_t  SOMEIP_MAX_SIZE         = 1500; // max UDP payload

class SomeIpMessage {
public:
    static SomeIpMessage make_notification(uint16_t service_id,
                                            uint16_t method_id,
                                            uint16_t session_id,
                                            const uint8_t* payload,
                                            size_t payload_len);

    static SomeIpMessage make_float_notification(uint16_t service_id,
                                                  uint16_t method_id,
                                                  uint16_t session_id,
                                                  float value);

    static SomeIpMessage make_uint32_notification(uint16_t service_id,
                                                   uint16_t method_id,
                                                   uint16_t session_id,
                                                   uint32_t value);

    // Zero-copy: write directly into provided buffer
    // Returns bytes written, 0 if buffer too small
    size_t serialize_into(uint8_t* buf, size_t buf_len) const;

    // Allocating version — kept for compatibility
    std::vector<uint8_t> serialize() const;

    uint16_t service_id()   const { return service_id_; }
    uint16_t method_id()    const { return method_id_; }
    uint16_t session_id()   const { return session_id_; }
    size_t   payload_size() const { return payload_.size(); }

private:
    uint16_t             service_id_  = 0;
    uint16_t             method_id_   = 0;
    uint16_t             client_id_   = 0;
    uint16_t             session_id_  = 0;
    uint8_t              msg_type_    = SOMEIP_MSG_NOTIFICATION;
    uint8_t              return_code_ = SOMEIP_RET_OK;
    std::vector<uint8_t> payload_;
};
