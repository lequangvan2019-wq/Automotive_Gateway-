#pragma once
#include <linux/can.h>
#include <iostream>
#include <iomanip>
#include <sstream>

// Print a CAN frame in the standard candump format:
// vcan0  123   [8]  DE AD BE EF 00 00 00 00
inline void print_can_frame(const std::string& iface, const can_frame& f) {
    std::ostringstream oss;
    oss << iface << "  "
        << std::hex << std::uppercase
        << std::setw(3) << std::setfill('0') << (f.can_id & CAN_EFF_MASK)
        << "   [" << std::dec << (int)f.can_dlc << "]  ";

    for (int i = 0; i < f.can_dlc; i++) {
        oss << std::hex << std::uppercase
            << std::setw(2) << std::setfill('0')
            << (int)f.data[i];
        if (i < f.can_dlc - 1) oss << " ";
    }
    std::cout << oss.str() << "\n";
}
