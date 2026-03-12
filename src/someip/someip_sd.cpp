#include "someip_sd.hpp"
#include <cstring>

void SomeIpSdMessage::add_offer_service(uint16_t service_id,
                                         uint32_t ipv4_addr,
                                         uint16_t port,
                                         uint32_t ttl)
{
    SdEntry entry;
    entry.type          = SD_ENTRY_OFFER_SERVICE;
    entry.service_id    = service_id;
    entry.instance_id   = 0x0001;
    entry.major_version = 0x01;
    entry.ttl           = ttl;
    entry.minor_version = 0x0000;
    entry.index_first   = static_cast<uint8_t>(options_.size());
    entry.num_opts      = 1;
    entries_.push_back(entry);

    SdOptionIpv4 opt;
    opt.type      = SD_OPT_IPV4_ENDPOINT;
    opt.ipv4_addr = ipv4_addr;
    opt.protocol  = 0x11;
    opt.port      = port;
    options_.push_back(opt);
}

void SomeIpSdMessage::add_subscribe_ack(uint16_t service_id,
                                         uint16_t eventgroup_id)
{
    SdEntry entry;
    entry.type          = SD_ENTRY_SUBSCRIBE_ACK;
    entry.service_id    = service_id;
    entry.instance_id   = 0x0001;
    entry.major_version = 0x01;
    entry.ttl           = 3;
    entry.eventgroup_id = eventgroup_id;
    entry.num_opts      = 0;
    entries_.push_back(entry);
}

std::vector<uint8_t> SomeIpSdMessage::serialize(uint16_t session_id) const {
    size_t entries_len = entries_.size() * 16;
    size_t options_len = options_.size() * 12;
    size_t total = 16 + 4 + 4 + entries_len + 4 + options_len;

    std::vector<uint8_t> buf(total, 0);
    uint8_t* p = buf.data();

    // SOME/IP header
    p[0] = 0xFF; p[1] = 0xFF;  // Service ID = 0xFFFF
    p[2] = 0x81; p[3] = 0x00;  // Method ID = 0x8100 (SD)

    uint32_t length = htonl(static_cast<uint32_t>(total - 8));
    memcpy(p + 4, &length, 4);

    p[8]  = 0x00; p[9]  = 0x00;  // Client ID
    uint16_t sess = htons(session_id);
    memcpy(p + 10, &sess, 2);
    p[12] = 0x01;  // Protocol version
    p[13] = 0x01;  // Interface version
    p[14] = 0x02;  // Notification
    p[15] = 0x00;  // Return code OK

    size_t pos = 16;

    // SD flags
    p[pos++] = 0xC0;
    p[pos++] = 0x00;
    p[pos++] = 0x00;
    p[pos++] = 0x00;

    // Entries array length
    uint32_t elen = htonl(static_cast<uint32_t>(entries_len));
    memcpy(p + pos, &elen, 4);
    pos += 4;

    // Serialize entries
    for (const auto& e : entries_) {
        e.serialize(p + pos);
        pos += 16;
    }

    // Options array length
    uint32_t olen = htonl(static_cast<uint32_t>(options_len));
    memcpy(p + pos, &olen, 4);
    pos += 4;

    // Serialize options
    for (const auto& o : options_) {
        o.serialize(p + pos);
        pos += 12;
    }

    return buf;
}
