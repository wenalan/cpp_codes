#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "types.h"

namespace hft {

class OmsRisk {
 public:
  OmsRisk(std::atomic<bool>& running,
          std::vector<std::shared_ptr<StrategyQueue>> strategy_inputs,
          std::shared_ptr<OrderQueue> outbound,
          std::shared_ptr<LogQueue> log_queue);

  void run();

 private:
  bool risk_passes(const StrategyDecision& d) const;
  void log(const std::string& msg);

  std::atomic<bool>& running_;
  std::vector<std::shared_ptr<StrategyQueue>> strategy_inputs_;
  std::shared_ptr<OrderQueue> outbound_;
  std::shared_ptr<LogQueue> log_queue_;
};

}  // namespace hft
