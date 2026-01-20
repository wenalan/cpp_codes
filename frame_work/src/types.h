#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "spsc_queue.h"

namespace hft {

struct LogEvent {
  std::string source;
  std::string message;
  std::chrono::steady_clock::time_point ts;
};

struct MarketEvent {
  std::string symbol;
  double bid{};
  double ask{};
  double size{};
  std::int64_t seq{};
};

struct StrategyDecision {
  std::string symbol;
  bool buy{};
  double price{};
  double qty{};
  std::int64_t seq{}; // same seq num as in MarketEvent?
};

struct OrderCommand { // same struct as above? merge?
  std::string symbol;
  bool buy{};
  double price{};
  double qty{};
  std::int64_t seq{};
};

using LogQueue = SpscQueue<LogEvent, 1024>;
using MarketQueue = SpscQueue<MarketEvent, 1024>;
using StrategyQueue = SpscQueue<StrategyDecision, 1024>;
using OrderQueue = SpscQueue<OrderCommand, 1024>;

}  // namespace hft
