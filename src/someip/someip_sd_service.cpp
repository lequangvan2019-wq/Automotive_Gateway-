#include "someip_sd_service.hpp"
#include "../event/event_loop.hpp"
#include <fcntl.h>

SomeIpSdService::SomeIpSdService() {
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0)
        throw std::runtime_error(
            "SD socket failed: " + std::string(strerror(errno)));

    int reuse = 1;
    setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Non-blocking
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

    // Bind to SD port
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SOMEIP_SD_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock_fd_,
             reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(
            "SD bind failed: " + std::string(strerror(errno)));

    // Join SD multicast group
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(SOMEIP_SD_MULTICAST);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(sock_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
               &mreq, sizeof(mreq));

    std::cout << "[SD] Listening on "
              << SOMEIP_SD_MULTICAST << ":" << SOMEIP_SD_PORT << "\n";
}

SomeIpSdService::~SomeIpSdService() {
    if (sock_fd_ >= 0) close(sock_fd_);
}

void SomeIpSdService::register_with_loop(EventLoop& loop) {
    loop.add_reader(sock_fd_, [this]() { on_readable(); });
}

void SomeIpSdService::offer_service(uint16_t service_id,
                                     const std::string& local_ip,
                                     uint16_t port)
{
    OfferedService svc;
    svc.service_id = service_id;
    svc.ipv4_addr  = inet_addr(local_ip.c_str());
    svc.port       = port;
    offered_.push_back(svc);
    std::cout << "[SD] Offering Service=0x"
              << std::hex << service_id << std::dec
              << " on " << local_ip << ":" << port << "\n";
}

void SomeIpSdService::send_offers() {
    if (offered_.empty()) return;

    SomeIpSdMessage sd_msg;
    for (const auto& svc : offered_)
        sd_msg.add_offer_service(svc.service_id,
                                  svc.ipv4_addr,
                                  svc.port);

    auto bytes = sd_msg.serialize(session_++);

    sockaddr_in dest{};
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons(SOMEIP_SD_PORT);
    dest.sin_addr.s_addr = inet_addr(SOMEIP_SD_MULTICAST);

    sendto(sock_fd_, bytes.data(), bytes.size(), 0,
           reinterpret_cast<sockaddr*>(&dest), sizeof(dest));

    std::cout << "[SD] Sent OfferService for "
              << offered_.size() << " services\n";
}

void SomeIpSdService::on_readable() {
    uint8_t buf[1500];
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);

    ssize_t n = recvfrom(sock_fd_, buf, sizeof(buf), 0,
                          reinterpret_cast<sockaddr*>(&from),
                          &from_len);
    if (n < 0) return;

    handle_sd_message(buf, static_cast<size_t>(n), from);
}

void SomeIpSdService::handle_sd_message(const uint8_t* buf,
                                         size_t len,
                                         const sockaddr_in& from)
{
    if (len < 28) return;

    // Verify SOME/IP-SD header
    uint16_t svc_id = (buf[0] << 8) | buf[1];
    uint16_t met_id = (buf[2] << 8) | buf[3];
    if (svc_id != 0xFFFF || met_id != 0x8100) return;

    size_t pos = 16 + 4;  // skip SOME/IP header + SD flags

    uint32_t entries_len;
    memcpy(&entries_len, buf + pos, 4);
    entries_len = ntohl(entries_len);
    pos += 4;

    size_t entries_end = pos + entries_len;
    while (pos + 16 <= entries_end && pos + 16 <= len) {
        uint8_t entry_type = buf[pos];

        uint16_t entry_svc_id;
        memcpy(&entry_svc_id, buf + pos + 4, 2);
        entry_svc_id = ntohs(entry_svc_id);

        uint16_t eventgroup_id;
        memcpy(&eventgroup_id, buf + pos + 14, 2);
        eventgroup_id = ntohs(eventgroup_id);

        if (entry_type == SD_ENTRY_SUBSCRIBE_EVENTGRP) {
            std::cout << "[SD] SubscribeEventgroup "
                      << "Service=0x" << std::hex << entry_svc_id
                      << " EventGroup=0x" << eventgroup_id
                      << " from " << inet_ntoa(from.sin_addr)
                      << std::dec << "\n";

            Subscriber sub;
            sub.addr          = from;
            sub.service_id    = entry_svc_id;
            sub.eventgroup_id = eventgroup_id;
            subscribers_.push_back(sub);

            send_subscribe_ack(from, entry_svc_id, eventgroup_id);

            if (on_subscribe) on_subscribe(sub);
        }

        pos += 16;
    }
}

void SomeIpSdService::send_subscribe_ack(const sockaddr_in& to,
                                          uint16_t service_id,
                                          uint16_t eventgroup_id)
{
    SomeIpSdMessage ack;
    ack.add_subscribe_ack(service_id, eventgroup_id);
    auto bytes = ack.serialize(session_++);

    sendto(sock_fd_, bytes.data(), bytes.size(), 0,
           reinterpret_cast<const sockaddr*>(&to), sizeof(to));

    std::cout << "[SD] Sent SubscribeAck to "
              << inet_ntoa(to.sin_addr) << "\n";
}
