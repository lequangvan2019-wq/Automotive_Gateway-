#pragma once
#include "someip_message.hpp"
#include "../../include/gateway/types.hpp"
#include <string>
#include <unordered_map>
#include <cstdint>

// Maps a signal name to its SOME/IP service/method IDs
struct SignalServiceMap {
    std::string signal_name;
    uint16_t    service_id;
    uint16_t    method_id;
};

class SomeIpPublisher {
public:
    // multicast_addr: e.g. "239.0.0.1"
    // port: e.g. 30490 (SOME/IP default)
    SomeIpPublisher(const std::string& multicast_addr, uint16_t port);
    ~SomeIpPublisher();

    // Register a signal -> service/method mapping
    void register_signal(const std::string& signal_name,
                         uint16_t service_id,
                         uint16_t method_id);

    // Publish a decoded signal as a SOME/IP notification
    // Does nothing if the signal has no registered mapping
    void publish(const DecodedSignal& signal);

    // Publish directly with known service/method IDs
    void publish_float(uint16_t service_id, uint16_t method_id,
                       float value);

private:
    int         sock_fd_  = -1;
    uint16_t    port_     = 0;
    uint16_t    session_  = 1;   // increments with each message

    struct sockaddr_in dest_addr_{};

    // signal name -> (service_id, method_id)
    std::unordered_map<std::string,
                       std::pair<uint16_t, uint16_t>> signal_map_;

    void send_message(const SomeIpMessage& msg);
};
