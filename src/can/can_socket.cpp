#include "can_socket.hpp"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <fcntl.h>

CanSocket::CanSocket(const std::string& interface)
    : interface_(interface)
{
    // AF_CAN = CAN socket family
    // SOCK_RAW = raw frames (not multiplexed)
    // CAN_RAW = raw CAN protocol
    fd_ = socket(AF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0)
        throw std::runtime_error(
            "socket(AF_CAN) failed: " + std::string(strerror(errno)));

    // Set non-blocking so epoll works correctly
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

    // Bind to the named interface (e.g. vcan0)
    struct ifreq ifr{};
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    if (ioctl(fd_, SIOCGIFINDEX, &ifr) < 0)
        throw std::runtime_error(
            "ioctl SIOCGIFINDEX failed for " + interface +
            ": " + strerror(errno));

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(
            "bind(AF_CAN) failed: " + std::string(strerror(errno)));

    std::cout << "[CAN] Opened " << interface << " (fd=" << fd_ << ")\n";
}

CanSocket::~CanSocket() {
    if (fd_ >= 0) close(fd_);
}

void CanSocket::register_with_loop(EventLoop& loop, OnCanFrame cb) {
    on_frame_ = std::move(cb);
    loop.add_reader(fd_, [this]() { on_readable(); });
}

void CanSocket::on_readable() {
    can_frame frame{};
    ssize_t n = read(fd_, &frame, sizeof(frame));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        std::cerr << "[CAN] read error: " << strerror(errno) << "\n";
        return;
    }
    if (n < static_cast<ssize_t>(sizeof(can_frame))) return;

    if (on_frame_) on_frame_(frame);
}

void CanSocket::send(const can_frame& frame) {
    ssize_t n = write(fd_, &frame, sizeof(frame));
    if (n < 0)
        std::cerr << "[CAN] send error: " << strerror(errno) << "\n";
}

void CanSocket::add_filter(uint32_t can_id, uint32_t mask) {
    struct can_filter filter{};
    filter.can_id   = can_id;
    filter.can_mask = mask;
    setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter));
}
