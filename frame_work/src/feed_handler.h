#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.h"

namespace hft {

class FeedHandler {
 public:
  FeedHandler(std::atomic<bool>& running,
              std::vector<std::string> symbols,
              std::vector<std::shared_ptr<MarketQueue>> shard_queues,
              std::shared_ptr<LogQueue> log_queue);

  void run();

 private:
  std::size_t shard_for_symbol(const std::string& sym) const;
  void log(const std::string& msg);

  std::atomic<bool>& running_;
  std::vector<std::string> symbols_;
  std::vector<std::shared_ptr<MarketQueue>> shard_queues_;
  std::shared_ptr<LogQueue> log_queue_;
};

}  // namespace hft
