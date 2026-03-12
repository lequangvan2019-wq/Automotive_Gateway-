#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <time.h>

// Get current time in nanoseconds (monotonic clock)
inline uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL
           + static_cast<uint64_t>(ts.tv_nsec);
}

// A timer that measures elapsed time between start() and stop()
struct Timer {
    uint64_t start_ns = 0;

    void start() { start_ns = now_ns(); }

    uint64_t elapsed_us() const {
        return (now_ns() - start_ns) / 1000;
    }

    uint64_t elapsed_ns() const {
        return now_ns() - start_ns;
    }
};

// Latency histogram — tracks min/max/mean/p50/p95/p99
// Fixed size — no heap allocation after construction
class LatencyHistogram {
public:
    explicit LatencyHistogram(size_t max_samples = 10000)
        : max_samples_(max_samples)
    {
        samples_.reserve(max_samples);
    }

    void record(uint64_t latency_us) {
        if (samples_.size() < max_samples_)
            samples_.push_back(latency_us);
        total_count_++;
    }

    uint64_t min()  const {
        if (samples_.empty()) return 0;
        return *std::min_element(samples_.begin(), samples_.end());
    }

    uint64_t max()  const {
        if (samples_.empty()) return 0;
        return *std::max_element(samples_.begin(), samples_.end());
    }

    double mean() const {
        if (samples_.empty()) return 0;
        uint64_t sum = 0;
        for (auto s : samples_) sum += s;
        return static_cast<double>(sum) / samples_.size();
    }

    uint64_t percentile(double p) const {
        if (samples_.empty()) return 0;
        std::vector<uint64_t> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(p / 100.0 * sorted.size());
        if (idx >= sorted.size()) idx = sorted.size() - 1;
        return sorted[idx];
    }

    uint64_t p50() const { return percentile(50); }
    uint64_t p95() const { return percentile(95); }
    uint64_t p99() const { return percentile(99); }

    size_t count()       const { return total_count_; }
    size_t sample_count()const { return samples_.size(); }

    void reset() {
        samples_.clear();
        total_count_ = 0;
    }

private:
    std::vector<uint64_t> samples_;
    size_t max_samples_  = 0;
    size_t total_count_  = 0;
};

// Central benchmark collector — one histogram per event type
class BenchmarkCollector {
public:
    // Record a latency sample for a named event
    void record(const std::string& event, uint64_t latency_us) {
        histograms_[event].record(latency_us);
        total_messages_++;
    }

    // Get histogram for a named event
    const LatencyHistogram* get(const std::string& event) const {
        auto it = histograms_.find(event);
        if (it == histograms_.end()) return nullptr;
        return &it->second;
    }

    // All event names
    std::vector<std::string> event_names() const {
        std::vector<std::string> names;
        for (const auto& [k, v] : histograms_)
            names.push_back(k);
        return names;
    }

    size_t total_messages() const { return total_messages_; }

    void reset() {
        for (auto& [k, v] : histograms_) v.reset();
        total_messages_ = 0;
    }

private:
    std::unordered_map<std::string, LatencyHistogram> histograms_;
    size_t total_messages_ = 0;
};
