#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "order_book.h"
#include "types.h"

namespace hft {

class BinanceDepthConnector {
 public:
  BinanceDepthConnector(std::atomic<bool>& running,
                        std::string symbol,
                        std::shared_ptr<MarketQueue> outbound,
                        std::shared_ptr<LogQueue> log_queue);

  void run();

 private:
  void log(const std::string& msg);
  void handle_message(const std::string& payload);

  std::atomic<bool>& running_;
  std::string symbol_;
  std::shared_ptr<MarketQueue> outbound_;
  std::shared_ptr<LogQueue> log_queue_;
  OrderBook book_;
};

}  // namespace hft
