#include "someip_message.hpp"
#include <cstring>

SomeIpMessage SomeIpMessage::make_notification(uint16_t service_id,
                                                uint16_t method_id,
                                                uint16_t session_id,
                                                const uint8_t* payload,
                                                size_t payload_len)
{
    SomeIpMessage msg;
    msg.service_id_  = service_id;
    msg.method_id_   = method_id;
    msg.client_id_   = 0x0000;
    msg.session_id_  = session_id;
    msg.msg_type_    = SOMEIP_MSG_NOTIFICATION;
    msg.return_code_ = SOMEIP_RET_OK;
    msg.payload_.assign(payload, payload + payload_len);
    return msg;
}

SomeIpMessage SomeIpMessage::make_float_notification(uint16_t service_id,
                                                      uint16_t method_id,
                                                      uint16_t session_id,
                                                      float value)
{
    uint32_t raw;
    memcpy(&raw, &value, sizeof(float));
    raw = htonl(raw);
    uint8_t buf[4];
    memcpy(buf, &raw, 4);
    return make_notification(service_id, method_id, session_id, buf, 4);
}

SomeIpMessage SomeIpMessage::make_uint32_notification(uint16_t service_id,
                                                       uint16_t method_id,
                                                       uint16_t session_id,
                                                       uint32_t value)
{
    uint32_t net = htonl(value);
    uint8_t buf[4];
    memcpy(buf, &net, 4);
    return make_notification(service_id, method_id, session_id, buf, 4);
}

// Zero-copy serialize — writes directly into caller-provided buffer
// Returns number of bytes written, 0 if buffer too small
size_t SomeIpMessage::serialize_into(uint8_t* buf, size_t buf_len) const {
    size_t total = SOMEIP_HEADER_SIZE + payload_.size();
    if (buf_len < total) return 0;

    buf[0] = (service_id_ >> 8) & 0xFF;
    buf[1] =  service_id_       & 0xFF;
    buf[2] = (method_id_  >> 8) & 0xFF;
    buf[3] =  method_id_        & 0xFF;

    uint32_t length = static_cast<uint32_t>(payload_.size() + 8);
    buf[4] = (length >> 24) & 0xFF;
    buf[5] = (length >> 16) & 0xFF;
    buf[6] = (length >>  8) & 0xFF;
    buf[7] =  length        & 0xFF;

    buf[8]  = (client_id_  >> 8) & 0xFF;
    buf[9]  =  client_id_        & 0xFF;
    buf[10] = (session_id_ >> 8) & 0xFF;
    buf[11] =  session_id_       & 0xFF;
    buf[12] = 0x01;  // protocol version
    buf[13] = 0x01;  // interface version
    buf[14] = msg_type_;
    buf[15] = return_code_;

    if (!payload_.empty())
        memcpy(buf + SOMEIP_HEADER_SIZE,
               payload_.data(), payload_.size());

    return total;
}

// Keep original serialize() for compatibility
std::vector<uint8_t> SomeIpMessage::serialize() const {
    std::vector<uint8_t> buf(SOMEIP_HEADER_SIZE + payload_.size());
    serialize_into(buf.data(), buf.size());
    return buf;
}
