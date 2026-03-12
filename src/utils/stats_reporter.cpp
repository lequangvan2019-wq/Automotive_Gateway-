#include "stats_reporter.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

void StatsReporter::print_table() const {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════"
              << "══════════════════════╗\n";
    std::cout << "║           AUTOMOTIVE GATEWAY LATENCY REPORT"
              << "                      ║\n";
    std::cout << "╠══════════════════════════════════════════════"
              << "══════════════════════╣\n";
    std::cout << "║ " << std::left << std::setw(20) << "Event"
              << std::right
              << std::setw(8)  << "Count"
              << std::setw(8)  << "Min"
              << std::setw(8)  << "Mean"
              << std::setw(8)  << "P50"
              << std::setw(8)  << "P95"
              << std::setw(8)  << "P99"
              << std::setw(8)  << "Max"
              << "  ║\n";
    std::cout << "║ " << std::left << std::setw(20) << ""
              << std::right
              << std::setw(8)  << ""
              << std::setw(8)  << "us"
              << std::setw(8)  << "us"
              << std::setw(8)  << "us"
              << std::setw(8)  << "us"
              << std::setw(8)  << "us"
              << std::setw(8)  << "us"
              << "  ║\n";
    std::cout << "╠══════════════════════════════════════════════"
              << "══════════════════════╣\n";

    for (const auto& name : bench_.event_names()) {
        const auto* h = bench_.get(name);
        if (!h || h->count() == 0) continue;

        std::cout << "║ " << std::left  << std::setw(20) << name
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(8)  << h->count()
                  << std::setw(8)  << h->min()
                  << std::setw(8)  << h->mean()
                  << std::setw(8)  << h->p50()
                  << std::setw(8)  << h->p95()
                  << std::setw(8)  << h->p99()
                  << std::setw(8)  << h->max()
                  << "  ║\n";
    }

    std::cout << "╠══════════════════════════════════════════════"
              << "══════════════════════╣\n";
    std::cout << "║ Total messages processed: "
              << std::left << std::setw(43)
              << bench_.total_messages() << "║\n";
    std::cout << "╚══════════════════════════════════════════════"
              << "══════════════════════╝\n\n";
}

std::string StatsReporter::to_json() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"total_messages\": " << bench_.total_messages() << ",\n";
    oss << "  \"latency\": {\n";

    auto names = bench_.event_names();
    for (size_t i = 0; i < names.size(); i++) {
        const auto* h = bench_.get(names[i]);
        if (!h) continue;
        oss << "    \"" << names[i] << "\": {\n";
        oss << "      \"count\": " << h->count()       << ",\n";
        oss << "      \"min_us\": "  << h->min()       << ",\n";
        oss << "      \"mean_us\": " << h->mean()      << ",\n";
        oss << "      \"p50_us\": "  << h->p50()       << ",\n";
        oss << "      \"p95_us\": "  << h->p95()       << ",\n";
        oss << "      \"p99_us\": "  << h->p99()       << ",\n";
        oss << "      \"max_us\": "  << h->max()       << "\n";
        oss << "    }";
        if (i < names.size() - 1) oss << ",";
        oss << "\n";
    }

    oss << "  }\n";
    oss << "}\n";
    return oss.str();
}
