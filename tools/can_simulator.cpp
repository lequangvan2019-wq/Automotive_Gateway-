#include <iostream>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

// Pack a little-endian uint16 into CAN data bytes
void pack_u16(uint8_t* data, int byte_offset, uint16_t value) {
    data[byte_offset]     = value & 0xFF;
    data[byte_offset + 1] = (value >> 8) & 0xFF;
}

int main() {
    // Open CAN socket
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

    std::cout << "[SIM] CAN simulator running on vcan0\n";
    std::cout << "[SIM] Sending engine, speed, brake data every 100ms\n";
    std::cout << "[SIM] Press Ctrl+C to stop\n";

    double t = 0.0;
    while (true) {
        // --- Engine Data (ID 291 = 0x123) ---
        // RPM: 800 idle + sine wave up to 4000
        double rpm    = 2400.0 + 1600.0 * sin(t * 0.5);
        double temp   = 85.0 + 5.0 * sin(t * 0.1);   // 80-90 degC
        double throttle = 20.0 + 15.0 * sin(t * 0.3); // 5-35%

        can_frame engine_frame{};
        engine_frame.can_id  = 291;
        engine_frame.can_dlc = 8;
        pack_u16(engine_frame.data, 0,
                 static_cast<uint16_t>(rpm / 0.25));
        engine_frame.data[2] = static_cast<uint8_t>((temp + 40) / 0.75);
        engine_frame.data[3] = static_cast<uint8_t>(throttle / 0.392157);
        write(sock, &engine_frame, sizeof(engine_frame));

        // --- Vehicle Speed (ID 292 = 0x124) ---
        double speed = 60.0 + 20.0 * sin(t * 0.2);  // 40-80 km/h
        can_frame speed_frame{};
        speed_frame.can_id  = 292;
        speed_frame.can_dlc = 8;
        pack_u16(speed_frame.data, 0,
                 static_cast<uint16_t>(speed / 0.01));
        pack_u16(speed_frame.data, 2,
                 static_cast<uint16_t>((speed + 1.0) / 0.01));
        pack_u16(speed_frame.data, 4,
                 static_cast<uint16_t>((speed - 1.0) / 0.01));
        write(sock, &speed_frame, sizeof(speed_frame));

        // --- Brake Data (ID 293 = 0x125) ---
        bool braking         = (sin(t * 0.15) < -0.5);
        double brake_pressure = braking ? 40.0 : 0.0;
        can_frame brake_frame{};
        brake_frame.can_id  = 293;
        brake_frame.can_dlc = 8;
        brake_frame.data[0] = static_cast<uint8_t>(brake_pressure);
        brake_frame.data[1] = braking ? 1 : 0;
        write(sock, &brake_frame, sizeof(brake_frame));

        std::cout << "[SIM] RPM=" << static_cast<int>(rpm)
                  << " Speed=" << static_cast<int>(speed)
                  << " Brake=" << (braking ? "ON" : "off") << "\n";

        t += 0.1;
        usleep(100000);  // 100ms = 10 Hz
    }

    close(sock);
    return 0;
}
