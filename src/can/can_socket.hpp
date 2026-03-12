#pragma once
#include "../event/event_loop.hpp"
#include <string>
#include <functional>
#include <linux/can.h>      // can_frame
#include <linux/can/raw.h>  // CAN_RAW

// Callback fired when a CAN frame arrives
using OnCanFrame = std::function<void(const can_frame&)>;

class CanSocket {
public:
    // Opens a raw CAN socket on the given interface (e.g. "vcan0")
    explicit CanSocket(const std::string& interface);
    ~CanSocket();

    // Register with event loop — cb fired on every incoming frame
    void register_with_loop(EventLoop& loop, OnCanFrame cb);

    // Send a CAN frame
    void send(const can_frame& frame);

    // Add a kernel-level filter — only receive these CAN IDs
    // Much more efficient than filtering in userspace
    void add_filter(uint32_t can_id, uint32_t mask = CAN_SFF_MASK);

    int fd() const { return fd_; }

private:
    int        fd_ = -1;
    std::string interface_;
    OnCanFrame on_frame_;

    void on_readable();
};

