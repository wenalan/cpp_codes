#include "strategy_shard.h"

#include <chrono>
#include <thread>

namespace hft {

StrategyShard::StrategyShard(std::atomic<bool>& running,
                             std::string name,
                             std::shared_ptr<MarketQueue> inbound,
                             std::shared_ptr<StrategyQueue> outbound,
                             std::shared_ptr<LogQueue> log_queue)
    : running_(running),
      name_(std::move(name)),
      inbound_(std::move(inbound)),
      outbound_(std::move(outbound)),
      log_queue_(std::move(log_queue)) {}

void StrategyShard::run() {
  while (running_.load(std::memory_order_acquire)) {
    MarketEvent evt;
    if (!inbound_->pop(evt)) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      continue;
    }
    const double mid = (evt.bid + evt.ask) * 0.5;
    const bool buy = (evt.seq % 2) == 0;
    const double px = buy ? mid - 0.05 : mid + 0.05;
    StrategyDecision decision{evt.symbol, buy, px, evt.size * 0.8, evt.seq};
    if (!outbound_->push(decision)) {
      log("drop decision seq=" + std::to_string(evt.seq));
    }
  }
}

void StrategyShard::log(const std::string& msg) {
  log_queue_->push(LogEvent{name_, msg, std::chrono::steady_clock::now()});
}

}  // namespace hft
