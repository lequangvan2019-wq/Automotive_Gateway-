#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <time.h>

uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL
           + static_cast<uint64_t>(ts.tv_nsec);
}

int main(int argc, char* argv[]) {
    // Default: 1000 frames/sec for 10 seconds
    int rate_hz  = (argc > 1) ? atoi(argv[1]) : 1000;
    int duration = (argc > 2) ? atoi(argv[2]) : 10;

    int sock = socket(AF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        std::cerr << "socket failed\n";
        return 1;
    }

    struct ifreq ifr{};
    strncpy(ifr.ifr_name, "vcan0", IFNAMSIZ - 1);
    ioctl(sock, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    uint64_t interval_ns = 1000000000ULL / rate_hz;
    uint64_t total_frames = static_cast<uint64_t>(rate_hz) * duration;

    std::cout << "[STRESS] Sending " << rate_hz << " frames/sec for "
              << duration << " seconds\n";
    std::cout << "[STRESS] Total frames: " << total_frames << "\n";
    std::cout << "[STRESS] Starting...\n";

    uint64_t sent       = 0;
    uint64_t start_time = now_ns();
    uint64_t next_send  = start_time;

    // Cycle through all 3 message types
    uint32_t can_ids[] = {291, 292, 293};

    while (sent < total_frames) {
        uint64_t now = now_ns();
        if (now < next_send) {
            // Busy wait for precise timing
            continue;
        }

        can_frame frame{};
        frame.can_id  = can_ids[sent % 3];
        frame.can_dlc = 8;
        // Random-ish data
        for (int i = 0; i < 8; i++)
            frame.data[i] = static_cast<uint8_t>((sent + i) & 0xFF);

        write(sock, &frame, sizeof(frame));
        sent++;
        next_send += interval_ns;

        if (sent % 1000 == 0) {
            double elapsed = (now_ns() - start_time) / 1e9;
            double actual_rate = sent / elapsed;
            std::cout << "[STRESS] Sent " << sent
                      << " frames, actual rate: "
                      << static_cast<int>(actual_rate) << " fps\n";
        }
    }

    double total_time = (now_ns() - start_time) / 1e9;
    std::cout << "[STRESS] Done. Sent " << sent
              << " frames in " << total_time << "s\n";
    std::cout << "[STRESS] Average rate: "
              << static_cast<int>(sent / total_time) << " fps\n";

    close(sock);
    return 0;
}
