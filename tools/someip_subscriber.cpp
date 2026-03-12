#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

constexpr auto     MULTICAST_ADDR = "239.0.0.1";
constexpr uint16_t SOMEIP_PORT    = 30490;
constexpr size_t   SOMEIP_HDR     = 16;

// Decode the SOME/IP header and print it
void print_someip(const uint8_t* buf, size_t len) {
    if (len < SOMEIP_HDR) {
        std::cout << "[SUB] Packet too short: " << len << " bytes\n";
        return;
    }

    uint16_t service_id = (buf[0]  << 8) | buf[1];
    uint16_t method_id  = (buf[2]  << 8) | buf[3];
    uint32_t length     = (buf[4]  << 24) | (buf[5] << 16)
                        | (buf[6]  <<  8) |  buf[7];
    uint16_t client_id  = (buf[8]  << 8) | buf[9];
    uint16_t session_id = (buf[10] << 8) | buf[11];
    uint8_t  proto_ver  =  buf[12];
    uint8_t  iface_ver  =  buf[13];
    uint8_t  msg_type   =  buf[14];
    uint8_t  ret_code   =  buf[15];

    std::cout << "\n[SOME/IP Message]\n";
    std::cout << "  Service ID:  0x" << std::hex << service_id << "\n";
    std::cout << "  Method ID:   0x" << method_id << "\n";
    std::cout << "  Length:      "   << std::dec << length << " bytes\n";
    std::cout << "  Client ID:   0x" << std::hex << client_id << "\n";
    std::cout << "  Session ID:  "   << std::dec << session_id << "\n";
    std::cout << "  Proto Ver:   "   << (int)proto_ver << "\n";
    std::cout << "  Iface Ver:   "   << (int)iface_ver << "\n";
    std::cout << "  Msg Type:    0x" << std::hex << (int)msg_type;

    if (msg_type == 0x02) std::cout << " (NOTIFICATION)";
    std::cout << "\n";
    std::cout << "  Return Code: 0x" << (int)ret_code << "\n";

    // Decode payload as float (4 bytes big-endian)
    size_t payload_len = len - SOMEIP_HDR;
    if (payload_len == 4) {
        uint32_t raw = (buf[16] << 24) | (buf[17] << 16)
                     | (buf[18] <<  8) |  buf[19];
        float value;
        memcpy(&value, &raw, sizeof(float));
        // raw is in network byte order, convert
        uint32_t host_raw = ntohl(raw);
        memcpy(&value, &host_raw, sizeof(float));
        std::cout << std::dec << std::fixed << std::setprecision(2);
        std::cout << "  Payload:     " << value << " (float)\n";
    } else {
        std::cout << std::dec;
        std::cout << "  Payload:     " << payload_len << " bytes\n";
    }
}

int main() {
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "socket failed: " << strerror(errno) << "\n";
        return 1;
    }

    // Allow multiple processes to bind to same port
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Bind to the SOME/IP port
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SOMEIP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind failed: " << strerror(errno) << "\n";
        close(sock);
        return 1;
    }

    // Join the multicast group
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
    mreq.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) < 0) {
        std::cerr << "IP_ADD_MEMBERSHIP failed: " << strerror(errno) << "\n";
        close(sock);
        return 1;
    }

    std::cout << "[SUB] Listening for SOME/IP on "
              << MULTICAST_ADDR << ":" << SOMEIP_PORT << "\n";
    std::cout << "[SUB] Waiting for messages...\n";

    uint8_t buf[1500];
    while (true) {
        struct sockaddr_in sender{};
        socklen_t sender_len = sizeof(sender);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&sender),
                             &sender_len);
        if (n < 0) {
            std::cerr << "[SUB] recvfrom failed: " << strerror(errno) << "\n";
            break;
        }

        std::cout << "[SUB] Received " << n << " bytes from "
                  << inet_ntoa(sender.sin_addr) << "\n";
        print_someip(buf, static_cast<size_t>(n));
    }

    close(sock);
    return 0;
}
