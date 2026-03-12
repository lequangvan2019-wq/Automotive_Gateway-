#include "doip_message.hpp"

std::vector<uint8_t> make_doip_message(uint16_t payload_type,
                                        const uint8_t* payload,
                                        size_t payload_len)
{
    std::vector<uint8_t> buf(DOIP_HEADER_SIZE + payload_len);
    uint8_t* p = buf.data();

    DoIpHeader hdr;
    hdr.payload_type   = payload_type;
    hdr.payload_length = static_cast<uint32_t>(payload_len);
    hdr.serialize(p);

    if (payload && payload_len > 0)
        memcpy(p + DOIP_HEADER_SIZE, payload, payload_len);

    return buf;
}
