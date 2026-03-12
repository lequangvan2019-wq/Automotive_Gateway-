#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

constexpr uint16_t DOIP_PORT          = 13400;
constexpr uint8_t  DOIP_PROTO_VER     = 0x02;
constexpr uint8_t  DOIP_PROTO_VER_INV = 0xFD;
constexpr uint16_t TESTER_ADDR        = 0x0E00;
constexpr uint16_t ECU_ADDR           = 0x0001;

std::vector<uint8_t> make_msg(uint16_t type,
                               const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> msg(8 + payload.size());
    msg[0] = DOIP_PROTO_VER;
    msg[1] = DOIP_PROTO_VER_INV;
    msg[2] = (type >> 8) & 0xFF;
    msg[3] =  type       & 0xFF;
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    memcpy(msg.data() + 4, &len, 4);
    memcpy(msg.data() + 8, payload.data(), payload.size());
    return msg;
}

void send_uds(int sock, const std::vector<uint8_t>& uds_req,
              const std::string& desc)
{
    std::cout << "=== " << desc << " ===\n";

    std::vector<uint8_t> diag_payload = {
        (TESTER_ADDR >> 8) & 0xFF,
         TESTER_ADDR       & 0xFF,
        (ECU_ADDR >> 8)    & 0xFF,
         ECU_ADDR          & 0xFF
    };
    diag_payload.insert(diag_payload.end(),
                        uds_req.begin(), uds_req.end());

    auto diag_msg = make_msg(0x8001, diag_payload);
    send(sock, diag_msg.data(), diag_msg.size(), 0);

    uint8_t buf[4096];
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) return;

    std::cout << "Raw [" << n << " bytes]: ";
    for (int i = 0; i < n; i++)
        std::cout << std::hex << std::setw(2)
                  << std::setfill('0') << (int)buf[i] << " ";
    std::cout << std::dec << "\n";

    if (n > 8) {
        uint32_t second_len;
        memcpy(&second_len, buf + 4, 4);
        second_len = ntohl(second_len);
        if (n >= static_cast<ssize_t>(8 + second_len) &&
            second_len >= 5) {
            uint8_t* uds_resp = buf + 8 + 4;
            size_t   uds_len  = second_len - 4;
            std::cout << "UDS Response: ";
            for (size_t i = 0; i < uds_len; i++)
                std::cout << std::hex << std::setw(2)
                          << std::setfill('0')
                          << (int)uds_resp[i] << " ";
            std::cout << std::dec << "\n";
        }
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    if (argc > 1) server_ip = argv[1];

    std::cout << "[CLIENT] Connecting to "
              << server_ip << ":" << DOIP_PORT << "\n";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { std::cerr << "socket failed\n"; return 1; }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(DOIP_PORT);
    addr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if (connect(sock,
                reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[CLIENT] Connection failed: "
                  << strerror(errno) << "\n";
        close(sock);
        return 1;
    }
    std::cout << "[CLIENT] Connected\n\n";

    // Step 1: Routing Activation
    std::cout << "=== Step 1: Routing Activation ===\n";
    std::vector<uint8_t> routing_req = {
        (TESTER_ADDR >> 8) & 0xFF,
         TESTER_ADDR       & 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00
    };
    auto routing_msg = make_msg(0x0005, routing_req);
    send(sock, routing_msg.data(), routing_msg.size(), 0);

    uint8_t buf[4096];
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) { close(sock); return 1; }

    std::cout << "Raw [" << n << " bytes]: ";
    for (int i = 0; i < n; i++)
        std::cout << std::hex << std::setw(2)
                  << std::setfill('0') << (int)buf[i] << " ";
    std::cout << std::dec << "\n";

    if (n >= 13 && buf[12] == 0x10)
        std::cout << "[CLIENT] Routing activation SUCCESS\n\n";
    else {
        std::cerr << "[CLIENT] Routing activation FAILED\n";
        close(sock);
        return 1;
    }

    // UDS commands
    send_uds(sock, {0x10, 0x03}, "UDS Session Control (Extended)");
    send_uds(sock, {0x19, 0x02, 0xFF}, "UDS Read DTC");
    send_uds(sock, {0x22, 0xF1, 0x90}, "UDS Read VIN");
    send_uds(sock, {0x22, 0xF1, 0x8C}, "UDS Read ECU Serial");
    send_uds(sock, {0x22, 0xF1, 0x87}, "UDS Read SW Version");
    send_uds(sock, {0x11, 0x01}, "UDS ECU Reset");

    std::cout << "[CLIENT] Done\n";
    close(sock);
    return 0;
}
