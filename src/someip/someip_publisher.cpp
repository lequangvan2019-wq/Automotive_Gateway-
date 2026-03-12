#include "someip_publisher.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

SomeIpPublisher::SomeIpPublisher(const std::string& multicast_addr,
                                  uint16_t port)
    : port_(port)
{
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0)
        throw std::runtime_error(
            "socket(UDP) failed: " + std::string(strerror(errno)));

    int reuse = 1;
    setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&dest_addr_, 0, sizeof(dest_addr_));
    dest_addr_.sin_family      = AF_INET;
    dest_addr_.sin_port        = htons(port);
    dest_addr_.sin_addr.s_addr = inet_addr(multicast_addr.c_str());

    int ttl = 1;
    setsockopt(sock_fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    std::cout << "[SOMEIP] Publisher ready -> "
              << multicast_addr << ":" << port << "\n";
}

SomeIpPublisher::~SomeIpPublisher() {
    if (sock_fd_ >= 0) close(sock_fd_);
}

void SomeIpPublisher::register_signal(const std::string& signal_name,
                                       uint16_t service_id,
                                       uint16_t method_id)
{
    signal_map_[signal_name] = {service_id, method_id};
    std::cout << "[SOMEIP] Registered: " << signal_name
              << " -> Service=0x" << std::hex << service_id
              << " Method=0x"     << method_id << std::dec << "\n";
}

void SomeIpPublisher::publish(const DecodedSignal& signal) {
    auto it = signal_map_.find(signal.name);
    if (it == signal_map_.end()) return;

    uint16_t service_id = it->second.first;
    uint16_t method_id  = it->second.second;

    auto msg = SomeIpMessage::make_float_notification(
        service_id, method_id, session_++,
        static_cast<float>(signal.value));

    send_message(msg);
    // verbose print removed — see stats table for aggregated results
}

void SomeIpPublisher::publish_float(uint16_t service_id,
                                     uint16_t method_id,
                                     float value)
{
    auto msg = SomeIpMessage::make_float_notification(
        service_id, method_id, session_++, value);
    send_message(msg);
}

void SomeIpPublisher::send_message(const SomeIpMessage& msg) {
    // Fixed buffer on the stack — no heap allocation
    uint8_t buf[SOMEIP_MAX_SIZE];
    size_t  len = msg.serialize_into(buf, sizeof(buf));
    if (len == 0) {
        std::cerr << "[SOMEIP] Message too large to serialize\n";
        return;
    }
    ssize_t sent = sendto(sock_fd_,
                          buf, len, 0,
                          reinterpret_cast<sockaddr*>(&dest_addr_),
                          sizeof(dest_addr_));
    if (sent < 0)
        std::cerr << "[SOMEIP] sendto failed: " << strerror(errno) << "\n";
}
