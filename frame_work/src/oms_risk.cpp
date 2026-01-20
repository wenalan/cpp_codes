#include "oms_risk.h"

#include <chrono>
#include <thread>

namespace hft {

OmsRisk::OmsRisk(std::atomic<bool>& running,
                 std::vector<std::shared_ptr<StrategyQueue>> strategy_inputs,
                 std::shared_ptr<OrderQueue> outbound,
                 std::shared_ptr<LogQueue> log_queue)
    : running_(running),
      strategy_inputs_(std::move(strategy_inputs)),
      outbound_(std::move(outbound)),
      log_queue_(std::move(log_queue)) {}

void OmsRisk::run() {
  while (running_.load(std::memory_order_acquire)) {
    bool progressed = false;
    for (auto& q : strategy_inputs_) {
      StrategyDecision d;
      if (!q->pop(d)) {
        continue;
      }
      progressed = true;
      if (!risk_passes(d)) {
        log("risk reject " + d.symbol + " seq=" + std::to_string(d.seq));
        continue;
      }
      OrderCommand cmd{d.symbol, d.buy, d.price, d.qty, d.seq};
      if (!outbound_->push(cmd)) {
        log("drop order seq=" + std::to_string(d.seq));
      }
    }
    if (!progressed) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  }
}

bool OmsRisk::risk_passes(const StrategyDecision& d) const {
  return d.qty <= 1.5;
}

void OmsRisk::log(const std::string& msg) {
  log_queue_->push(LogEvent{"oms", msg, std::chrono::steady_clock::now()});
}

}  // namespace hft
