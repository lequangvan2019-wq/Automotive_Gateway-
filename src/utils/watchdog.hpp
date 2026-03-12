#pragma once
#include "../bench/benchmark.hpp"
#include <cstdint>
#include <functional>
#include <iostream>

class Watchdog {
public:
    // timeout_us: how long without a frame before alerting
    explicit Watchdog(uint64_t timeout_us = 2000000)
        : timeout_us_(timeout_us) {}

    // Call this every time a CAN frame arrives
    void feed() {
        last_feed_ns_ = now_ns();
        fired_        = false;
    }

    // Call this periodically from the event loop or main loop
    // Returns true if timeout has been exceeded
    bool check() {
        if (last_feed_ns_ == 0) return false;
        uint64_t elapsed = (now_ns() - last_feed_ns_) / 1000;
        if (elapsed > timeout_us_ && !fired_) {
            fired_ = true;
            std::cerr << "[WATCHDOG] CAN bus silent for "
                      << elapsed / 1000 << "ms — check connection\n";
            return true;
        }
        return false;
    }

    void reset() {
        last_feed_ns_ = 0;
        fired_        = false;
    }

private:
    uint64_t timeout_us_   = 2000000;
    uint64_t last_feed_ns_ = 0;
    bool     fired_        = false;
};
