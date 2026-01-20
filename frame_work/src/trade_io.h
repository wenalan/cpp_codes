#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "types.h"

namespace hft {

class TradeIo {
 public:
  TradeIo(std::atomic<bool>& running, std::shared_ptr<OrderQueue> inbound, std::shared_ptr<LogQueue> log_queue);

  void run();

 private:
  static std::string fmt(double v);
  void log(const std::string& msg);

  std::atomic<bool>& running_;
  std::shared_ptr<OrderQueue> inbound_;
  std::shared_ptr<LogQueue> log_queue_;
};

}  // namespace hft
