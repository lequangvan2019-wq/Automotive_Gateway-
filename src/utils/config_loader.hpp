#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SignalRoute {
    std::string signal_name;
    uint16_t    service_id;
    uint16_t    method_id;
};

class ConfigLoader {
public:
    // Parse gateway.conf — returns false on failure
    bool load(const std::string& filepath);

    const std::vector<SignalRoute>& routes() const { return routes_; }
    size_t route_count() const { return routes_.size(); }

private:
    std::vector<SignalRoute> routes_;
};
