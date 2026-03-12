#pragma once
#include "someip_sd.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <stdexcept>

class EventLoop;

struct OfferedService {
    uint16_t service_id;
    uint32_t ipv4_addr;
    uint16_t port;
};

struct Subscriber {
    sockaddr_in addr;
    uint16_t    service_id;
    uint16_t    eventgroup_id;
};

class SomeIpSdService {
public:
    SomeIpSdService();
    ~SomeIpSdService();

    void register_with_loop(EventLoop& loop);

    void offer_service(uint16_t service_id,
                       const std::string& local_ip,
                       uint16_t port);

    void send_offers();

    const std::vector<Subscriber>& subscribers() const {
        return subscribers_;
    }

    std::function<void(const Subscriber&)> on_subscribe;

private:
    int      sock_fd_ = -1;
    uint16_t session_ = 1;

    std::vector<OfferedService> offered_;
    std::vector<Subscriber>     subscribers_;

    void on_readable();
    void handle_sd_message(const uint8_t* buf, size_t len,
                           const sockaddr_in& from);
    void send_subscribe_ack(const sockaddr_in& to,
                            uint16_t service_id,
                            uint16_t eventgroup_id);
};
