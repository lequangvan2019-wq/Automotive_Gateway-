#pragma once
#include "doip_message.hpp"
#include "uds_handler.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <errno.h>

class EventLoop;

// One active DoIP TCP connection
struct DoIpConnection {
    int      fd            = -1;
    bool     activated     = false;  // routing activation done
    uint16_t tester_addr   = 0;      // logical address of tester
    uint8_t  recv_buf[4096] = {};
    size_t   recv_len      = 0;
};

class DoIpServer {
public:
    // port: 13400 is the IANA assigned DoIP port
    DoIpServer(uint16_t port = 13400);
    ~DoIpServer();

    void register_with_loop(EventLoop& loop);

private:
    int         listen_fd_ = -1;
    uint16_t    port_      = 13400;
    UdsHandler  uds_;

    std::unordered_map<int, DoIpConnection> connections_;

    void on_new_connection();
    void on_client_data(int client_fd);
    void handle_doip_message(DoIpConnection& conn,
                              const DoIpHeader& hdr,
                              const uint8_t* payload,
                              size_t payload_len);

    void handle_routing_activation(DoIpConnection& conn,
                                    const uint8_t* payload,
                                    size_t len);

    void handle_diagnostic_message(DoIpConnection& conn,
                                    const uint8_t* payload,
                                    size_t len);

    void send_to_client(int fd, const std::vector<uint8_t>& data);
};
