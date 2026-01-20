#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "types.h"

namespace hft {

class StrategyShard {
 public:
  StrategyShard(std::atomic<bool>& running,
                std::string name,
                std::shared_ptr<MarketQueue> inbound,
                std::shared_ptr<StrategyQueue> outbound,
                std::shared_ptr<LogQueue> log_queue);

  void run();

 private:
  void log(const std::string& msg);

  std::atomic<bool>& running_;
  std::string name_;
  std::shared_ptr<MarketQueue> inbound_;
  std::shared_ptr<StrategyQueue> outbound_;
  std::shared_ptr<LogQueue> log_queue_;
};

}  // namespace hft
