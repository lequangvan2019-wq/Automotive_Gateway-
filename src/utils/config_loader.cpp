#include "config_loader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

bool ConfigLoader::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[CFG] Cannot open: " << filepath << "\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip carriage return
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip blank lines and comments
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;

        std::istringstream ss(line);
        std::string name, svc_str, method_str;
        ss >> name >> svc_str >> method_str;
        if (ss.fail()) continue;

        try {
            SignalRoute route;
            route.signal_name = name;
            route.service_id  = static_cast<uint16_t>(
                                    std::stoul(svc_str, nullptr, 16));
            route.method_id   = static_cast<uint16_t>(
                                    std::stoul(method_str, nullptr, 16));
            routes_.push_back(route);

            std::cout << "[CFG] Route: " << name
                      << " -> Service=0x" << std::hex << route.service_id
                      << " Method=0x"     << route.method_id
                      << std::dec << "\n";
        } catch (...) {
            std::cerr << "[CFG] Bad line: " << line << "\n";
        }
    }

    std::cout << "[CFG] Loaded " << routes_.size() << " routes\n";
    return !routes_.empty();
}
