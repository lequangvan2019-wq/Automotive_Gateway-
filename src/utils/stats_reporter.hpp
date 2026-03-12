#pragma once
#include "../bench/benchmark.hpp"
#include <string>

class StatsReporter {
public:
    explicit StatsReporter(const BenchmarkCollector& bench)
        : bench_(bench) {}

    // Print a formatted table to stdout
    void print_table() const;

    // Return stats as a JSON string
    std::string to_json() const;

private:
    const BenchmarkCollector& bench_;
};
