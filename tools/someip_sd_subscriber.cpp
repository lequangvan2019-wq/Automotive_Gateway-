#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

constexpr auto     SD_MULTICAST  = "224.224.224.245";
constexpr auto     DATA_MULTICAST= "239.0.0.1";
constexpr uint16_t SD_PORT       = 30490;
constexpr uint16_t DATA_PORT     = 30490;

// Send a SubscribeEventgroup to the gateway
void send_subscribe(int sock, const sockaddr_in& gateway_sd,
                    uint16_t service_id, uint16_t eventgroup_id)
{
    uint8_t buf[44] = {};

    // SOME/IP header
    buf[0] = 0xFF; buf[1] = 0xFF;  // Service ID
    buf[2] = 0x81; buf[3] = 0x00;  // Method ID (SD)
    uint32_t len = htonl(36);
    memcpy(buf + 4, &len, 4);
    buf[8]  = 0x00; buf[9]  = 0x00;
    buf[10] = 0x00; buf[11] = 0x01;  // session
    buf[12] = 0x01;  // proto ver
    buf[13] = 0x01;  // iface ver
    buf[14] = 0x02;  // notification
    buf[15] = 0x00;  // return code OK

    // SD flags
    buf[16] = 0xC0;
    buf[17] = 0x00;
    buf[18] = 0x00;
    buf[19] = 0x00;

    // Entries array length = 16
    uint32_t elen = htonl(16);
    memcpy(buf + 20, &elen, 4);

    // SubscribeEventgroup entry
    buf[24] = 0x06;  // type = subscribe eventgroup
    buf[25] = 0x00;
    buf[26] = 0x00;
    buf[27] = 0x00;
    uint16_t svc = htons(service_id);
    memcpy(buf + 28, &svc, 2);
    uint16_t ins = htons(0x0001);
    memcpy(buf + 30, &ins, 2);
    buf[32] = 0x01;  // major version
    buf[33] = 0x00;
    buf[34] = 0x00;
    buf[35] = 0x03;  // TTL = 3
    buf[36] = 0x00;
    buf[37] = 0x00;
    buf[38] = 0x00;
    buf[39] = 0x00;

    // Options array length = 0
    uint32_t olen = htonl(0);
    memcpy(buf + 40, &olen, 4);

    // eventgroup id at bytes 42-43
    uint16_t evg = htons(eventgroup_id);
    memcpy(buf + 42, &evg, 2);

    sendto(sock, buf, sizeof(buf), 0,
           reinterpret_cast<const sockaddr*>(&gateway_sd),
           sizeof(gateway_sd));

    std::cout << "[SUB] Sent SubscribeEventgroup "
              << "Service=0x" << std::hex << service_id
              << " EventGroup=0x" << eventgroup_id
              << std::dec << "\n";
}

int main() {
    int reuse = 1;

    // --- Step 1: Listen for SD OfferService ---
    int sd_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd_sock < 0) {
        std::cerr << "SD socket failed\n";
        return 1;
    }

    setsockopt(sd_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in sd_addr{};
    sd_addr.sin_family      = AF_INET;
    sd_addr.sin_port        = htons(SD_PORT);
    sd_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sd_sock,
             reinterpret_cast<sockaddr*>(&sd_addr), sizeof(sd_addr)) < 0) {
        std::cerr << "SD bind failed\n";
        close(sd_sock);
        return 1;
    }

    // Join SD multicast group
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(SD_MULTICAST);
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(sd_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
               &mreq, sizeof(mreq));

    std::cout << "[SUB] Waiting for OfferService on "
              << SD_MULTICAST << ":" << SD_PORT << "\n";
    std::cout << "[SUB] Make sure gateway is running first\n";

    // Wait for OfferService from gateway
    uint8_t buf[1500];
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);

    ssize_t n = recvfrom(sd_sock, buf, sizeof(buf), 0,
                          reinterpret_cast<sockaddr*>(&from),
                          &from_len);
    if (n < 0) {
        std::cerr << "recvfrom failed\n";
        close(sd_sock);
        return 1;
    }

    std::cout << "[SUB] Received SD OfferService from "
              << inet_ntoa(from.sin_addr) << "\n";

    // Send SubscribeEventgroup for all three services
    send_subscribe(sd_sock, from, 0x0101, 0x0001);
    send_subscribe(sd_sock, from, 0x0102, 0x0001);
    send_subscribe(sd_sock, from, 0x0103, 0x0001);

    close(sd_sock);

    // --- Step 2: Listen for SOME/IP data notifications ---
    int data_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (data_sock < 0) {
        std::cerr << "Data socket failed\n";
        return 1;
    }

    setsockopt(data_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in data_addr{};
    data_addr.sin_family      = AF_INET;
    data_addr.sin_port        = htons(DATA_PORT);
    data_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(data_sock,
             reinterpret_cast<sockaddr*>(&data_addr),
             sizeof(data_addr)) < 0) {
        std::cerr << "Data bind failed\n";
        close(data_sock);
        return 1;
    }

    // Join data multicast group
    struct ip_mreq data_mreq{};
    data_mreq.imr_multiaddr.s_addr = inet_addr(DATA_MULTICAST);
    data_mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(data_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
               &data_mreq, sizeof(data_mreq));

    std::cout << "[SUB] Subscribed successfully\n";
    std::cout << "[SUB] Listening for SOME/IP data on "
              << DATA_MULTICAST << ":" << DATA_PORT << "\n";
    std::cout << "[SUB] Start can_simulator in another terminal\n\n";

    // Receive and print SOME/IP notifications
    while (true) {
        n = recvfrom(data_sock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n < 16) continue;

        uint16_t svc_id = (buf[0] << 8) | buf[1];
        uint16_t met_id = (buf[2] << 8) | buf[3];
        uint16_t ses_id = (buf[10] << 8) | buf[11];
        uint8_t  msg_type = buf[14];

        // Skip SD messages (Service=0xFFFF)
        if (svc_id == 0xFFFF) continue;

        // Skip non-notification messages
        if (msg_type != 0x02) continue;

        // Need at least 4 bytes of payload for a float
        if (n < 20) continue;

        // Decode float payload
        uint32_t raw;
        memcpy(&raw, buf + 16, 4);
        raw = ntohl(raw);
        float value;
        memcpy(&value, &raw, sizeof(float));

        // Map service/method to signal name
        std::string signal_name = "Unknown";
        if      (svc_id == 0x0101 && met_id == 0x8001) signal_name = "EngineRPM";
        else if (svc_id == 0x0101 && met_id == 0x8002) signal_name = "EngineTemp";
        else if (svc_id == 0x0101 && met_id == 0x8003) signal_name = "ThrottlePos";
        else if (svc_id == 0x0102 && met_id == 0x8001) signal_name = "VehicleSpeed";
        else if (svc_id == 0x0102 && met_id == 0x8002) signal_name = "WheelSpeed_FL";
        else if (svc_id == 0x0102 && met_id == 0x8003) signal_name = "WheelSpeed_FR";
        else if (svc_id == 0x0103 && met_id == 0x8001) signal_name = "BrakePressure";
        else if (svc_id == 0x0103 && met_id == 0x8002) signal_name = "BrakeActive";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "[SUB] " << std::left << std::setw(15) << signal_name
                  << " = " << std::right << std::setw(10) << value
                  << "  (Session=" << std::dec << ses_id << ")\n";
    }

    close(data_sock);
    return 0;
}
