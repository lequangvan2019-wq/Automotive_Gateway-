#include "doip_server.hpp"
#include "../event/event_loop.hpp"

DoIpServer::DoIpServer(uint16_t port) : port_(port) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
        throw std::runtime_error(
            "DoIP socket failed: " + std::string(strerror(errno)));

    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd_,
             reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(
            "DoIP bind failed: " + std::string(strerror(errno)));

    if (listen(listen_fd_, 10) < 0)
        throw std::runtime_error(
            "DoIP listen failed: " + std::string(strerror(errno)));

    std::cout << "[DOIP] Server listening on TCP port " << port << "\n";
}

DoIpServer::~DoIpServer() {
    for (auto& [fd, conn] : connections_)
        if (fd >= 0) close(fd);
    if (listen_fd_ >= 0) close(listen_fd_);
}

void DoIpServer::register_with_loop(EventLoop& loop) {
    loop.add_reader(listen_fd_, [this, &loop]() {
        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);

        int client_fd = accept(listen_fd_,
                               reinterpret_cast<sockaddr*>(&client_addr),
                               &client_len);
        if (client_fd < 0) return;

        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

        std::cout << "[DOIP] New connection from "
                  << inet_ntoa(client_addr.sin_addr) << "\n";

        DoIpConnection conn;
        conn.fd        = client_fd;
        conn.activated = false;
        connections_[client_fd] = conn;

        loop.add_reader(client_fd, [this, client_fd]() {
            on_client_data(client_fd);
        });
    });
}

void DoIpServer::on_client_data(int client_fd) {
    auto it = connections_.find(client_fd);
    if (it == connections_.end()) return;

    DoIpConnection& conn = it->second;

    ssize_t n = recv(client_fd,
                     conn.recv_buf + conn.recv_len,
                     sizeof(conn.recv_buf) - conn.recv_len, 0);

    if (n <= 0) {
        std::cout << "[DOIP] Connection closed\n";
        close(client_fd);
        connections_.erase(it);
        return;
    }

    conn.recv_len += static_cast<size_t>(n);

    while (conn.recv_len >= DOIP_HEADER_SIZE) {
        DoIpHeader hdr;
        DoIpHeader::parse(conn.recv_buf, conn.recv_len, hdr);

        size_t total = DOIP_HEADER_SIZE + hdr.payload_length;
        if (conn.recv_len < total) break;

        handle_doip_message(conn,
                             hdr,
                             conn.recv_buf + DOIP_HEADER_SIZE,
                             hdr.payload_length);

        conn.recv_len -= total;
        if (conn.recv_len > 0)
            memmove(conn.recv_buf,
                    conn.recv_buf + total,
                    conn.recv_len);
    }
}

void DoIpServer::handle_doip_message(DoIpConnection& conn,
                                      const DoIpHeader& hdr,
                                      const uint8_t* payload,
                                      size_t payload_len)
{
    std::cout << "[DOIP] Message type=0x"
              << std::hex << hdr.payload_type << std::dec
              << " len=" << payload_len << "\n";

    switch (hdr.payload_type) {
    case DOIP_ROUTING_ACTIVATION_REQ:
        handle_routing_activation(conn, payload, payload_len);
        break;
    case DOIP_DIAGNOSTIC_MESSAGE:
        handle_diagnostic_message(conn, payload, payload_len);
        break;
    case DOIP_ALIVE_CHECK_REQ: {
        uint8_t res[2];
        res[0] = (DOIP_ECU_ADDRESS >> 8) & 0xFF;
        res[1] =  DOIP_ECU_ADDRESS       & 0xFF;
        auto msg = make_doip_message(DOIP_ALIVE_CHECK_RES, res, 2);
        send_to_client(conn.fd, msg);
        break;
    }
    default:
        std::cout << "[DOIP] Unknown payload type 0x"
                  << std::hex << hdr.payload_type << std::dec << "\n";
        break;
    }
}

void DoIpServer::handle_routing_activation(DoIpConnection& conn,
                                            const uint8_t* payload,
                                            size_t len)
{
    if (len < 2) return;

    uint16_t tester_addr = (payload[0] << 8) | payload[1];
    conn.tester_addr = tester_addr;
    conn.activated   = true;

    std::cout << "[DOIP] Routing activation from tester 0x"
              << std::hex << tester_addr << std::dec << "\n";

    uint8_t res[9] = {};
    res[0] = (tester_addr      >> 8) & 0xFF;
    res[1] =  tester_addr            & 0xFF;
    res[2] = (DOIP_ECU_ADDRESS >> 8) & 0xFF;
    res[3] =  DOIP_ECU_ADDRESS       & 0xFF;
    res[4] = DOIP_ROUTING_OK;
    res[5] = res[6] = res[7] = res[8] = 0x00;

    auto msg = make_doip_message(DOIP_ROUTING_ACTIVATION_RES, res, 9);
    send_to_client(conn.fd, msg);

    std::cout << "[DOIP] Routing activated — ready for UDS\n";
}

void DoIpServer::handle_diagnostic_message(DoIpConnection& conn,
                                            const uint8_t* payload,
                                            size_t len)
{
    if (!conn.activated) {
        std::cout << "[DOIP] Not activated — rejecting\n";
        return;
    }

    if (len < 4) return;

    uint16_t src_addr = (payload[0] << 8) | payload[1];
    uint16_t tgt_addr = (payload[2] << 8) | payload[3];

    std::cout << "[DOIP] Diagnostic message src=0x"
              << std::hex << src_addr
              << " tgt=0x" << tgt_addr << std::dec << "\n";

    const uint8_t* uds_data = payload + 4;
    size_t         uds_len  = len - 4;

    // Send ACK
    uint8_t ack[5] = {};
    ack[0] = (src_addr >> 8) & 0xFF;
    ack[1] =  src_addr       & 0xFF;
    ack[2] = (tgt_addr >> 8) & 0xFF;
    ack[3] =  tgt_addr       & 0xFF;
    ack[4] = DOIP_ACK_OK;

    auto ack_msg = make_doip_message(DOIP_DIAGNOSTIC_ACK, ack, 5);
    send_to_client(conn.fd, ack_msg);

    // Process UDS and send response
    auto uds_response = uds_.handle(uds_data, uds_len);

    std::vector<uint8_t> diag_payload;
    diag_payload.push_back((tgt_addr >> 8) & 0xFF);
    diag_payload.push_back( tgt_addr       & 0xFF);
    diag_payload.push_back((src_addr >> 8) & 0xFF);
    diag_payload.push_back( src_addr       & 0xFF);
    diag_payload.insert(diag_payload.end(),
                        uds_response.begin(),
                        uds_response.end());

    auto diag_msg = make_doip_message(DOIP_DIAGNOSTIC_MESSAGE,
                                       diag_payload.data(),
                                       diag_payload.size());
    send_to_client(conn.fd, diag_msg);
}

void DoIpServer::send_to_client(int fd,
                                 const std::vector<uint8_t>& data)
{
    ssize_t sent = send(fd, data.data(), data.size(), 0);
    if (sent < 0)
        std::cerr << "[DOIP] send failed: " << strerror(errno) << "\n";
}
