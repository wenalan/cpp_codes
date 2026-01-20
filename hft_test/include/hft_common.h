#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hft {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
constexpr int kSimPort = 9001;

inline int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

enum class Side : uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : uint8_t { Limit = 0 };
enum class TimeInForce : uint8_t { GTC = 0 };
enum class ExecType : uint8_t { Ack = 0, Fill = 1, PartialFill = 2, Reject = 3, Cancel = 4 };

struct LevelDelta {
    Side side{};
    int64_t px = 0;   // fixed decimal with tick applied
    int64_t qty = 0;  // size in base units
};

struct BookDelta {
    uint64_t md_event_id = 0;
    std::string symbol;
    std::vector<LevelDelta> levels;
    uint64_t exch_update_id_begin = 0;
    uint64_t exch_update_id_end = 0;
};

struct OrderRequest {
    uint64_t req_id = 0;
    uint64_t md_event_id = 0;
    Side side{};
    int64_t px = 0;
    int64_t qty = 0;
    OrderType type = OrderType::Limit;
    TimeInForce tif = TimeInForce::GTC;
    double signal_z = 0.0;
};

struct ExecUpdate {
    uint64_t cl_ord_id = 0;
    uint64_t md_event_id = 0;
    ExecType exec_type = ExecType::Ack;
    int64_t fill_px = 0;
    int64_t fill_qty = 0;
    int64_t ts_oms_recv_ns = 0;
};

struct OrderState {
    uint64_t md_event_id = 0;
    Side side{};
    int64_t px = 0;
    int64_t qty = 0;
    ExecType state = ExecType::Ack;
    int64_t filled = 0;
};

struct RollingStats {
    // Welford incremental mean/std
    uint64_t n = 0;
    double mean = 0.0;
    double m2 = 0.0;

    void add(double x) {
        ++n;
        double delta = x - mean;
        mean += delta / static_cast<double>(n);
        double delta2 = x - mean;
        m2 += delta * delta2;
    }

    [[nodiscard]] double stddev() const {
        if (n < 2) return 0.0;
        return std::sqrt(m2 / static_cast<double>(n - 1));
    }
};

class ScopedTimer {
public:
    using Sink = std::function<void(const std::string&, int64_t)>;

    ScopedTimer(const std::string& name, Sink sink)
        : name_(name), sink_(std::move(sink)), begin_(Clock::now()) {}

    ~ScopedTimer() {
        if (sink_) {
            auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin_).count();
            sink_(name_, dur);
        }
    }

private:
    std::string name_;
    Sink sink_;
    TimePoint begin_;
};

class Telemetry {
public:
    void record(const std::string& name, int64_t ns) {
        auto& bucket = buckets_[name];
        bucket.samples.push_back(ns);
    }

    std::string summary() const {
        std::string out;
        for (auto& [name, bucket] : buckets_) {
            if (bucket.samples.empty()) continue;
            auto stats = percentiles(bucket.samples);
            out += name + ": p50=" + std::to_string(stats.p50) + "us p90=" + std::to_string(stats.p90) +
                   "us p99=" + std::to_string(stats.p99) + "us\n";
        }
        return out;
    }

private:
    struct Bucket {
        std::vector<int64_t> samples;
    };

    struct Percentiles {
        double p50 = 0;
        double p90 = 0;
        double p99 = 0;
    };

    static Percentiles percentiles(std::vector<int64_t> v) {
        if (v.empty()) return {};
        std::sort(v.begin(), v.end());
        auto pick = [&](double frac) {
            size_t idx = static_cast<size_t>(frac * (v.size() - 1));
            return static_cast<double>(v[idx]) / 1000.0;  // ns -> us
        };
        return {pick(0.50), pick(0.90), pick(0.99)};
    }

    std::unordered_map<std::string, Bucket> buckets_;
};

}  // namespace hft
